#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Physics/PhysicsEngine.h"

#include <glm/glm.hpp>

// AI perception API for what an AI senses (such as sees, hears, gets damaged by).
namespace Eagle::AIPerception
{
	enum class Sense : uint32_t
	{
		Sight   = BIT(0),
		Hearing = BIT(1),
		Damage  = BIT(2),
	};
	inline constexpr uint32_t SenseCount = 3u;
	inline constexpr uint32_t AllSenses = (1u << SenseCount) - 1u;

	inline uint32_t GetSenseIndex(Sense sense)
	{
		switch (sense)
		{
			case Sense::Sight: return 0u;
			case Sense::Hearing: return 1u;
			case Sense::Damage: return 2u;
		}
		EG_CORE_ASSERT(false);
		return 0u;
	}

	inline Sense GetSenseByIndex(uint32_t index) { return Sense(BIT(index)); }

	// Which entities count as a one.
	// For example, the AI's own body that doesn't block its sight
	enum class HierarchyScope : uint32_t
	{
		Self,
		SelfAndChildren,
		PhysicsBody, // The nearest entity from this one up with a rigid body or a character controller, and its children. If there's none, the same as `SelfAndChildren`
	};

	enum class DebugDrawMode : uint32_t
	{
		Off,
		Selected,
		All,
	};

	// Settings of something that has perception.
	// Note: keep in sync with the same struct on C# side.
	// When a new settings is added, don't forget to update `Clamp` function
	struct Settings
	{
		// Sight
		float SightRange = 10.f;
		float LoseSightRange = 12.f; // Something that's seen is lost when it's further than this
		float SightHalfAngle = 60.f; // Degrees from the forward direction. So, 180 means all around
		float SightInterval = 0.2f;  // Seconds between checks. Checks of different AIs are spread over the interval
		float SightMaxAge = 5.f;     // Seconds it's remembered after it's lost
		CollisionGroup SightBlockedBy = s_CollisionGroupAny;

		// Hearing
		float HearingRange = 20.f;
		float HearingMaxAge = 5.f; // Seconds it's remembered after it's heard

		// Damage
		float DamageMaxAge = 10.f; // Seconds it's remembered after it caused damage

		// The AI's own body doesn't trigger/block the sight.
		HierarchyScope IgnoreOwnBody = HierarchyScope::PhysicsBody;

		bool bSight = true;
		bool bHearing = true;
		bool bDamage = true;
		bool bDrawDebug = false;

		// If enabled, the max ages aren't used
		bool bRememberSightForever = false;
		bool bRememberHearingForever = false;
		bool bRememberDamageForever = false;

		bool IsEnabled(Sense sense) const
		{
			switch (sense)
			{
				case Sense::Sight: return bSight;
				case Sense::Hearing: return bHearing;
				case Sense::Damage: return bDamage;
			}

			EG_CORE_ASSERT(false);
			return false;
		}

		float GetMaxAge(Sense sense) const
		{
			switch (sense)
			{
				case Sense::Sight: return SightMaxAge;
				case Sense::Hearing: return HearingMaxAge;
				case Sense::Damage: return DamageMaxAge;
			}

			EG_CORE_ASSERT(false);
			return 0.f;
		}

		bool DoesRememberForever(Sense sense) const
		{
			switch (sense)
			{
				case Sense::Sight: return bRememberSightForever;
				case Sense::Hearing: return bRememberHearingForever;
				case Sense::Damage: return bRememberDamageForever;
			}

			EG_CORE_ASSERT(false);
			return false;
		}

		void Clamp();
	};

	// Settings of something that can be perceived.
	// Note: keep in sync with the same struct on C# side
	struct SourceSettings
	{
		glm::vec3 SightOffset = glm::vec3(0.f); // The point AIs look at, relative to the entity (for example, the chest)
		HierarchyScope SeenWhenRayHits = HierarchyScope::SelfAndChildren;

		bool bSight = true;
	};

	// What was sensed
	struct Stimulus
	{
		glm::vec3 Location = glm::vec3(0.f); // Where it was sensed
		float Strength = 1.f; // Loudness for hearing, the amount for damage, 1 for sight
		Sense Type = Sense::Sight;
		bool bSensed = true; // False when a target is lost from sight
	};

	struct Event
	{
		GUID Target = GUID(0, 0);
		Stimulus Data;
		bool bForgotten = false; // Forgotten by all senses
	};

	struct SenseState
	{
		glm::vec3 Location = glm::vec3(0.f);
		float Strength = 0.f;
		float Age = 0.f;      // Seconds since it was last sensed. 0 while it's sensed
		bool bKnown = false;  // Remembered. Meaning it's sensed and not forgotten yet
		bool bSensed = false; // Sensed right now. Only for sight
	};

	struct KnownTarget
	{
		GUID Target = GUID(0, 0);
		SenseState Senses[SenseCount];

		const SenseState& GetSense(Sense sense) const { return Senses[GetSenseIndex(sense)]; }
		SenseState& GetSense(Sense sense) { return Senses[GetSenseIndex(sense)]; }

		bool IsKnown(uint32_t senses = AllSenses) const;
		bool IsSeen() const { return GetSense(Sense::Sight).bSensed; }

		glm::vec3 GetLastKnownLocation() const;
	};

	struct DebugMarker
	{
		glm::vec3 Location = glm::vec3(0.f);
		float Age = 0.f;
		Sense Type = Sense::Sight;
		bool bSensed = true; // False when sight was lost
	};

	struct DebugNoise
	{
		glm::vec3 Location = glm::vec3(0.f);
		float Loudness = 1.f;
		float Age = 0.f;
	};

	inline constexpr float DebugMarkerLifetime = 3.f;
	inline constexpr uint32_t MaxDebugMarkers = 16u;
	inline constexpr uint32_t MaxDebugNoises = 256u;

	enum class SightCheck
	{
		Visible, // In range and in the cone. A ray decides if it's seen
		OutOfRange,
		OutOfCone,
	};

	// @bSeen. Whether it's seen now, so it's lost further than `LoseSightRange`
	SightCheck CheckSight(const Settings& settings, const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& target, bool bSeen);

	bool CanHear(const Settings& settings, const glm::vec3& listener, const glm::vec3& noise, float loudness);

	// What an AI remembers
	class Memory
	{
	public:
		struct SightResult
		{
			GUID Target = GUID(0, 0);
			glm::vec3 Location = glm::vec3(0.f);
		};

		// The targets seen by a sight check. Targets that were seen and aren't in it are lost
		void UpdateSight(const std::vector<SightResult>& seen, std::vector<Event>& outEvents);

		// Hearing and damage only.
		// `target` can be null to indicate an unknown source.
		// Unknown target replaces the previous one (for each sense)
		void AddStimulus(const GUID& target, Sense sense, const glm::vec3& location, float strength, std::vector<Event>& outEvents);

		void Update(float ts, const Settings& settings, std::vector<Event>& outEvents);

		bool Forget(const GUID& target);
		void ForgetAll() { m_Targets.clear(); }

		template<typename Pred>
		void ForgetIf(Pred&& pred)
		{
			std::erase_if(m_Targets, [&pred](const KnownTarget& target) { return pred(target.Target); });
		}

		const KnownTarget* Find(const GUID& target) const;
		const std::vector<KnownTarget>& GetTargets() const { return m_Targets; }

	private:
		KnownTarget& FindOrAdd(const GUID& target);

	private:
		std::vector<KnownTarget> m_Targets;
	};
}
