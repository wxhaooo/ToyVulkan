#version 450

layout (binding = 0) uniform sampler2D samplerSSAO;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

void main() 
{
	vec3 color = texture(samplerSSAO, inUV).rgb;
	outColor = vec4(color, 1.0);
}