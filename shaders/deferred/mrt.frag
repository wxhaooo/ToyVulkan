#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerBaseColor;
layout (set = 1, binding = 1) uniform sampler2D samplerNormal;
layout (set = 1, binding = 2) uniform sampler2D samplerRoughness;
layout (set = 1, binding = 3) uniform sampler2D samplerEmissive;
layout (set = 1, binding = 4) uniform sampler2D samplerOcclusion;

// struct HasSampler
// {
// 	bool hasBaseColor;
// 	bool hasNormal;
// 	bool hasRoughness;
// 	bool hasEmissive;
// 	bool hasOcclusion;
// };

// layout(set = 1, binding = 8) uniform HasSamplerUBO
// {
// 	HasSampler hasSampler[];
// }hasSamperUbo;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outRoughness;
layout (location = 4) out vec4 outEmissive;
layout (location = 5) out vec4 outOcclusion;

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anyways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(samplerNormal, inUV).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(inWorldPos);
    vec3 Q2  = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV);
    vec2 st2 = dFdy(inUV);

    vec3 N   = normalize(inNormal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}


void main() 
{
	outPosition = vec4(inWorldPos, 1.0);

	outAlbedo = texture(samplerBaseColor, inUV);

	// Calculate normal in tangent space
	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent);
	vec3 B = cross(N, T);
	mat3 TBN = mat3(T, B, N);
	// outNormal = vec4(N, 1.0);
	outNormal = vec4(getNormalFromMap(),0.0);
	// outNormal = vec4(texture(samplerNormal,inUV).xyz,1.0);
	// vec3 tnorm = TBN * normalize(texture(samplerNormal, inUV).xyz * 2.0 - vec3(1.0));
	// outNormal = vec4(texture(samplerNormal, inUV).xyz, 1.0);

	outRoughness = texture(samplerRoughness, inUV);
	outEmissive = texture(samplerEmissive, inUV);
	outOcclusion = texture(samplerOcclusion,inUV);
}