#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerBaseColor;
layout (set = 1, binding = 1) uniform sampler2D samplerNormal;
// r = ao (optional), g = roughness, b = metallic
layout (set = 1, binding = 2) uniform sampler2D samplerOcclusionRoughnessMetallic;
layout (set = 1, binding = 3) uniform sampler2D samplerEmissive;
// optional
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
layout (location = 3) out vec4 outRoughnessMetallic;
layout (location = 4) out vec4 outEmissive;
layout (location = 5) out vec4 outOcclusion;
layout (location = 6) out vec4 outDepth;

layout (set = 0, binding = 0) uniform UBO 
{
	float nearPlane;
	float farPlane;
	mat4 projection;
	mat4 view;
} ubo;

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

float linearDepth(float depth)
{
	float z = depth;
	return (ubo.nearPlane * ubo.farPlane) / (z * (ubo.farPlane - ubo.nearPlane) - ubo.farPlane);
}

// float linearDepth(float depth)
// {
// 	float z = depth;
// 	return (ubo.nearPlane * ubo.farPlane) / (ubo.farPlane + z * (ubo.farPlane - ubo.nearPlane));
// }

void main()
{
    outPosition = vec4(inWorldPos, 1.0);

    outAlbedo = texture(samplerBaseColor, inUV);

    // Calculate normal in tangent space
    // vec3 N = normalize(inNormal);
    // vec3 T = normalize(inTangent);
    // vec3 B = cross(N, T);
    // mat3 TBN = mat3(T, B, N);
    // outNormal = vec4(N, 1.0);
    outNormal = vec4(getNormalFromMap(), 1.0);
    // outNormal = vec4(texture(samplerNormal,inUV).xyz,1.0);
    // vec3 tnorm = TBN * normalize(texture(samplerNormal, inUV).xyz * 2.0 - vec3(1.0));
    // outNormal = vec4(texture(samplerNormal, inUV).xyz, 1.0);
    outRoughnessMetallic = vec4(0.0, texture(samplerOcclusionRoughnessMetallic, inUV).gb, 1.0);
    outEmissive = texture(samplerEmissive, inUV);

    // ao
    ivec2 occlusionSize2d = textureSize(samplerOcclusion, 0);
    if(occlusionSize2d.x != 1)
        outOcclusion = vec4(texture(samplerOcclusion, inUV).rrr, 1);
    else
        outOcclusion = vec4(texture(samplerOcclusionRoughnessMetallic, inUV).rrr, 1);

    float depth = gl_FragCoord.z;
//    float depth = linearDepth(gl_FragCoord.z);
    // float depth = 1.0 - gl_FragCoord.z / gl_FragCoord.w;
    outDepth = vec4(depth, depth, depth, 1.0);
}