#version 460

layout (set=0, binding=0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout (push_constant) uniform constants
{
    mat4 model;
} pc;

void main() {
    gl_Position = camera.proj * camera.view * pc.model * vec4(inPosition, 1.0);
}