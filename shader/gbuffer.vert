#version 450

layout (location = 0) in vec4 inPos;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;
layout (constant_id = 0) const int type = 0;


layout (binding = 0) uniform UBO 
{
	
	mat4 projection;
	mat4 model;
	mat4 view;
	
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outEyePos;
layout (location = 5) out vec3 outEyeNormal;


out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
			outNormal = transpose(inverse(mat3(ubo.model)))*inNormal;
			outEyeNormal = transpose(inverse(mat3(ubo.view*ubo.model)))*inNormal;
			outColor = vec4(inColor,1.0);
			outWorldPos = vec3(ubo.model * inPos);
			outEyePos = vec3(ubo.view*ubo.model * inPos);
			gl_Position = ubo.projection * ubo.view * ubo.model * inPos;
			
				
	

	
	       outLightVec = normalize(vec3( 0.0f)-outEyePos);
	

	
}
