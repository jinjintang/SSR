#version 450

layout (location = 0) in vec2 inUV;
layout(binding = 0) uniform sampler2D samplerColor;
layout(binding = 1) uniform sampler2D samplerColor1;

layout (location = 0) out vec4 outColor;
void main() 
{
	float mask = texture(samplerColor,inUV).a;
	//outColor = texture(samplerColor,inUV);
	outColor = mix(texture(samplerColor1,inUV),texture(samplerColor,inUV),mask);
}
