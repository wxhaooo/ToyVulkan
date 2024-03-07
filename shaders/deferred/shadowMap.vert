#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBO
{
    float nearPlane;
    float farPlane;
    mat4 projection;
    mat4 view;
    mat4 lightSpace;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

void main()
{
    gl_Position = ubo.lightSpace * primitive.model * vec4(inPos, 1.0);
}