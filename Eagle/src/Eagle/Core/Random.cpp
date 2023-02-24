#include "egpch.h"
#include "Random.h"

#include <random>

static std::random_device s_RandomDevice;
static thread_local std::mt19937_64 s_Engine(s_RandomDevice());
static thread_local std::uniform_int_distribution<int> s_IntDistribution;
static thread_local std::uniform_int_distribution<int64_t> s_Int64Distribution;
static thread_local std::uniform_int_distribution<uint32_t> s_UIntDistribution;
static thread_local std::uniform_int_distribution<uint64_t> s_UInt64Distribution;
static thread_local std::uniform_real_distribution<float> s_FloatDistribution;
static thread_local std::uniform_real_distribution<double> s_DoubleDistribution;

namespace Eagle::Random
{
	int Int(int min, int max)
	{
		return (s_IntDistribution(s_Engine) % (max - min)) + min;
	}

	uint32_t UInt(uint32_t min, uint32_t max)
	{
		return (s_UIntDistribution(s_Engine) % (max - min)) + min;
	}

	int64_t Int64(int64_t min, int64_t max)
	{
		return (s_Int64Distribution(s_Engine) % (max - min)) + min;
	}

	uint64_t UInt64(uint64_t min, uint64_t max)
	{
		return (s_UInt64Distribution(s_Engine) % (max - min)) + min;
	}

	float Float(float min, float max)
	{
		return s_FloatDistribution(s_Engine) * (max - min) + min;
	}

	double Double(double min, double max)
	{
		return s_DoubleDistribution(s_Engine) * (max - min) + min;
	}
}
