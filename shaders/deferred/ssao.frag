#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
//layout (binding = 2) uniform sampler2D samplerDepth;
layout (binding = 2) uniform sampler2D ssaoNoise;

layout (constant_id = 0) const int SSAO_KERNEL_SIZE = 64;
// layout (constant_id = 1) const float SSAO_RADIUS = 0.3;

layout (binding = 3) uniform UBO
{
	mat4 view;
	mat3 invViewT;
	mat4 projection;
	vec4 samples[SSAO_KERNEL_SIZE];	
	float ssaoRadius;
	float ssaoBias;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	// frag and normal shoule be in view space
	// Get G-Buffer values
	vec4 worldPos = texture(samplerPosition, inUV);
	vec3 fragPos = (ubo.view * worldPos).rgb;
	vec3 worldNormal = texture(samplerNormal, inUV).rgb;
	vec3 normal = ubo.invViewT * worldNormal;
	// normal = normalize(normal * 2.0 - 1.0);

	// Get a random vector using a noise lookup
	ivec2 texDim = textureSize(samplerPosition, 0); 
	ivec2 noiseDim = textureSize(ssaoNoise, 0);
	const vec2 noiseUV = vec2(float(texDim.x) / float(noiseDim.x), float(texDim.y) / (noiseDim.y)) * inUV;  
	vec3 randomVec = texture(ssaoNoise, noiseUV).xyz;
	// vec3 randomVec = texture(ssaoNoise, noiseUV).xyz * 2.0 - 1.0;
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{		
		vec3 samplePos = TBN * ubo.samples[i].xyz; 
		samplePos = fragPos + samplePos * ubo.ssaoRadius; 
		
		// project
		vec4 offset = vec4(samplePos, 1.0f);
		offset = ubo.projection * offset; 
		offset.xyz /= offset.w; 
		offset.xyz = offset.xyz * 0.5f + 0.5f;

		vec4 offsetWorldPos = texture(samplerPosition, offset.xy);
		float sampleDepthValue = (ubo.view * offsetWorldPos).z;
//		float sampleDepthValue = texture(samplerDepth, offset.xy).x;

		float rangeCheck = smoothstep(0.0f, 1.0f, ubo.ssaoRadius / abs(fragPos.z - sampleDepthValue));
		occlusion += (sampleDepthValue >= samplePos.z + ubo.ssaoBias ? 1.0f : 0.0f) * rangeCheck;
	}
	occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
	
	outFragColor = vec4(occlusion, occlusion, occlusion, 1.0);
}

