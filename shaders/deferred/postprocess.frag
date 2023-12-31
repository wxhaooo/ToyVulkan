#version 450
layout (binding = 0) uniform sampler2D samplerDepth;
layout (binding = 1) uniform sampler2D samplerLighting;
layout (binding = 2) uniform sampler2D samplerEnv;

layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outColor;

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

void main() 
{
	vec3 color = texture(samplerEnv, inUV).rgb;
	float sceneDepth = texture(samplerDepth, inUV).r;
	vec3 sceneColor = texture(samplerLighting, inUV).rgb;

	if(sceneDepth < 1)
		color = sceneColor;

	// Tone mapping
	color = Uncharted2Tonemap(color * 2.5);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(1.0f / 2.2));

	outColor = vec4(color, 1.0);
}