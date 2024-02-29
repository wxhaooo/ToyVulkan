#version 450

layout (binding = 0) uniform sampler2D samplerSSAO;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

void main() 
{
	const int blurRange = 2;
	int n = 0;
	vec2 texelSize = 1.0 / vec2(textureSize(samplerSSAO, 0));
	float result = 0.0;
	for (int x = -blurRange; x < blurRange; x++) 
	{
		for (int y = -blurRange; y < blurRange; y++) 
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += texture(samplerSSAO, inUV + offset).r;
			n++;
		}
	}
	float blurSSAO = result / (float(n));
	outColor = vec4(blurSSAO, blurSSAO, blurSSAO, 1.0);
}