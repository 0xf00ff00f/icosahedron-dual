#version 450 core

in vec3 world_normal;
in vec3 world_position;

uniform vec3 eyePosition;

const vec3 light_position = vec3(3.0, 5.0, 3.0);
const vec3 ambient = vec3(0.15);
const vec3 diffuse_color = vec3(1.0, 0.0, 0.0);
const float shininess = 8.0;
const vec3 light_color = vec3(1.0, 1.0, 1.0);
const float kd = 0.5;
const float ks = 0.5;

out vec4 frag_color;

void main(void)
{
    vec3 l = normalize(light_position - world_position);
    float diffuse_light = max(dot(world_normal, l), 0.0);
    vec3 diffuse = kd * diffuse_light * diffuse_color;
    vec3 v = normalize(eyePosition - world_position);
    vec3 h = normalize(l + v);
    float specular_light = pow(max(dot(world_normal, h), 0.0), shininess);
    if (diffuse_light <= 0.0)
        specular_light = 0.0;
    vec3 specular = ks * specular_light * light_color;
    frag_color = vec4(ambient + diffuse + specular, 1.0);
}
