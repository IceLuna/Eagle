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
	int Int()
	{
		return s_IntDistribution(s_Engine);
	}

	int64_t Int64()
	{
		return s_Int64Distribution(s_Engine);
	}

	uint32_t UInt()
	{
		return s_UIntDistribution(s_Engine);
	}

	uint64_t UInt64()
	{
		return s_UInt64Distribution(s_Engine);
	}

	float Float()
	{
		return s_FloatDistribution(s_Engine);
	}

	double Double()
	{
		return s_DoubleDistribution(s_Engine);
	}
}
