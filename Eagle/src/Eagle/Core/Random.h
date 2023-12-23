#pragma once

namespace Eagle::Random
{
	// Max is exclusive
	int Int(int min = 0, int max = std::numeric_limits<int>::max());

	// Max is exclusive
	uint32_t UInt(uint32_t min = 0, uint32_t max = std::numeric_limits<uint32_t>::max());

	// Max is exclusive
	int64_t Int64(int64_t min = 0, int64_t max = std::numeric_limits<int64_t>::max());

	// Max is exclusive
	uint64_t UInt64(uint64_t min = 0, uint64_t max = std::numeric_limits<uint64_t>::max());

	// Max is exclusive
	float Float(float min = 0.f, float max = 1.f);

	// Max is exclusive
	double Double(double min = 0.f, double max = 1.f);
}
