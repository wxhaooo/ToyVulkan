#version 450

layout (binding = 1) uniform sampler2D samplerShadowMap;

layout (location = 0) in vec4 inPosInLightSpace;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outColor;

void main()
{
    vec3 projCoords = inPosInLightSpace.xyz / inPosInLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; 
    float closestDepth = texture(samplerShadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;
    // outColor = vec4(shadow, shadow, shadow, 1.0);
    outColor = vec4(0.2, 0.2, 0.2, 1.0);
}