#version 450

layout (location = 0) in vec2 inUV;


layout(binding = 0) uniform UBO{
	mat4 view;
	mat4 perspective;
	vec4 randomVector;
	vec4 time;
	vec3 _WorldSpaceCameraPos;
	int _NumSteps;
	int num;
	
	
}ubo;

layout(binding = 1) uniform sampler2D _CameraDepthTexture;
layout(binding = 2) uniform sampler2D samplerNormal;



layout (location = 0) out vec4 outRayCast;
layout (location = 1) out vec4 outRayCastMask;




const float PI = 3.1415926536;

// Based omn http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(vec2 co)
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt= dot(co.xy ,vec2(a,b));
	float sn= mod(dt,3.14);
	return fract(sin(sn) * c);
}

vec2 hammersley2d(uint i, uint N) 
{
	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return vec2(float(i) /float(N), rdi);
}

	
// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec4 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal) 
{
	// Maps a 2D point to a hemisphere with spread based on roughness
	float alpha = roughness * roughness;
	float phi = 2.0 * PI * Xi.x + random(normal.xz) * 0.1;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	// Tangent space
	vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangentX = normalize(cross(up, normal));
	vec3 tangentY = normalize(cross(normal, tangentX));
	float d = (cosTheta * alpha - cosTheta) * cosTheta + 1;
    float D = alpha / (PI * d * d);
    float pdf = D * cosTheta;
	// Convert to world Space
	return vec4(normalize(tangentX * H.x + tangentY * H.y + normal * H.z),pdf);
}
// Normal Distribution function
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}
float LinearizeDepth(float depth)
{
  float n = 0.1; // camera z near
  float f = 64.0; // camera z far
  float z = depth;
 return  n*f/ (f- depth * (f - n));	
// return -(2.0 * n*f) / (f + n - z * (f - n));	
}
float PDnrand(vec2 n) 
{
	return fract(sin(dot(n.xy, vec2(12.9898f, 78.233f)))* 43758.5453f);
}
float PDsrand(vec2 n) 
{
	return PDnrand(n) * 2 - 1;
}
vec4 RayMarch(vec3 sampleNormal, vec3 normal, vec3 position, vec3 csViewRay)
{
	if (length(sampleNormal) < 0.5f)
		return vec4(1,0,0,1);

	float maxDistance = 64.0f;
	float csNearPlane = 0.1;

	vec4 csReflectDir = vec4(reflect(csViewRay, sampleNormal),0.0);
	
	if (dot(csReflectDir.xyz, normal) < 0.0f)
		return vec4(0);
 

	vec4 csRayOrigin = vec4(position,1.0);

	float rayLength = (csRayOrigin.z + csReflectDir.z * maxDistance > csNearPlane) ? (csNearPlane - csRayOrigin.z) / csReflectDir.z : maxDistance;

	vec4 csRayEnd = csRayOrigin + csReflectDir * rayLength;

	vec4 clipRayOrigin = ubo.perspective * csRayOrigin;
	vec4 clipRayEnd = ubo.perspective * csRayEnd;

	float k0 = 1.0f / clipRayOrigin.w;
	float k1 = 1.0f / clipRayEnd.w;

	vec3 Q0 = csRayOrigin.xyz * k0;
	vec3 Q1 = csRayEnd.xyz * k1;

	vec2 P0 = clipRayOrigin.xy * k0 * 0.5f + 0.5f;
	vec2 P1 = clipRayEnd.xy * k1 * 0.5f + 0.5f;
	
	P0 *= ivec2(1280,720);
	P1 *= ivec2(1280,720);

	vec2 screenOffset = P1 - P0;
	
	float sqScreenDist = dot(screenOffset, screenOffset);
	P1 += step(sqScreenDist, 0.0001f) * vec2(0.01f);

	bool permute = false;
	if (abs(screenOffset.x) < abs(screenOffset.y))
	{
		permute = true;
		screenOffset = screenOffset.yx;
		P0 = P0.yx;
		P1 = P1.yx;
	}
	
	float stepDirection = sign(screenOffset.x);
	float stepInterval = stepDirection / screenOffset.x;
	float rayTraceStride = 1.0;
	vec3 dQ = (Q1 - Q0) * stepInterval * rayTraceStride;
	float dk = (k1 - k0) * stepInterval * rayTraceStride;
	vec2 dP = vec2(stepDirection, screenOffset.y * stepInterval) * rayTraceStride;

	float jitter = PDsrand(inUV + vec2(ubo.time.x));
	float init = 0.0;//+ jitter;
	 
	vec3 Q = Q0 + dQ * init;
	float k = k0 + dk * init;
	vec2 P = P0 + dP * init;
	
	float end = P1.x * stepDirection;

	float stepCount = 0.0f;
	float prevZMax =  -csRayOrigin.z;
	float ZMin = prevZMax;
	float ZMax = prevZMax;
	float sampleZ = prevZMax -1000;

	vec2 hit;
	int rayTraceMaxStep = 2000;
	float rayTraceHitThickness = 0.0;
	for (;((P.x * stepDirection) <= end) &&
			(stepCount <= rayTraceMaxStep - 1) &&
			//(ZMax > (sampleZ - rayTraceHitThickness)) &&
			((abs(ZMax-sampleZ)<0.001)||(ZMax > sampleZ) || (ZMin < (sampleZ - rayTraceHitThickness)) )&& 
			sampleZ != 0.0f;
			P += dP, Q.z += dQ.z, k += dk, stepCount++)
	{
		ZMin = prevZMax;
		ZMax =  -(Q.z + dQ.z * 0.5f) / (k + dk * 0.5f);
		
		prevZMax = ZMax;

		if (ZMin < ZMax)
		{
			float t = ZMin;
			ZMin = ZMax;
			ZMax = t;
		}

		hit = permute ? P.yx : P;

		float window_z = texture(_CameraDepthTexture, vec2(hit.x/1280.0,hit.y/720.0)).r;
		sampleZ = LinearizeDepth(window_z);
	}
	
	
	vec4 rayHitInfo;
	
	rayHitInfo.rg = vec2(hit.x/1280.0,hit.y/720.0);

	
	vec3 hitNormal = (texture(samplerNormal,rayHitInfo.rg)*2.0-1.0).xyz;

	rayHitInfo.b = stepCount;
	rayHitInfo.a = float((ZMax < sampleZ) && (ZMin > sampleZ - rayTraceHitThickness) && (dot(hitNormal, csReflectDir.xyz) < 0));


	return rayHitInfo;
}
vec2 seed;
float rand()
{
	seed -= vec2(ubo.randomVector.x * ubo.randomVector.y);
	return fract(sin(dot(seed, vec2(12.9898, 78.233))) * 43758.5453);
}
void main () 
{	
	vec2 uv = inUV;
	
	vec4 viewNormal = texture(samplerNormal, uv)*2.0-1.0;
	

	float depth = texture(_CameraDepthTexture, uv).r;
	vec3 viewPos;

	vec3 screenPos = vec3(uv*2.0-1.0, depth);

	vec4 viewPos0 = inverse(ubo.perspective)*vec4(screenPos,1);
	viewPos = viewPos0.xyz / viewPos0.w;

	
	float roughness = 0.1;
	seed = uv;
	vec2 Xi = vec2(rand(),rand());
	vec4 H = importanceSample_GGX(Xi, roughness, viewNormal.xyz);
		
	vec4 rayTrace = RayMarch( H.xyz, viewNormal.xyz, viewPos,normalize(viewPos));
	rayTrace.b = H.w;
	outRayCast = rayTrace ; 

	
	outRayCastMask = vec4(rayTrace.a); 

}