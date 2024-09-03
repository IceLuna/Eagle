#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	struct AABB
	{
		AABB() = default;
		AABB(const glm::vec3& min, const glm::vec3& max) : Min(min), Max(max) {}

		glm::vec3 Min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 Max = glm::vec3(std::numeric_limits<float>::lowest());

		glm::vec3 Center() const { return (Min + Max) * 0.5f; }
		glm::vec3 Extents() const { return Max - Min; }

		void Grow(const AABB& other)
		{
			Min = glm::min(Min, other.Min);
			Max = glm::max(Max, other.Max);
		}

		void Grow(const glm::vec3& p)
		{
			Min = glm::min(Min, p);
			Max = glm::max(Max, p);
		}

		bool Contains(const glm::vec3& p) const
		{
			const glm::vec3 radius = Extents() * 0.5f;
			const glm::vec3 center = Center();

			return
				(glm::abs(p.x - center.x) <= radius.x) &&
				(glm::abs(p.y - center.y) <= radius.y) &&
				(glm::abs(p.z - center.z) <= radius.z);
		}

		float MaxSide() const
		{
			const glm::vec3 extents = Extents();
			return glm::max(extents.x, glm::max(extents.y, extents.z));
		}

		bool operator== (const AABB& other) const
		{
			return Min == other.Min && Max == other.Max;
		}
	};
}
