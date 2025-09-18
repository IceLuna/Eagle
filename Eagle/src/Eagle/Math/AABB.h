#pragma once

#include "Eagle/Math/Math.h"

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

		void Transform(const Eagle::Transform& transform)
		{
#if 0 // Apply rotation
			// All 8 corners of the original AABB
			glm::vec3 corners[8] =
			{
				{ box.Min.x, box.Min.y, box.Min.z },
				{ box.Max.x, box.Min.y, box.Min.z },
				{ box.Min.x, box.Max.y, box.Min.z },
				{ box.Max.x, box.Max.y, box.Min.z },
				{ box.Min.x, box.Min.y, box.Max.z },
				{ box.Max.x, box.Min.y, box.Max.z },
				{ box.Min.x, box.Max.y, box.Max.z },
				{ box.Max.x, box.Max.y, box.Max.z }
			};

			AABB result;

			// Transform all corners and expand new AABB
			for (int i = 0; i < 8; i++)
			{
				glm::vec3 transformed = glm::vec3(transform * glm::vec4(corners[i], 1.0f));
				result.Min = glm::min(result.Min, transformed);
				result.Max = glm::max(result.Max, transformed);
			}

			return result;
#else
			// Discarding rotation
			Eagle::Transform temp(transform.Location, Rotator{}, transform.Scale3D);
			const glm::mat4 tr = Math::ToTransformMatrix(temp);
			Min = tr * glm::vec4(Min, 1.f);
			Max = tr * glm::vec4(Max, 1.f);
#endif
		}

		static AABB Transformed(const AABB& aabb, const Eagle::Transform& transform)
		{
			AABB result = aabb;
			result.Transform(transform);
			return result;
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

		constexpr bool IsValid() const
		{
			return Min != glm::vec3(std::numeric_limits<float>::max()) && Max != glm::vec3(std::numeric_limits<float>::lowest());
		}

		static bool Overlap(const AABB& a, const AABB& b)
		{
			return (a.Min.x <= b.Max.x && a.Max.x >= b.Min.x) &&
				(a.Min.y <= b.Max.y && a.Max.y >= b.Min.y) &&
				(a.Min.z <= b.Max.z && a.Max.z >= b.Min.z);
		}
	};
}
