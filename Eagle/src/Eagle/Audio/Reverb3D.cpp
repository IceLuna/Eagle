#include "egpch.h"
#include "Reverb3D.h"
#include "AudioEngine.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	static FMOD_REVERB_PROPERTIES ToFMODPreset(ReverbPreset preset)
	{
		switch (preset)
		{
			case ReverbPreset::Generic: return FMOD_PRESET_GENERIC;
			case ReverbPreset::PaddedCell: return FMOD_PRESET_PADDEDCELL;
			case ReverbPreset::Room: return FMOD_PRESET_ROOM;
			case ReverbPreset::Bathroom: return FMOD_PRESET_BATHROOM;
			case ReverbPreset::LivingRoom: return FMOD_PRESET_LIVINGROOM;
			case ReverbPreset::StoneRoom: return FMOD_PRESET_STONEROOM;
			case ReverbPreset::Auditorium: return FMOD_PRESET_AUDITORIUM;
			case ReverbPreset::ConcertHall: return FMOD_PRESET_CONCERTHALL;
			case ReverbPreset::Cave: return FMOD_PRESET_CAVE;
			case ReverbPreset::Arena: return FMOD_PRESET_ARENA;
			case ReverbPreset::Hangar: return FMOD_PRESET_HANGAR;
			case ReverbPreset::CarpettedHallway: return FMOD_PRESET_CARPETTEDHALLWAY;
			case ReverbPreset::Hallway: return FMOD_PRESET_HALLWAY;
			case ReverbPreset::StoneCorridor: return FMOD_PRESET_STONECORRIDOR;
			case ReverbPreset::Alley: return FMOD_PRESET_ALLEY;
			case ReverbPreset::Forest: return FMOD_PRESET_FOREST;
			case ReverbPreset::City: return FMOD_PRESET_CITY;
			case ReverbPreset::Mountains: return FMOD_PRESET_MOUNTAINS;
			case ReverbPreset::Quarry: return FMOD_PRESET_QUARRY;
			case ReverbPreset::Plain: return FMOD_PRESET_PLAIN;
			case ReverbPreset::ParkingLot: return FMOD_PRESET_PARKINGLOT;
			case ReverbPreset::SewerPipe: return FMOD_PRESET_SEWERPIPE;
			case ReverbPreset::UnderWater: return FMOD_PRESET_UNDERWATER;
			default: return FMOD_PRESET_OFF;
		}
	}

	Reverb3D::Reverb3D(ReverbPreset preset) : m_Preset(preset)
	{
		if (AudioEngine::CreateReverb(&m_Reverb))
			SetPreset(preset);
	}

	Reverb3D::Reverb3D(const Reverb3D& other)
	: m_MinDistance(other.m_MinDistance)
	, m_MaxDistance(other.m_MaxDistance)
	, m_Position(other.m_Position)
	, m_Preset(other.m_Preset)
	, m_IsActive(other.m_IsActive)
	{
		if (AudioEngine::CreateReverb(&m_Reverb))
		{
			SetPreset(m_Preset);
			SetMinMaxDistance(m_MinDistance, m_MaxDistance); // Also updates position
			SetActive(m_IsActive);
		}
	}

	Reverb3D::~Reverb3D()
	{
		m_Reverb->release();
		m_Reverb = nullptr;
	}

	Reverb3D& Reverb3D::operator=(const Reverb3D& other)
	{
		m_MinDistance = other.m_MinDistance;
		m_MaxDistance = other.m_MaxDistance;
		m_Position = other.m_Position;
		m_Preset = other.m_Preset;
		m_IsActive = other.m_IsActive;

		SetPreset(m_Preset);
		SetMinMaxDistance(m_MinDistance, m_MaxDistance); // Also updates position
		SetActive(m_IsActive);
		return *this;
	}
	
	void Reverb3D::SetPreset(ReverbPreset preset)
	{
		m_Preset = preset;
		FMOD_REVERB_PROPERTIES props = ToFMODPreset(m_Preset);
		m_Reverb->setProperties(&props);
	}
	
	void Reverb3D::SetActive(bool bActive)
	{
		m_IsActive = bActive;
		m_Reverb->setActive(bActive);
	}

	void Reverb3D::SetMinDistance(float minDistance)
	{
		SetMinMaxDistance(minDistance, m_MaxDistance);
	}

	void Reverb3D::SetMaxDistance(float maxDistance)
	{
		SetMinMaxDistance(m_MinDistance, maxDistance);
	}

	void Reverb3D::SetMinMaxDistance(float minDistance, float maxDistance)
	{
		m_MinDistance = minDistance;
		m_MaxDistance = maxDistance;
		m_Reverb->set3DAttributes((FMOD_VECTOR*)&m_Position.x, m_MinDistance, m_MaxDistance);
	}
	
	void Reverb3D::SetPosition(const glm::vec3& position)
	{
		m_Position = position;
		m_Reverb->set3DAttributes((FMOD_VECTOR*)&m_Position.x, m_MinDistance, m_MaxDistance);
	}
}