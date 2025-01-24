#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	struct AABB
	{
		constexpr AABB() = default;
		constexpr AABB(const glm::vec3& min, const glm::vec3& max) : Min(min), Max(max) {}

		glm::vec3 Min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 Max = glm::vec3(std::numeric_limits<float>::lowest());

		constexpr glm::vec3 Center() const { return (Min + Max) * 0.5f; }
		constexpr glm::vec3 Extents() const { return Max - Min; }
		float Length() const { return glm::length(Extents()); }

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

		constexpr bool Contains(const glm::vec3& p) const
		{
			const glm::vec3 radius = Extents() * 0.5f;
			const glm::vec3 center = Center();

			return
				(glm::abs(p.x - center.x) <= radius.x) &&
				(glm::abs(p.y - center.y) <= radius.y) &&
				(glm::abs(p.z - center.z) <= radius.z);
		}

		constexpr float MinSide() const
		{
			const glm::vec3 extents = Extents();
			return glm::min(extents.x, glm::min(extents.y, extents.z));
		}

		constexpr float MaxSide() const
		{
			const glm::vec3 extents = Extents();
			return glm::max(extents.x, glm::max(extents.y, extents.z));
		}

		constexpr bool operator== (const AABB& other) const
		{
			return Min == other.Min && Max == other.Max;
		}

		static bool Overlap(const AABB& a, const AABB& b)
		{
			return (a.Min.x <= b.Max.x && a.Max.x >= b.Min.x) &&
				(a.Min.y <= b.Max.y && a.Max.y >= b.Min.y) &&
				(a.Min.z <= b.Max.z && a.Max.z >= b.Min.z);
		}
	};
}
