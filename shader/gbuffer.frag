#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec3 inEyePos;
layout (location = 5) in vec3 inEyeNormal;

layout (constant_id = 0) const int type = 0;

layout (location = 0) out vec4 outNormal;
layout (location = 1) out vec4 outFragColor;

void main() 
{
	
	vec3 Reflected = normalize(reflect(-inLightVec, inEyeNormal)); 

	vec4 IAmbient = vec4(0.1, 0.1, 0.1, 1.0);
	vec4 IDiffuse = vec4(max(dot(inEyeNormal, inLightVec), 0.0));
	float specular = 0.75;
	vec4 ISpecular = vec4(0.0);
	if (dot(inEyePos, inEyeNormal) < 0.0)
	{ 
		ISpecular = vec4(0.5, 0.5, 0.5, 1.0) * pow(max(dot(Reflected, inEyePos), 0.0), 16.0) * specular; 
	}
	outNormal = 0.5*vec4(inEyeNormal,1.0)+0.5;
    outFragColor = vec4(vec3((IAmbient + IDiffuse) * inColor),0.0 );
  
}