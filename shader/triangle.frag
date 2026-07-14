#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 2.0, 3.0));
    vec3 normal   = normalize(fragNormal);

    float ambient  = 0.15;
    float diffuse  = max(dot(normal, lightDir), 0.0);

    // Specular
    vec3  viewDir  = normalize(vec3(0.0, 0.0, 1.0) - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfDir), 0.0), 32.0);

    vec3 texColor = texture(texSampler, fragUv).rgb;
    vec3 result = texColor * (ambient + diffuse) + vec3(1.0) * specular * 0.5;

    outColor = vec4(result, 1.0);
}
