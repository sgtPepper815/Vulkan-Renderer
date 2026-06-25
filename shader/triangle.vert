#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;

layout(binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
    fragNormal = mat3(ubo.model) * inNormal;
    fragPos    = vec3(ubo.model * vec4(inPos, 1.0));
}