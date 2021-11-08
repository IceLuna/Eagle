#include "egpch.h"
#include "Sound3D.h"
#include "AudioEngine.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	static FMOD_VECTOR ToFMODVector(const glm::vec3& vector)
	{
		FMOD_VECTOR position = { vector.x, vector.y, vector.z };
		return position;
	}

	static int ToFMODRollOffModel(RollOffModel model)
	{
		switch (model)
		{
			case RollOffModel::Linear: return FMOD_3D_LINEARROLLOFF;
			case RollOffModel::Logarithmic: return 0; //Because by default FMOD uses Logarithmic
			case RollOffModel::Inverse: return FMOD_3D_INVERSEROLLOFF;
			case RollOffModel::LinearSquare: return FMOD_3D_LINEARSQUAREROLLOFF;
		}
	}

	Sound3D::Sound3D(const std::filesystem::path& path, const glm::vec3& position, RollOffModel rollOff, SoundSettings settings)
	: Sound(path, settings)
	{
		FMOD_MODE playMode = FMOD_DEFAULT;
		playMode |= settings.IsLooping * FMOD_LOOP_NORMAL;
		playMode |= settings.IsStreaming * FMOD_CREATESTREAM;
		playMode |= FMOD_3D;
		playMode |= ToFMODRollOffModel(rollOff);

		m_System = AudioEngine::GetSystem();

		auto res = m_System->createSound(path.u8string().c_str(), playMode, 0, &m_Sound);
		if (res != FMOD_OK)
			EG_CORE_ERROR("[AudioEngine] Failed to create sound. Audio filepath: {0}. Error: {1}", path, FMOD_ErrorString(res));

		m_SoundData.Position = position;
	}
	
	void Sound3D::Play()
	{
		Sound::Play();

		if (m_Channel)
		{
			FMOD_VECTOR position = ToFMODVector(m_SoundData.Position);
			FMOD_VECTOR velocity = ToFMODVector(m_SoundData.Velocity);

			m_Channel->set3DAttributes(&position, &velocity);
			m_Channel->set3DMinMaxDistance(m_SoundData.MinDistance, m_SoundData.MaxDistance);
		}
	}

	void Sound3D::SetPosition(const glm::vec3& position)
	{
		SetPositionAndVelocity(position, m_SoundData.Velocity);
	}

	void Sound3D::SetVelocity(const glm::vec3& velocity)
	{
		SetPositionAndVelocity(m_SoundData.Position, velocity);
	}

	void Sound3D::SetPositionAndVelocity(const glm::vec3& position, const glm::vec3& velocity)
	{
		m_SoundData.Position = position;
		m_SoundData.Velocity = velocity;

		if (IsPlaying())
		{
			FMOD_VECTOR position = ToFMODVector(m_SoundData.Position);
			FMOD_VECTOR velocity = ToFMODVector(m_SoundData.Velocity);

			m_Channel->set3DAttributes(&position, &velocity);
		}
	}

	void Sound3D::SetMinDistance(float minDistance)
	{
		SetMinMaxDistance(minDistance, m_SoundData.MaxDistance);
	}

	void Sound3D::SetMaxDistance(float maxDistance)
	{
		SetMinMaxDistance(m_SoundData.MinDistance, maxDistance);
	}

	void Sound3D::SetMinMaxDistance(float minDistance, float maxDistance)
	{
		m_SoundData.MinDistance = minDistance;
		m_SoundData.MaxDistance = maxDistance;

		if (IsPlaying())
		{
			m_Channel->set3DMinMaxDistance(minDistance, maxDistance);
		}
	}
}