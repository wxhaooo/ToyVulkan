#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;

layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerMetallicRoughness;
layout (binding = 4) uniform sampler2D samplerEmissive;
layout (binding = 5) uniform sampler2D samplerOcclusion;

layout (binding = 6) uniform samplerCube samplerIrradianceCube;
layout (binding = 7) uniform samplerCube samplerPreFilteringCube;
layout (binding = 8) uniform sampler2D samplerSpecularBRDFLut;

#define LIGHT_COUNT 2

struct Light
{
	float intensity;
	vec4 position;
	vec4 color;
};

layout (binding = 9) uniform UBO
{
	Light lights[LIGHT_COUNT];
	vec4 viewPos;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// Normal Distribution function --------------------------------------
float D_GGX(float NH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = NH * NH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float NL, float NV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = NL / (NL * (1.0 - k) + k);
	float GV = NV / (NV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, float roughness, vec3 F0)
{
	vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
	return F;    
}

// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(samplerPreFilteringCube, R, lodf).rgb;
	vec3 b = textureLod(samplerPreFilteringCube, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerPosition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec3 albedo = pow(texture(samplerAlbedo, inUV).rgb, vec3(2.2));
	float metallic = texture(samplerMetallicRoughness, inUV).b;
	float roughness = texture(samplerMetallicRoughness, inUV).g;
	vec3 emissive = texture(samplerEmissive, inUV).rgb;
	float ao = texture(samplerOcclusion, inUV).r;

	vec3 N = normalize(normal);
	vec3 V = normalize(ubo.viewPos.xyz - fragPos);
	vec3 R = reflect(-V, N);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	// direct lighting
	// Precalculate vectors and dot products
	float NV = clamp(dot(N, V), 0.0, 1.0);

	// Specular contribution
	vec3 Lo = vec3(0.0);
	for(int i=0; i < LIGHT_COUNT; i++)
	{
		vec3 lightPos = ubo.lights[i].position.xyz;
		vec3 lightColor = ubo.lights[i].color.xyz;
		vec3 LD = lightPos - fragPos;
		float dist = length(LD);
		float attenuation = 1.0 / (dist * dist);
		vec3 radiance = lightColor * attenuation;

		vec3 L = normalize(LD);
		vec3 H = normalize (V + L);

		float HV = clamp(dot(H, V), 0.0, 1.0);
		float NH = clamp(dot(N, H), 0.0, 1.0);
		float NL = clamp(dot(N, L), 0.0, 1.0);
		float LH = clamp(dot(L, H), 0.0, 1.0);

		// BRDF
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(NH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(NL, NV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = fresnelSchlick(HV, F0);

		vec3 ks = F;
        vec3 kd = vec3(1.0) - ks;
        kd *= (1.0 - metallic);

		vec3 spec = D * F * G / (4.0 * NV * NL + 1e-4);
		Lo += (kd * albedo / PI + spec) * radiance * NL; 
	}

	// ambient lighting
	vec3 F = fresnelSchlickRoughness(NV, roughness, F0);
	vec3 ks = F;
	vec3 kd = 1.0 - ks;
	kd *= (1.0 - metallic);	
  
	vec3 irradiance = texture(samplerIrradianceCube, N).rgb;
	vec3 diffuse = irradiance * albedo;
	// vec3 diffuse = vec3(0.0);

	vec3 reflectionColor = prefilteredReflection(R, roughness).rgb; 
	// const float MAX_REFLECTION_LOD = 7.0;
	// vec3 reflectionColor = textureLod(samplerPreFilteringCube, R, roughness * MAX_REFLECTION_LOD).rgb;   
	vec2 envBRDF = texture(samplerSpecularBRDFLut, vec2(NV, roughness)).rg;
	// raw implementation
	// vec3 specular = reflectionColor * (F * envBRDF.x + envBRDF.y);
	// multi albedo
	vec3 specular = reflectionColor * (F * envBRDF.x + envBRDF.y);
	// vec3 specular = reflectionColor * (F * envBRDF.x + envBRDF.y) * albedo;
	// 这个实现存疑，但是原来的实现没有考虑到反射的颜色和物体本身颜色的关系以及是否是金属，所以改成这样了
	// vec3 specular = reflectionColor * (F * envBRDF.x + envBRDF.y) * albedo * metallic;
	// vec3 specular = vec3(0.0);
	vec3 ambient = (kd * diffuse + specular) * ao.rrr;
    vec3 color = ambient + Lo;
	// 自发光没有处理好，需要做一下
	color += emissive;

	// color = vec3(NV, NV, NV);
	// color = V;

	// Tone mapping
	color = Uncharted2Tonemap(color * 2.5);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    // HDR tonemapping
    // color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color , 1.0);

	// vec3 Lo = vec3(0.0);
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