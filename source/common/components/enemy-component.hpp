#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "../ecs/world.hpp"
#include "../ecs/component.hpp"
// #include "../ecs/laser-bullet.hpp"

#include <iostream>

namespace our{
    enum class EnemyType {
        SHOOTER,
        MELEE,
        BOSS1,
        BOSS2
    };

// Abstract class for all enemies
class EnemyComponent : public Component{
    our::EnemyType type;
    float range;
    float health;
    int weapon;
    float rateOfFire = 30;
    int deltasCounter = 0;
    World* world;

    void createEnemyRange(float range)
    {
        // get the json object representing bullet entity
        nlohmann::json rangeCollider = {
            // collider attributes
            {"type", "Collider"},
            {"colliderShape", "sphere"},
            {"colliderType", "bullet"},         // TODO: change
            {"radius", range}
        };

        ColliderComponent* enemyRangeCollider = getOwner()->addComponent<ColliderComponent>();

        // deserialize the entity data to render it and add bullet data
        enemyRangeCollider->deserialize(rangeCollider);
        
        // set collider attributes
        enemyRangeCollider->setEntity(getOwner());

        // push the entity to the colliders array
        world->addDynamicEntity(getOwner());        // TODO: may cause error
    }

    bool checkRateOfFire()
        {
            // get current weapon BPS
            // check for it's cooldown
            if (deltasCounter == 0)
            {
                deltasCounter++;
                return true;
            }
            else if ((deltasCounter)* 0.008335638028169f >= (1 / rateOfFire))
            {
                deltasCounter = 0;
            }
            else
            {
                deltasCounter++;
            }
            return false;
        }

    public:
        static std::string getID() { return "Enemy"; }

        EnemyComponent() = default;

        EnemyComponent(World* ownerWorld)
        {
            world = ownerWorld;
        }

        void deserialize(const nlohmann::json& data) override
        {
            if (!data.is_object())
                return;

            // get the enemy type
            std::string enemyTypeStr = data.value("enemyType", "shooter");
            if(enemyTypeStr == "shooter"){
                type = EnemyType::SHOOTER;
                range = 20;
            }
            else if(enemyTypeStr == "melee"){
                type = EnemyType::MELEE;
                range = 40;
            }
            else if(enemyTypeStr == "boss1"){
                type = EnemyType::BOSS1;
                range = 30;
            }
            else if(enemyTypeStr == "boss2"){
                type = EnemyType::BOSS2;
                range = 30;
            }

            createEnemyRange(range);
        }

        void aimAt(CameraComponent* camera)
        {
            auto M = getOwner()->getLocalToWorldMatrix();
            glm::vec3 eye = glm::vec3(M * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            glm::vec3 up = glm::vec3(M * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
            glm::mat4 matrix = glm::lookAt(eye, camera->getOwner()->localTransform.position, up);
            
            glm::quat newRotation;
            glm::decompose(matrix, glm::vec3(), newRotation, glm::vec3(), glm::vec3(), glm::vec4());
            glm::vec3 rotationDegrees = glm::degrees(glm::eulerAngles(newRotation));

            getOwner()->localTransform.rotation = glm::radians(rotationDegrees);
        }

        bool isPlayerInRange(CameraComponent* player)
        {
            glm::vec3 playerPosition = player->getOwner()->localTransform.position;
            // TODO: check player collision

            return true;
        }

        // void attack()
        // {
        //     if (checkRateOfFire())
        //     {

        //     glm::vec3 rotation = getOwner()->localTransform.rotation;
        //     glm::vec3 position = getOwner()->localTransform.position;

        //     float bulletRotation[3] = {180 - glm::degrees(rotation.x), glm::degrees(rotation.y) - 180, glm::degrees(rotation.z)};
        //     float bulletSpeedX = -cos(-rotation.x) * sin(rotation.y);
        //     float bulletSpeedY = -sin(-rotation.x);
        //     float bulletSpeedZ = -cos(-rotation.x) * cos(rotation.y);
        //     float bulletMovementDirections[3] = {bulletSpeedX, bulletSpeedY, bulletSpeedZ};
        //     float bulletPosition[3] = {position.x + bulletSpeedX / 4, position.y + bulletSpeedY / 4, position.z + bulletSpeedZ / 4};

        //     LaserBullet* laserBullet = new LaserBullet(bulletPosition, bulletRotation, bulletMovementDirections, world, false);
        //     laserBullet->shoot();
        //     }
        // }
    };
};
