#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 2.0, 3.0));
    vec3 normal   = normalize(fragNormal);

    float ambient  = 0.15;
    float diffuse  = max(dot(normal, lightDir), 0.0);

    // Specular
    vec3  viewDir  = normalize(vec3(0.0, 0.0, 1.0) - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfDir), 0.0), 32.0);

    vec3 color = vec3(0.7, 0.75, 0.8);  // Hellgrau
    vec3 result = color * (ambient + diffuse) + vec3(1.0) * specular * 0.5;

    outColor = vec4(result, 1.0);
}