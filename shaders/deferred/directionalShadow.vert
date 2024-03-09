#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;

layout (set = 0, binding = 0) uniform UBO
{
    float nearPlane;
    float farPlane;
    mat4 projection;
    mat4 view;
    mat4 lightSpace;
    vec4 lightPosition;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

layout (location = 0) out vec4 outPosInLightSpace;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormalInWorld;
layout (location = 3) out vec4 outPositionInWorld;

void main()
{
    outUV = inUV;
    outNormalInWorld = normalize(transpose(inverse(mat3(primitive.model))) * inNormal);
    outPositionInWorld = primitive.model * vec4(inPos, 1.0);
    // 点在光空间中最后的位置
    outPosInLightSpace = ubo.lightSpace * outPositionInWorld;
    // 点在相机空间中最后的位置
    gl_Position = ubo.projection * ubo.view * outPositionInWorld;
}