#version 450

layout (binding = 0) uniform sampler2D samplerposition;
layout (binding = 1) uniform sampler2D samplerNormal;

layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerRoughness;
layout (binding = 4) uniform sampler2D samplerEmissive;
// layout (binding = 5) uniform sampler2D samplerOcclusion;

#define LIGHT_COUNT 2

struct Light
{
	float intensity;
	vec4 position;
	vec4 color;
};

layout (binding = 5) uniform UBO
{
	Light lights[LIGHT_COUNT];
	vec4 viewPos;
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
	vec3 fragcolor  = vec3(albedo);

	vec3 N = normalize(normal);

	// for(int i=0; i < LIGHT_COUNT; i++)
	// {
	// 	vec3 L = ubo.lights[i].position.xyz - fragPos;
	// 	float dist = length(L);
	// 	L = normalize(L);

	// 	vec3 V = ubo.viewPos.xyz - fragPos;
	// 	V = normalize(V);

	// 	// Diffuse lighting
	// 	float NdotL = max(0.0, dot(N, L));
	// 	vec3 diff = vec3(NdotL);

	// 	// Specular lighting
	// 	vec3 R = reflect(-L, N);
	// 	float NdotR = max(0.0, dot(R, V));
	// 	vec3 spec = vec3(pow(NdotR, 16.0) * albedo.a * 2.5);
	
	// 	fragcolor += vec3(diff) * albedo.xyz * ubo.lights[i].color.rgb;
	// }

	outFragColor = vec4(fragcolor,1.0f);
}