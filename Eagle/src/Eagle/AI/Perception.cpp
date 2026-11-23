#include "egpch.h"
#include "Perception.h"

namespace Eagle::AIPerception
{
	static_assert(magic_enum::enum_count<Sense>() == SenseCount, "Outdated SenseCount");

	static Event MakeEvent(const GUID& target, Sense sense, const glm::vec3& location, float strength, bool bSensed)
	{
		Event event;
		event.Target = target;
		event.Data.Location = location;
		event.Data.Strength = strength;
		event.Data.Type = sense;
		event.Data.bSensed = bSensed;
		return event;
	}

	void Settings::Clamp()
	{
		SightRange = glm::max(SightRange, 0.f);
		LoseSightRange = glm::max(LoseSightRange, SightRange);
		SightHalfAngle = glm::clamp(SightHalfAngle, 0.f, 180.f);
		SightInterval = glm::max(SightInterval, 0.f);
		SightMaxAge = glm::max(SightMaxAge, 0.f);
		HearingRange = glm::max(HearingRange, 0.f);
		HearingMaxAge = glm::max(HearingMaxAge, 0.f);
		DamageMaxAge = glm::max(DamageMaxAge, 0.f);
	}

	bool KnownTarget::IsKnown(uint32_t senses) const
	{
		for (uint32_t i = 0; i < SenseCount; ++i)
		{
			if ((senses & BIT(i)) && Senses[i].bKnown)
				return true;
		}

		return false;
	}

	glm::vec3 KnownTarget::GetLastKnownLocation() const
	{
		const SenseState* latest = nullptr;
		for (const SenseState& sense : Senses)
		{
			if (!sense.bKnown)
				continue;

			if (!latest || sense.bSensed || (!latest->bSensed && sense.Age < latest->Age))
				latest = &sense;
		}

		return latest ? latest->Location : GetSense(Sense::Sight).Location;
	}

	SightCheck CheckSight(const Settings& settings, const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& target, bool bSeen)
	{
		const glm::vec3 toTarget = target - eye;
		const float distance = glm::length(toTarget);
		const float range = bSeen ? settings.LoseSightRange : settings.SightRange;
		if (distance > range)
			return SightCheck::OutOfRange;

		if (distance < 0.0001f || settings.SightHalfAngle >= 180.f)
			return SightCheck::Visible;

		const float cosHalfAngle = glm::cos(glm::radians(settings.SightHalfAngle));
		if (glm::dot(forward, toTarget / distance) < cosHalfAngle)
			return SightCheck::OutOfCone;

		return SightCheck::Visible;
	}

	bool CanHear(const Settings& settings, const glm::vec3& listener, const glm::vec3& noise, float loudness)
	{
		return glm::distance(listener, noise) <= settings.HearingRange * glm::max(loudness, 0.f);
	}

	void Memory::UpdateSight(const std::vector<SightResult>& seen, std::vector<Event>& outEvents)
	{
		for (const SightResult& result : seen)
		{
			KnownTarget& target = FindOrAdd(result.Target);
			SenseState& sight = target.GetSense(Sense::Sight);
			const bool bWasSeen = sight.bSensed;
			sight.Location = result.Location;
			sight.Strength = 1.f;
			sight.Age = 0.f;
			sight.bKnown = true;
			sight.bSensed = true;

			if (!bWasSeen)
				outEvents.push_back(MakeEvent(result.Target, Sense::Sight, result.Location, 1.f, true));
		}

		// Process lost targets
		for (KnownTarget& target : m_Targets)
		{
			SenseState& sight = target.GetSense(Sense::Sight);
			if (!sight.bSensed)
				continue;

			const bool bSeenNow = std::ranges::any_of(seen, [&target](const SightResult& result) { return result.Target == target.Target; });
			if (bSeenNow)
				continue;

			// It's remembered where it was seen last
			sight.bSensed = false;
			sight.Age = 0.f;
			outEvents.push_back(MakeEvent(target.Target, Sense::Sight, sight.Location, sight.Strength, false));
		}
	}

	void Memory::AddStimulus(const GUID& target, Sense sense, const glm::vec3& location, float strength, std::vector<Event>& outEvents)
	{
		EG_CORE_ASSERT(sense != Sense::Sight, "Sight is updated with `UpdateSight`");

		KnownTarget& known = FindOrAdd(target);
		SenseState& state = known.GetSense(sense);
		state.Location = location;
		state.Strength = strength;
		state.Age = 0.f;
		state.bKnown = true;
		state.bSensed = false; // Instant

		outEvents.push_back(MakeEvent(target, sense, location, strength, true));
	}

	void Memory::Update(float ts, const Settings& settings, std::vector<Event>& outEvents)
	{
		for (auto it = m_Targets.begin(); it != m_Targets.end();)
		{
			KnownTarget& target = *it;
			for (uint32_t i = 0; i < SenseCount; ++i)
			{
				SenseState& state = target.Senses[i];
				if (!state.bKnown)
					continue;

				if (state.bSensed)
				{
					state.Age = 0.f;
					continue;
				}

				state.Age += ts;
				const Sense sense = GetSenseByIndex(i);
				if (!settings.DoesRememberForever(sense) && state.Age > settings.GetMaxAge(sense))
					state.bKnown = false;
			}

			if (target.IsKnown())
			{
				++it;
				continue;
			}

			Event event;
			event.Target = target.Target;
			event.bForgotten = true;
			outEvents.push_back(event);
			it = m_Targets.erase(it);
		}
	}

	bool Memory::Forget(const GUID& target)
	{
		return std::erase_if(m_Targets, [&target](const KnownTarget& known) { return known.Target == target; }) > 0;
	}

	const KnownTarget* Memory::Find(const GUID& target) const
	{
		auto it = std::find_if(m_Targets.begin(), m_Targets.end(), [&target](const KnownTarget& known) { return known.Target == target; });
		return it != m_Targets.end() ? &*it : nullptr;
	}

	KnownTarget& Memory::FindOrAdd(const GUID& target)
	{
		auto it = std::find_if(m_Targets.begin(), m_Targets.end(), [&target](const KnownTarget& known) { return known.Target == target; });
		if (it != m_Targets.end())
			return *it;

		KnownTarget& known = m_Targets.emplace_back();
		known.Target = target;
		return known;
	}
}
