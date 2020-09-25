#version 450
layout (location = 0) in vec2 inUV;

layout (binding = 0) uniform UBO{
	
	mat4 view;
	mat4 perspective;
    vec4 _WorldSpaceCameraPos;
	ivec4 _ScreenSize;
	int num;
}ubo;
layout(binding = 1) uniform sampler2D samplerNormal;
layout(binding = 2) uniform sampler2D _CameraDepthTexture;
layout(binding = 3) uniform sampler2D _RayCast;
layout(binding = 4) uniform sampler2D _MainTex;
layout(binding = 5) uniform sampler2D  _RayCastMask;
layout(binding = 6) uniform sampler2D  Accumulate;

layout (location = 0) out vec4 outFragColor;
vec4 blur(vec2 uv){
	float w[5];
	w[0] = 0.227027;
	w[1] = 0.1945946;
	w[2] = 0.1216216;
	w[3] = 0.054054;
	w[4] = 0.016216;
    float weightSum = 0.0;	
	vec2 _ResolveSize = vec2(1280.0,720.0);
	vec4 color = vec4(0.0);
	vec2 offset[5] =
{
	vec2(0, 0),
	vec2(1, 0),
	vec2(-1, 0),
	vec2(0, 1),
	vec2(0,-1)
};
	for(int i = 0; i < 5; i++)
	{
	vec2 offsetUV = offset[i] * (1.0 / _ResolveSize.xy);
	
	vec2 neighborUv = uv + offsetUV;

	color += texture(_RayCast,neighborUv)*w[i];
	weightSum +=w[i];


	}

	return color/weightSum;



}

vec2 offset[5] =
{
	vec2(0, 0),
	vec2(1, 0),
	vec2(-1, 0),
	vec2(0, 1),
	vec2(0,-1)
};

int _RayReuse = 0;
float Luminance(vec3 c){
return 0.3*c.x + 0.6*c.y + 0.1*c.z;}

const float PI = 3.1415926536;
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
float F_Schlick(float cosTheta, float metallic)
{
	float F0 = 0.04;
	float F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
} 
}
vec4 blur(vec2 uv){
	float w[5];
	w[0] = 0.227027;
	w[1] = 0.1945946;
	w[2] = 0.1216216;
	w[3] = 0.054054;
	w[4] = 0.016216;
    float weightSum = 0.0;	
	vec2 _ResolveSize = vec2(1280.0,720.0);
	vec4 color = vec4(0.0);
	vec2 offset[5] =
{
	vec2(0, 0),
	vec2(1, 0),
	vec2(-1, 0),
	vec2(0, 1),
	vec2(0,-1)
};
	for(int i = 0; i < 5; i++)
	{
	vec2 offsetUV = offset[i] * (1.0 / _ResolveSize.xy);
	
	vec2 neighborUv = uv + offsetUV;

	color += texture(_MainTex,neighborUv)*w[i];
	weightSum +=w[i];


	}

	return color/weightSum;



}
void main() 
{
	vec2 uv = inUV;
	vec4 viewNormal = texture(samplerNormal, uv)*2.0-1.0;


	float depth = texture(_CameraDepthTexture, uv).r;
	vec3 screenPos = vec3(uv.xy * 2.0 - 1.0, depth);
	
	vec4 viewPos0 =inverse(ubo.perspective)*vec4(screenPos,1);
	vec3 viewPos = viewPos0.xyz / viewPos0.w;


	int NumResolve = 1;
	if(_RayReuse == 1)
		NumResolve = 5;

	vec4 result = vec4(0.0);
    float weightSum = 0.0;	

	vec2 _ResolveSize = vec2(1280.0,720.0);

	
	for(int i = 0; i < NumResolve; i++)
	{
	vec2 offsetUV = offset[i] * (1.0 / _ResolveSize.xy);
	
	vec2 neighborUv = uv + offsetUV;

	// Now we fetch the intersection point and the PDF that the neighbor's ray hit.
	vec4 hitPacked = texture(_RayCast, neighborUv);
	vec2 hitUv = hitPacked.xy;      
	float hitPDF = hitPacked.z;
	float hitMask = texture(_RayCastMask, neighborUv).r;
	float hitZ = texture(_CameraDepthTexture,hitUv).r;
	vec4 hitViewPos0 = inverse(ubo.perspective)*vec4(hitUv*2.0-1.0, hitZ,1.0);
	vec3 hitViewPos =  hitViewPos0.xyz/hitViewPos0.z;
		
	vec3  L = hitViewPos-viewPos;
	vec3 V = -viewPos;
	vec3 H = normalize(L+V);
	vec3 N = viewNormal.xyz;
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotVH = clamp(dot(V, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	float roughness = 0.1;//texture(_MainTex,inUV).a; 
		
	float D = D_GGX(dotNH, roughness);  
	
	float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
	float F = F_Schlick(dotNV, 0.1);
	float weight = D*G*F/((4.0 * dotNL * dotNV)*hitPDF);	

	
	vec4 sampleColor= texture(_MainTex, hitUv);
	
	sampleColor.rgb /= 1 + Luminance(sampleColor.rgb);

	result += sampleColor*hitMask* weight;
	weightSum += weight*hitMask; 

	}

    result /= weightSum;
	result.rgb /= 1 - Luminance(result.rgb);

	if (texture(_RayCastMask, uv).r>0)
    outFragColor = (mix(texture(_MainTex,inUV), max(vec4(1e-5), result),0.5)+texture(Accumulate,inUV)*(ubo.num-1))/ubo.num;
	else{
	outFragColor = (texture(_MainTex,inUV)+texture(Accumulate,inUV)*(ubo.num-1))/ubo.num;
	}
}