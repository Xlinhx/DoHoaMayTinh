#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
uniform mat4 model, view, projection, lightVP;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;
out vec4 lightPosition;
void main() {
    vec4 p = model * vec4(position, 1.0);
    worldPosition = p.xyz;
    worldNormal = transpose(inverse(mat3(model))) * normal;
    uv = texcoord;
    lightPosition = lightVP * p;
    gl_Position = projection * view * p;
}
