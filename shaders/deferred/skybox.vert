#version 450

//layout (location = 0) in vec3 inPos;
//layout (location = 1) in vec2 inUV;
//layout (location = 2) in vec3 inColor;
//layout (location = 3) in vec3 inNormal;
//layout (location = 4) in vec3 inTangent;

layout (binding = 3) uniform UBO
{
	mat4 invProjView;
} ubo;

layout (location = 0) out vec3 outUVW;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewPos;

void main()
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vec4 posInClip = vec4(outUV * 2.0f - 1.0f, 1.0f, 1.0f);
//	vec4 posInClip = vec4(outUV.x * 2.0f - 1.0f, outUV.y * -2.0f + 1.0f, 0.0f, 1.0f);
	// Set z = 1 to draw skybox at the back of the depth range
//	vec4 posInClip = vec4(inPos.xy, 1.0f, 1.0f);
	vec4 vecView = ubo.invProjView * posInClip;
	outViewPos = vecView.xyz / vecView.w;
	gl_Position = posInClip;

//	outUVW = inPos;
//	outUV = inUV;
//	gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
}
