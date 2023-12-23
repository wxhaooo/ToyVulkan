#version 450

layout (binding = 0) uniform sampler2D samplerposition;
layout (binding = 1) uniform sampler2D samplerNormal;

layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerMetallicRoughness;
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

layout (location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic, vec3 albedo)
{
	vec3 F0 = mix(vec3(0.04), albedo, metallic); // * material.specular
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}


// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness, vec3 albedo)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic, albedo);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}


void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec3 albedo = texture(samplerAlbedo, inUV).rgb;
	float metallic = texture(samplerMetallicRoughness, inUV).g;
	float roughness = texture(samplerMetallicRoughness, inUV).b;
	vec3 emissive = texture(samplerEmissive,inUV).rgb;

	vec3 N = normalize(normal);
	vec3 V = normalize(ubo.viewPos.xyz - fragPos);

	// Specular contribution
	vec3 Lo = vec3(0.0);
	for(int i=0; i < LIGHT_COUNT; i++)
	{
		vec3 L = normalize(ubo.lights[i].position.xyz - fragPos);
		Lo += BRDF(L, V, N, metallic, roughness, albedo);
	}

	vec3 color = albedo * 0.02;
	color += Lo;

	color = pow(color, vec3(0.4545));
	outColor = vec4(color, 1.0);

	// for(int i=0; i < LIGHT_COUNT; i++)
	// {
	// 	vec3 L = ubo.lights[i].position.xyz - fragPos;
	// 	float dist = length(L);
	// 	L = normalize(L);

	// 	// Specular lighting
	// 	vec3 R = reflect(-L, N);

	// 	// Diffuse lighting
	// 	float NdotL = max(0.0, dot(N, L));
	// 	vec3 diff = vec3(NdotL);

	// 	float NdotR = max(0.0, dot(R, V));
	// 	vec3 spec = vec3(pow(NdotR, 16.0) * albedo.r * 2.5);
	
	// 	Lo += vec3(diff) * albedo.xyz * ubo.lights[i].color.rgb;
	// }

	// vec3 color = vec3(0.0);
	// color += Lo;

	// color = pow(color, vec3(0.4545));
	// outColor = vec4(color, 1.0);
}