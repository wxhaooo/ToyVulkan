#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

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

layout (location = 0) out vec4 outPosInLightSpace;
layout (location = 1) out vec2 outUV;

void main()
{
    outUV = inUV;
    // 点在光空间中最后的位置
    outPosInLightSpace = ubo.lightSpace * primitive.model * vec4(inPos, 1.0);
    // 点在相机空间中最后的位置
    gl_Position = ubo.projection * ubo.view * primitive.model * vec4(inPos, 1.0);
}