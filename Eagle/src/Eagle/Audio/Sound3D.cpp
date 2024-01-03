#include "egpch.h"
#include "Sound3D.h"
#include "AudioEngine.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"
#include "Eagle/Utils/PlatformUtils.h"

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
			case RollOffModel::Inverse: return FMOD_3D_INVERSEROLLOFF;
			case RollOffModel::LinearSquare: return FMOD_3D_LINEARSQUAREROLLOFF;
			case RollOffModel::InverseTapered: return FMOD_3D_INVERSETAPEREDROLLOFF;
		}
		return 0;
	}

	static FMOD_MODE ToFMODPlayMode(const SoundSettings& settings, RollOffModel rollOff)
	{
		FMOD_MODE playMode = FMOD_DEFAULT;
		playMode |= settings.IsLooping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
		playMode |= settings.IsStreaming ? FMOD_CREATESTREAM : FMOD_CREATESAMPLE;
		playMode |= FMOD_3D;
		playMode |= ToFMODRollOffModel(rollOff);
		return playMode;
	}

	Sound3D::Sound3D(const Ref<Audio>& audio, const glm::vec3& position, RollOffModel rollOff, const SoundSettings& settings)
	: Sound(audio, settings)
	{
		FMOD_MODE playMode = ToFMODPlayMode(settings, rollOff);

		m_SoundData.Position = position;
		m_SoundData.RollOff = rollOff;
	}
	
	void Sound3D::Play()
	{
		Sound::Play();

		if (m_Channel)
		{
			FMOD_VECTOR position = ToFMODVector(m_SoundData.Position);
			FMOD_VECTOR velocity = ToFMODVector(m_SoundData.Velocity);
			FMOD_MODE playMode = ToFMODPlayMode(m_Settings, m_SoundData.RollOff);

			m_Channel->set3DAttributes(&position, &velocity);
			m_Channel->set3DMinMaxDistance(m_SoundData.MinDistance, m_SoundData.MaxDistance);
			m_Channel->setMode(playMode);
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

		if (m_Channel)
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
		if (m_Channel)
			m_Channel->set3DMinMaxDistance(minDistance, maxDistance);
	}
	
	void Sound3D::SetRollOffModel(RollOffModel rollOff)
	{
		m_SoundData.RollOff = rollOff;
		auto playMode = ToFMODPlayMode(m_Settings, rollOff);
		if (m_Channel)
			m_Channel->setMode(playMode);
	}
	
	void Sound3D::SetLooping(bool bLooping)
	{
		m_Settings.IsLooping = bLooping;
		auto playMode = ToFMODPlayMode(m_Settings, m_SoundData.RollOff);
		if (m_Channel)
			m_Channel->setMode(playMode);
	}
	
	void Sound3D::SetStreaming(bool bStreaming)
	{
		m_Settings.IsStreaming = bStreaming;
		auto playMode = ToFMODPlayMode(m_Settings, m_SoundData.RollOff);
		if (m_Channel)
			m_Channel->setMode(playMode);
	}
	
	Ref<Sound3D> Sound3D::Create(const Ref<Audio>& audio, const glm::vec3& position, RollOffModel rollOff, const SoundSettings& settings)
	{
		class LocalSound3D : public Sound3D
		{
		public:
			LocalSound3D(const Ref<Audio>& audio, const glm::vec3& position, RollOffModel rollOff = RollOffModel::Default, const SoundSettings& settings = {})
				: Sound3D(audio, position, rollOff, settings) {}
		};

		return MakeRef<LocalSound3D>(audio, position, rollOff, settings);
	}
}