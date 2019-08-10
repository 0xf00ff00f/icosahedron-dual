#version 450 core

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

uniform mat4 mvp;
uniform mat3 normalMatrix;
uniform mat4 modelMatrix;

out vec3 world_position;
out vec3 world_normal;

void main(void)
{
    world_position = vec3(modelMatrix * vec4(position, 1.0));
    world_normal = normalMatrix*normal;
    gl_Position = mvp*vec4(position, 1.0);
}
