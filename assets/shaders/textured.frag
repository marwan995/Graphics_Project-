#version 330 core

in Varyings {
    vec3 fragPosition;
    vec4 color;
    vec2 tex_coord;
    vec3 normal;

} fs_in;

struct Material {
    sampler2D tex;
    float shininess;
}; 
struct Light {
    vec3 position;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Light light; 

uniform Material material;

out vec4 frag_color;

uniform vec4 tint;
uniform vec3 camPos;

void main(){
    //TODO: (Req 7) Modify the following line to compute the fragment color
    // by multiplying the tint with the vertex color and with the texture color 
    // ambient lighting
    vec3 ambient = light.ambient * texture(material.tex, fs_in.tex_coord).rgb;

    vec3 lightPos = vec3(1.0, 2.0, 2.0);
	// diffuse lighting
	vec3 normalized = normalize(fs_in.normal);
	vec3 lightDirection =normalize(lightPos - fs_in.fragPosition);
	float diff = max(dot(normalized, lightDirection), 0.0f);
    vec3 diffuse = light.diffuse * (diff *  texture(material.tex, fs_in.tex_coord).rgb);


    //
	vec3 viewDirection = normalize(camPos - fs_in.fragPosition);
	vec3 reflectionDirection = reflect(-lightDirection, normalized);
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), material.shininess);
    vec3 specular = light.specular * (specAmount *  texture(material.tex,fs_in.tex_coord).rgb);  

//    vec4 texture_color = texture(tex, fs_in.tex_coord);
    frag_color = tint * (vec4(specular+diffuse+ambient,1.0));
}