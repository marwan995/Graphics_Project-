#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

using namespace std;

namespace our
{
    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));
            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram *skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textureWithoutLight.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textureWithoutLight.frag", GL_FRAGMENT_SHADER);
            skyShader->link();

            // TODO: (Req 10) Pick the correct pipeline state to draw the sky
            //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            //  We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;
            skyPipelineState.faceCulling.frontFace = GL_CCW;

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky
            Sampler *skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
        }
        

        // Then we check if there is a postprocessing shader in the configuration
        if (config.contains("postprocess"))
        {
            string filePath = config.value<std::string>("postprocess", "");
            initializePostprocess(filePath);
        }

    }
    
    void ForwardRenderer::initializePostprocess(std::string filePath)
    {
        // TODO: This may cause error, you need to delete the current postprocess material before creating a new one
        // if(postprocessMaterial)
        //     deletePostprocessMatrial();
        // TODO: (Req 11) Create a framebuffer
        glGenFramebuffers(1, &postprocessFrameBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);

        // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
        //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
        //  The depth format can be (Depth component with 24 bits).
        // void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
        colorTarget = texture_utils::empty(GL_RGBA8, windowSize);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);

        depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);
        // TODO: (Req 11) Unbind the framebuffer just to be safe
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        // Create a vertex array to use for drawing the texture
        glGenVertexArrays(1, &postProcessVertexArray);

        // Create a sampler to use for sampling the scene texture in the post processing shader
        Sampler *postprocessSampler = new Sampler();
        postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create the post processing shader
        ShaderProgram *postprocessShader = new ShaderProgram();
        postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
        postprocessShader->attach(filePath, GL_FRAGMENT_SHADER);
        postprocessShader->link();

        // Create a post processing material
        postprocessMaterial = new TexturedMaterial();
        postprocessMaterial->shader = postprocessShader;
        postprocessMaterial->texture = colorTarget;
        postprocessMaterial->sampler = postprocessSampler;
        // The default options are fine but we don't need to interact with the depth buffer
        // so it is more performant to disable the depth mask
        postprocessMaterial->pipelineState.depthMask = false;
    }

    void ForwardRenderer::deletePostprocessMatrial()
    {
        
        glDeleteFramebuffers(1, &postprocessFrameBuffer);
        glDeleteVertexArrays(1, &postProcessVertexArray);
        delete colorTarget;
        delete depthTarget;
        delete postprocessMaterial->sampler;
        delete postprocessMaterial->shader;
        delete postprocessMaterial;
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if (postprocessMaterial)
            deletePostprocessMatrial();
    }

    void ForwardRenderer::render(World *world)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        // RenderCommand bulletCommand;
        opaqueCommands.clear();
        transparentCommands.clear();
        lightSourceEntities.clear();

        for (auto entity : world->getEntities())
        {
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }


            auto lightComp = entity->getComponent<LightingComponent>();
            if (lightComp)
            {
                lightSourceEntities.push_back(entity);
            }
        }
        // If there is no camera, we return (we cannot render without a camera)
        if (camera == nullptr)
            return;
        // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
        glm::mat4 viewMatrix = camera->getViewMatrix();
        glm::vec3 cameraForward = -glm::vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward](const RenderCommand &first, const RenderCommand &second)
                  {
            //TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second". 
            float distanceToFirst = glm::dot(first.center, cameraForward);
            float distanceToSecond = glm::dot(second.center, cameraForward);
    
            return distanceToFirst > distanceToSecond; });

        // TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
        glm::mat4 VP = camera->getProjectionMatrix(windowSize) * viewMatrix;
        // TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
        glViewport(0, 0, windowSize.x, windowSize.y); // ask MO3ED for viewportStart

        // TODO: (Req 9) Set the clear color to black and the clear depth to 1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);

        // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        // If there is a postprocess material, bind the framebuffer
        if (postprocessMaterial)
        {
            // TODO: (Req 11) bind the framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);
        }

        // TODO: (Req 9) Clear the color and depth buffers

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // TODO: (Req 9) Draw all the opaque commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command

        for (const auto &command : opaqueCommands)
        {
            // Set the model-view-projection matrix (transform) uniform for the shader
            command.material->setup(); //  u called the set before the setup what an idiot
            command.material->shader->set("transform", VP * command.localToWorld);
            command.material->shader->set("model", command.localToWorld);
            int i = 0;
            for (const auto &lightEntity : lightSourceEntities)
            {
                LightingComponent *lightCom = lightEntity->getComponent<LightingComponent>();
                if (lightCom)
                {
                    Entity* lightOwner = lightCom->getOwner();

                    if (lightCom->type == lightingType::DIRECTIONAL)
                    {
                        command.material->shader->set("dirLight.direction", lightCom->direction);
                        command.material->shader->set("dirLight.ambient", lightCom->ambient);
                        command.material->shader->set("dirLight.diffuse",  lightCom->diffuse);
                        command.material->shader->set("dirLight.specular", lightCom->specular);
                    }else if (lightCom->type == lightingType::SPOT){
                        command.material->shader->set("spotLight.constant", lightCom->constant);
                        command.material->shader->set("spotLight.linear",  lightCom->linear);
                        command.material->shader->set("spotLight.quadratic", lightCom->quadratic);
                        
                        command.material->shader->set("spotLight.cutOff", glm::cos(glm::radians(25.0f)));
                        command.material->shader->set("spotLight.outerCutOff", glm::cos(glm::radians(35.0f)));
                        

                        command.material->shader->set("spotLight.position", camera->getOwner()->localTransform.position);
                        command.material->shader->set("spotLight.direction", camera->getFrontVector());

                        command.material->shader->set("spotLight.ambient", lightCom->ambient);
                        command.material->shader->set("spotLight.diffuse",  lightCom->diffuse);
                        command.material->shader->set("spotLight.specular", lightCom->specular);

                    }
                    else if (lightCom->type == lightingType::POINT){
                        command.material->shader->set("pointLights["+std::to_string(i)+"].constant", lightCom->constant);
                        command.material->shader->set("pointLights["+std::to_string(i)+"].linear",  lightCom->linear);
                        command.material->shader->set("pointLights["+std::to_string(i)+"].quadratic", lightCom->quadratic);
                        command.material->shader->set("pointLights["+std::to_string(i)+"].position", lightOwner->localTransform.position);
                        command.material->shader->set("pointLights["+std::to_string(i)+"].ambient", lightCom->ambient);
                        command.material->shader->set("pointLights["+std::to_string(i)+"].diffuse",  lightCom->diffuse);
                        command.material->shader->set("pointLights["+std::to_string(i)+"].specular", lightCom->specular);
                        i++;
                    }

                    command.material->shader->set("camPos", camera->getOwner()->localTransform.position);
                }
            }
            // Draw the mesh using the material's shader
            command.mesh->draw();
        }

        // If there is a sky material, draw the sky
        if (this->skyMaterial)
        {
            // TODO: (Req 10) setup the sky material
            skyMaterial->setup();
            // TODO: (Req 10) Get the camera position
            glm::vec3 cameraPosition = camera->getOwner()->getLocalToWorldMatrix()[3];
            // TODO: (Req 10) Create a model matrix for the sky such that it always follows the camera (sky sphere center = camera position)
            glm::mat4 skyModelMatrix = glm::translate(glm::mat4(1.0f), cameraPosition);
            glm::mat4 skyMatrix = VP * skyModelMatrix;
            // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);

            // TODO: (Req 10) set the "transform" uniform
            skyMaterial->shader->set("transform", alwaysBehindTransform * skyMatrix);
            // TODO: (Req 10) draw the sky sphere
            skySphere->draw();
        }
        // TODO: (Req 9) Draw all the transparent commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (const auto &command : transparentCommands)
        {
            // Set the model-view-projection matrix (transform) uniform for the shader
            command.material->setup();
            command.material->shader->set("transform", VP * command.localToWorld);
            // Draw the mesh using the material's shader
            command.mesh->draw();
        }
        // If there is a postprocess material, apply postprocessing
        if (postprocessMaterial)
        {
            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            postprocessMaterial->setup();            
            glBindVertexArray(postProcessVertexArray);
            iTime = glfwGetTime();
            postprocessMaterial->shader->set("iTime", iTime);
            postprocessMaterial->shader->set("iResolution", iResolution);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
}