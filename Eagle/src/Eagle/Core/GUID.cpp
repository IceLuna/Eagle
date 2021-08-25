#include "egpch.h"
#include "GUID.h"

#include <random>

namespace Eagle
{
	static std::random_device s_RandomDevice;
	static std::mt19937_64 eng(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

	GUID::GUID() 
	: m_GUID(s_UniformDistribution(eng))
	{}
}
