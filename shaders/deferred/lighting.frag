#version 450

layout (binding = 0) uniform sampler2D samplerposition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;

#define LIGHT_COUNT 2

struct Light
{
	float intensity;
	mat4 transform;
	vec4 color;
};

layout (binding = 3) uniform UBO
{
	Light lights[LIGHT_COUNT];
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 albedo = texture(samplerAlbedo, inUV);

	// Ambient part
	vec3 fragcolor  = albedo.rgb;
	outFragColor = vec4(ubo.lights[1].color.xyz, 1.0);
}