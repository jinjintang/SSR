#include<map>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class HaltonSequence
{
public:
	enum HaltonMode
	{
		x8,
		x16,
		x32,
		x256,
		HaltonModeCount,
	};

	static float POINTS_HALTON_2_3_X8[8 * 2];
	static float POINTS_HALTON_2_3_X16[16 * 2];
	static float POINTS_HALTON_2_3_X32[32 * 2];
	static float POINTS_HALTON_2_3_X256[256 * 2];

	static bool Initialized;
	static unsigned int PatternLength;
	static float PatternScale;

	static std::pair<float*, unsigned int> POINTS_HALTON_2_3[HaltonModeCount];

	static glm::vec2 Sample(float* pattern, int index);
	static float HaltonSeq(unsigned int prime, unsigned int index);
	static void InitializeHalton_2_3();
	static glm::vec2 GetHaltonJitter(HaltonMode mode, uint64_t index);
};

class FrustumJitter 
{
	

public:
	static std::shared_ptr<FrustumJitter> Create();

protected:
	bool Init(const std::shared_ptr<FrustumJitter>& pJitter);

public:
	void Awake();
	void Start();
	void Update();

public:
	void SetHaltonMode(HaltonSequence::HaltonMode mode);

protected:
	std::shared_ptr<PhysicalCamera>	m_pCamera;
	HaltonSequence::HaltonMode		m_haltonMode = HaltonSequence::HaltonMode::x8;
	unsigned int						m_currentIndex = 0;
	bool							m_jitterEnabled = true;
};