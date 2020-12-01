#pragma once
#include <vector>
class HaltonValue
{
public:

	HaltonValue();
	std::vector<float> GenerateRandomOffset();
	~HaltonValue();

private:
	// From Unity TAA
	 int m_SampleIndex = 0;
	 const int k_SampleCount = 64;
	 std::vector<float> offset;
	 float  GetHaltonValue(int index, int radix);
	 
};

HaltonValue::HaltonValue()
{
	offset.resize(2);
}

HaltonValue::~HaltonValue()
{
}

 float HaltonValue:: GetHaltonValue(int index, int radix)
{
	float result = 0.0f;
	float fraction = 1.0f / (float)radix;

	while (index > 0)
	{
		result += (float)(index % radix) * fraction;

		index /= radix;
		fraction /= (float)radix;
	}

	return result;
}

 std::vector<float> HaltonValue::GenerateRandomOffset()
{
	 offset[0] = GetHaltonValue(m_SampleIndex & 1023, 2);
	 offset[1] = GetHaltonValue(m_SampleIndex & 1023, 3);
	
	if (++m_SampleIndex >= k_SampleCount)
		m_SampleIndex = 0;

	return offset;
}

//Vector2 jitterSample = GenerateRandomOffset();
//
//context.command.SetComputeVectorParam(computeShader, ComputeUniforms.JitterSizeAndOffset, new Vector4
//(
//(float)context.width / (float)noise.width,
//(float)context.height / (float)noise.height,
//jitterSample.x,
//jitterSample.y
//));

