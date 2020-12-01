#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;




layout (location = 0) out vec4 outNormal;

layout (location = 1) out vec4 outColor;

const float PI = 3.14159265359;

//#define ROUGHNESS_PATTERN 1



// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic)
{
	vec3 F0 = mix(vec3(0.04), inColor.xyz, metallic); // * material.specular
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}

// ----------------------------------------------------------------------------
void main()
{		  
	vec3 campos=vec3(0.35f, 0.25f, -0.5f);
	vec3 N = normalize(inNormal);
	vec3 V = normalize(campos - inWorldPos);

	float roughness = 0.5;

	
	// Specular contribution
	vec3 Lo = vec3(0.0);
	vec3 light = vec3(.64299999f, .54779997f, .22700010f);
	vec3 L = normalize(light.xyz - inWorldPos);
	Lo += BRDF(L, V, N, 0.01, roughness);


	// Combine with ambient
	vec3 color = inColor.xyz * 0.02;
	color += Lo;

	// Gamma correct
	color = pow(color, vec3(0.4545));
	outNormal = vec4(0.5*normalize(inNormal)+0.5,1.0);
	outColor =vec4(color*vec3(4.0,3.0,1),inColor.a);
}