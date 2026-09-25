#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Math/Transform.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <vector>

namespace Eagle
{
	// How the segment that STARTS at a key is interpolated.
	// The mode is owned by the left key of a segment
	enum class SequenceInterpolation
	{
		// Value is held until the next key is reached
		Constant,

		// Straight line between the two keys
		Linear,

		// Cubic hermite using automatically computed (Catmull-Rom) tangents
		Smooth,

		// Cubic hermite using the user-authored tangents stored on the keys
		Cubic
	};

	template <typename T>
	struct SequenceKey
	{
		// A stable identity so that UI selection survives re-sorting when keys are dragged past each other.
		GUID ID;

		float Time = 0.f;
		T Value{};

		SequenceInterpolation Interpolation = SequenceInterpolation::Smooth;

		// Only used when `Interpolation == Cubic`. Expressed in units per second.
		T InTangent{};
		T OutTangent{};

		SequenceKey() = default;
		SequenceKey(float time, const T& value, SequenceInterpolation interpolation = SequenceInterpolation::Smooth)
			: Time(time), Value(value), Interpolation(interpolation) {}
	};

	// Value traits. Add a specialization to support a new channel value type
	template <typename T>
	struct SequenceValueTraits
	{
		static T Zero() { return T(0); }

		static T Lerp(const T& a, const T& b, float alpha)
		{
			return a + (b - a) * alpha;
		}

		// Tangent between two keys, in units per second
		static T Slope(const T& a, const T& b, float dt)
		{
			if (dt <= 0.f)
				return Zero();
			return (b - a) / dt;
		}

		static T Hermite(const T& p0, const T& m0, const T& p1, const T& m1, float dt, float alpha)
		{
			const float t = alpha;
			const float t2 = t * t;
			const float t3 = t2 * t;

			const float h00 = 2.f * t3 - 3.f * t2 + 1.f;
			const float h10 = t3 - 2.f * t2 + t;
			const float h01 = -2.f * t3 + 3.f * t2;
			const float h11 = t3 - t2;

			return p0 * h00 + m0 * (h10 * dt) + p1 * h01 + m1 * (h11 * dt);
		}
	};

	// `glm::quat` gets its own specialization because quaternions can't be
	// blended component-wise without the rotation wobbling on the way.
	template <>
	struct SequenceValueTraits<glm::quat>
	{
		static glm::quat Zero() { return glm::quat(1.f, 0.f, 0.f, 0.f); }

		static glm::quat Lerp(const glm::quat& a, const glm::quat& b, float alpha)
		{
			glm::quat to = b;
			// Always take the short way around
			if (glm::dot(a, to) < 0.f)
				to = -to;

			return glm::normalize(glm::slerp(a, to, alpha));
		}

		static glm::quat Slope(const glm::quat&, const glm::quat&, float)
		{
			return Zero();
		}

		// Quaternions are smoothed by easing the blend alpha rather than by building a spline
		// through the tangents. It gives C1-looking (smooth) motion without the instability of a squad
		// implementation, and it can never produce a non-unit rotation.
		static glm::quat Hermite(const glm::quat& p0, const glm::quat&, const glm::quat& p1, const glm::quat&, float, float alpha)
		{
			const float eased = alpha * alpha * (3.f - 2.f * alpha); // smoothstep
			return Lerp(p0, p1, eased);
		}
	};

	// Can't interpolate integer/string based types. Every segment holds its left key
	// whatever the key's interpolation mode is, and the value switches exactly at the next key's time.
	template <>
	struct SequenceValueTraits<bool>
	{
		static bool Zero() { return false; }
		static bool Lerp(bool a, bool b, float alpha) { return alpha < 1.f ? a : b; }
		static bool Slope(bool, bool, float) { return false; }
		static bool Hermite(bool p0, bool, bool p1, bool, float, float alpha) { return alpha < 1.f ? p0 : p1; }
	};

	template <>
	struct SequenceValueTraits<int32_t>
	{
		static int32_t Zero() { return 0; }
		static int32_t Lerp(int32_t a, int32_t b, float alpha) { return alpha < 1.f ? a : b; }
		static int32_t Slope(int32_t, int32_t, float) { return 0; }
		static int32_t Hermite(int32_t p0, int32_t, int32_t p1, int32_t, float, float alpha) { return alpha < 1.f ? p0 : p1; }
	};

	template <>
	struct SequenceValueTraits<uint32_t>
	{
		static uint32_t Zero() { return 0; }
		static uint32_t Lerp(uint32_t a, uint32_t b, float alpha) { return alpha < 1.f ? a : b; }
		static uint32_t Slope(uint32_t, uint32_t, float) { return 0; }
		static uint32_t Hermite(uint32_t p0, uint32_t, uint32_t p1, uint32_t, float, float alpha) { return alpha < 1.f ? p0 : p1; }
	};

	template <>
	struct SequenceValueTraits<GUID>
	{
		static GUID Zero() { return GUID(0, 0); }
		static GUID Lerp(const GUID& a, const GUID& b, float alpha) { return alpha < 1.f ? a : b; }
		static GUID Slope(const GUID&, const GUID&, float) { return Zero(); }
		static GUID Hermite(const GUID& p0, const GUID&, const GUID& p1, const GUID&, float, float alpha) { return alpha < 1.f ? p0 : p1; }
	};

	template <>
	struct SequenceValueTraits<std::string>
	{
		static std::string Zero() { return ""; }
		static std::string Lerp(const std::string& a, const std::string& b, float alpha) { return alpha < 1.f ? a : b; }
		static std::string Slope(const std::string&, const std::string&, float) { return Zero(); }
		static std::string Hermite(const std::string& p0, const std::string&, const std::string& p1, const std::string&, float, float alpha) { return alpha < 1.f ? p0 : p1; }
	};

	template <typename T>
	class SequenceChannel
	{
	public:
		using KeyType = SequenceKey<T>;
		using Traits = SequenceValueTraits<T>;

		bool IsEmpty() const { return m_Keys.empty(); }
		size_t GetKeysCount() const { return m_Keys.size(); }

		const std::vector<KeyType>& GetKeys() const { return m_Keys; }
		std::vector<KeyType>& GetKeys() { return m_Keys; }

		const KeyType& GetKey(size_t index) const { return m_Keys[index]; }
		KeyType& GetKey(size_t index) { return m_Keys[index]; }

		void Clear() { m_Keys.clear(); }

		// Returns the index of the newly inserted key.
		// If a key already exists at (almost) the same time, that key is overwritten instead.
		size_t AddKey(float time, const T& value, SequenceInterpolation interpolation = SequenceInterpolation::Smooth)
		{
			if (KeyType* existing = FindKeyAtTime(time))
			{
				existing->Value = value;
				existing->Interpolation = interpolation;
				return size_t(existing - m_Keys.data());
			}

			KeyType key(time, value, interpolation);
			return InsertKey(key);
		}

		size_t InsertKey(const KeyType& key)
		{
			const auto it = std::upper_bound(m_Keys.begin(), m_Keys.end(), key.Time,
				[](float time, const KeyType& other) { return time < other.Time; });

			const size_t index = size_t(it - m_Keys.begin());
			m_Keys.insert(it, key);
			return index;
		}

		bool RemoveKey(const GUID& id)
		{
			for (auto it = m_Keys.begin(); it != m_Keys.end(); ++it)
			{
				if (it->ID == id)
				{
					m_Keys.erase(it);
					return true;
				}
			}
			return false;
		}

		void RemoveKeyAt(size_t index)
		{
			if (index < m_Keys.size())
				m_Keys.erase(m_Keys.begin() + index);
		}

		KeyType* FindKey(const GUID& id)
		{
			for (auto& key : m_Keys)
				if (key.ID == id)
					return &key;
			return nullptr;
		}

		const KeyType* FindKey(const GUID& id) const
		{
			for (const auto& key : m_Keys)
				if (key.ID == id)
					return &key;
			return nullptr;
		}

		bool GetKeyIndex(const GUID& id, size_t* outIndex) const
		{
			for (size_t i = 0; i < m_Keys.size(); ++i)
			{
				if (m_Keys[i].ID == id)
				{
					if (outIndex)
						*outIndex = i;
					return true;
				}
			}
			return false;
		}

		KeyType* FindKeyAtTime(float time, float tolerance = 1e-4f)
		{
			for (auto& key : m_Keys)
				if (glm::abs(key.Time - time) <= tolerance)
					return &key;
			return nullptr;
		}

		// Call after mutating key times directly (for example while dragging keys in the timeline).
		void SortKeys()
		{
			std::stable_sort(m_Keys.begin(), m_Keys.end(),
				[](const KeyType& a, const KeyType& b) { return a.Time < b.Time; });
		}

		float GetFirstKeyTime() const { return m_Keys.empty() ? 0.f : m_Keys.front().Time; }
		float GetLastKeyTime() const { return m_Keys.empty() ? 0.f : m_Keys.back().Time; }

		// @defaultValue. Returned when the channel has no keys at all
		T Evaluate(float time, const T& defaultValue) const
		{
			const size_t count = m_Keys.size();
			if (count == 0)
				return defaultValue;

			if (count == 1 || time <= m_Keys.front().Time)
				return m_Keys.front().Value;

			if (time >= m_Keys.back().Time)
				return m_Keys.back().Value;

			// Last key whose time is <= `time`
			size_t index = 0;
			{
				const auto it = std::upper_bound(m_Keys.begin(), m_Keys.end(), time,
					[](float t, const KeyType& other) { return t < other.Time; });
				index = size_t(it - m_Keys.begin());
				index = index > 0 ? index - 1 : 0;
			}

			if (index + 1 >= count)
				return m_Keys.back().Value;

			const KeyType& left = m_Keys[index];
			const KeyType& right = m_Keys[index + 1];

			const float dt = right.Time - left.Time;
			if (dt <= 0.f)
				return right.Value;

			const float alpha = glm::clamp((time - left.Time) / dt, 0.f, 1.f);
			switch (left.Interpolation)
			{
				case SequenceInterpolation::Constant:
					return left.Value;

				case SequenceInterpolation::Linear:
					return Traits::Lerp(left.Value, right.Value, alpha);

				case SequenceInterpolation::Smooth:
				case SequenceInterpolation::Cubic:
				{
					const T m0 = GetOutTangent(index);
					const T m1 = GetInTangent(index + 1);
					return Traits::Hermite(left.Value, m0, right.Value, m1, dt, alpha);
				}
			}

			return Traits::Lerp(left.Value, right.Value, alpha);
		}

		// Tangent leaving key `index`, in units per second.
		// Honours the key's own mode so that mixing Linear and Smooth keys doesn't overshoot.
		T GetOutTangent(size_t index) const
		{
			if (index >= m_Keys.size())
				return Traits::Zero();

			const KeyType& key = m_Keys[index];
			if (key.Interpolation == SequenceInterpolation::Cubic)
				return key.OutTangent;

			if (key.Interpolation == SequenceInterpolation::Constant)
				return Traits::Zero();

			if (key.Interpolation == SequenceInterpolation::Linear)
			{
				if (index + 1 < m_Keys.size())
					return Traits::Slope(key.Value, m_Keys[index + 1].Value, m_Keys[index + 1].Time - key.Time);
				return Traits::Zero();
			}

			return GetAutoTangent(index);
		}

		// Tangent arriving at key `index`, in units per second
		T GetInTangent(size_t index) const
		{
			if (index >= m_Keys.size())
				return Traits::Zero();

			const KeyType& key = m_Keys[index];
			if (key.Interpolation == SequenceInterpolation::Cubic)
				return key.InTangent;

			if (key.Interpolation == SequenceInterpolation::Constant)
				return Traits::Zero();

			if (key.Interpolation == SequenceInterpolation::Linear)
			{
				if (index > 0)
					return Traits::Slope(m_Keys[index - 1].Value, key.Value, key.Time - m_Keys[index - 1].Time);
				return Traits::Zero();
			}

			return GetAutoTangent(index);
		}

		// Catmull-Rom style tangent through the neighbours of `index`.
		// End keys fall back to the one-sided slope so the curve leaves/enters cleanly.
		T GetAutoTangent(size_t index) const
		{
			const size_t count = m_Keys.size();
			if (count < 2 || index >= count)
				return Traits::Zero();

			if (index == 0)
				return Traits::Slope(m_Keys[0].Value, m_Keys[1].Value, m_Keys[1].Time - m_Keys[0].Time);

			if (index == count - 1)
				return Traits::Slope(m_Keys[count - 2].Value, m_Keys[count - 1].Value, m_Keys[count - 1].Time - m_Keys[count - 2].Time);

			const KeyType& prev = m_Keys[index - 1];
			const KeyType& next = m_Keys[index + 1];
			return Traits::Slope(prev.Value, next.Value, next.Time - prev.Time);
		}

	private:
		std::vector<KeyType> m_Keys; // Always sorted by time
	};

	using SequenceBoolChannel = SequenceChannel<bool>;
	using SequenceIntChannel = SequenceChannel<int32_t>;
	using SequenceUIntChannel = SequenceChannel<uint32_t>;
	using SequenceFloatChannel = SequenceChannel<float>;
	using SequenceVec2Channel = SequenceChannel<glm::vec2>;
	using SequenceVec3Channel = SequenceChannel<glm::vec3>;
	using SequenceGUIDChannel = SequenceChannel<GUID>;
	using SequenceStringChannel = SequenceChannel<std::string>;
	using SequenceQuatChannel = SequenceChannel<glm::quat>;

	using SequenceChannelVariant = std::variant
		<SequenceBoolChannel, SequenceIntChannel, SequenceUIntChannel, SequenceFloatChannel,
		SequenceVec2Channel, SequenceVec3Channel, SequenceGUIDChannel, SequenceStringChannel>;
}
