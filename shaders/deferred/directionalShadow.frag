#version 450

layout (binding = 1) uniform sampler2D samplerShadowMap;

layout (location = 0) in vec4 inPosInLightSpace;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormalInWorld;
layout (location = 3) in vec4 inPositionInWorld;

layout (set = 0, binding = 0) uniform UBO
{
    float nearPlane;
    float farPlane;
    mat4 projection;
    mat4 view;
    mat4 lightSpace;
    vec4 lightPosition;
} ubo;

layout (location = 0) out vec4 outColor;

float textureProj(vec4 shadowCoord, vec2 off, float bias)
{
	float shadow = 0.0;
    float currentDepth = shadowCoord.z;
    vec2 uv = shadowCoord.xy * 0.5 + 0.5;
	if (currentDepth < 1.0) 
	{
		float dist = texture(samplerShadowMap, uv + off).r;
		if (dist < currentDepth - bias) 
			shadow = 1.0;
	}
	return shadow;
}

float filterPCF(vec4 sc, float bias)
{
	ivec2 texDim = textureSize(samplerShadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), bias);
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main()
{
    vec3 lightDir = normalize(inPositionInWorld - ubo.lightPosition).xyz;
    float bias = max(0.01 * (1.0 - dot(-lightDir, inNormalInWorld)), 0.001);
    vec4 projCoords = inPosInLightSpace / inPosInLightSpace.w;
    // vec2 uv = projCoords.xy * 0.5 + 0.5;
    float shadow = filterPCF(projCoords, bias) * 0.65;
    // float closestDepth = texture(samplerShadowMap, uv).r;
    // float currentDepth = projCoords.z;
    // float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;  
    // if(currentDepth > 1.0)
        // shadow = 0.0;
    outColor = vec4(shadow, shadow, shadow, 1.0);
}