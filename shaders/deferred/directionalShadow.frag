#version 450

layout (binding = 1) uniform sampler2D samplerShadowLut;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

void main()
{
    outColor = texture(samplerShadowLut, inUV);
}