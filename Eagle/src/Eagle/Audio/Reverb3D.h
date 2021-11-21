#pragma once

#include <glm/glm.hpp>

namespace FMOD
{
	class Reverb3D;
}

namespace Eagle
{
	enum class ReverbPreset
	{
		Generic, PaddedCell, Room, Bathroom, LivingRoom, StoneRoom, Auditorium, ConcertHall,
		Cave, Arena, Hangar, CarpettedHallway, Hallway, StoneCorridor, Alley, Forest, City, Mountains,
		Quarry, Plain, ParkingLot, SewerPipe, UnderWater
	};

	class Reverb3D
	{
	public:
		Reverb3D(ReverbPreset preset = ReverbPreset::Generic);
		Reverb3D(const Reverb3D& other);
		~Reverb3D();

		Reverb3D& operator=(const Reverb3D& other);

		void SetPreset(ReverbPreset preset);
		void SetActive(bool bActive);
		bool IsActive() const { return m_IsActive; }

		void SetMinDistance(float minDistance);
		void SetMaxDistance(float maxDistance);
		void SetMinMaxDistance(float minDistance, float maxDistance);
		
		//Internal use
		void SetPosition(const glm::vec3& position);

		float GetMinDistance() const { return m_MinDistance; }
		float GetMaxDistance() const { return m_MaxDistance; }
		ReverbPreset GetPreset() const { return m_Preset; }

		static Ref<Reverb3D> Create(ReverbPreset preset = ReverbPreset::Generic) { return MakeRef<Reverb3D>(preset); }
		static Ref<Reverb3D> Create(const Ref<Reverb3D>& other) { return MakeRef<Reverb3D>(*other.get()); }

	private:
		FMOD::Reverb3D* m_Reverb = nullptr;
		float m_MinDistance = 0.f; // Reverb is at full volume within that radius
		float m_MaxDistance = 100.f; // Reverb is disabled outside that radius
		glm::vec3 m_Position = glm::vec3{0.f};
		ReverbPreset m_Preset;
		bool m_IsActive = false;
	};
}