#include "egpch.h"
#include "Sound2D.h"
#include "AudioEngine.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	static FMOD_MODE ToFMODPlayMode(const SoundSettings& settings)
	{
		FMOD_MODE playMode = FMOD_DEFAULT;
		playMode |= settings.IsLooping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
		playMode |= settings.IsStreaming ? FMOD_CREATESTREAM : FMOD_CREATESAMPLE;
		playMode |= FMOD_2D;
		return playMode;
	}

	Ref<Sound2D> Sound2D::Create(const Ref<Audio>& audio, const SoundSettings& settings)
	{
		class LocalSound2D : public Sound2D
		{
		public:
			LocalSound2D(const Ref<Audio>& audio, const SoundSettings& settings = {})
				: Sound2D(audio, settings) {}
		};

		return MakeRef<LocalSound2D>(audio, settings);
	}

	void Sound2D::SetLooping(bool bLooping)
	{
		m_Settings.IsLooping = bLooping;
		auto playMode = ToFMODPlayMode(m_Settings);
		if (m_Channel)
			m_Channel->setMode(playMode);
	}
	
	void Sound2D::SetStreaming(bool bStreaming)
	{
		m_Settings.IsStreaming = bStreaming;
		auto playMode = ToFMODPlayMode(m_Settings);
		if (m_Channel)
			m_Channel->setMode(playMode);
	}
	
	void Sound2D::Play()
	{
		Sound::Play();

		if (m_Channel)
			m_Channel->setMode(ToFMODPlayMode(m_Settings));
	}
}