#include "egpch.h"
#include "Sound.h"
#include "AudioEngine.h"
#include "SoundGroup.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	Audio::~Audio()
	{
		if (m_Sound)
		{
			m_Sound->release();
			m_Sound = nullptr;
		}
	}

	void Audio::Play()
	{
		FMOD::Channel* channel;
		AudioEngine::PlaySound(m_Sound, &channel);
		channel->setVolume(m_Volume);

		if (const auto& group = GetSoundGroup())
			channel->setChannelGroup(group->GetFMODGroup());
	}

	Ref<Audio> Audio::Create(const DataBuffer& buffer, float volume)
	{
		class LocalAudio : public Audio
		{
		public:
			LocalAudio(const DataBuffer& buffer, float volume) : Audio(buffer, volume) {}
		};

		return MakeRef<LocalAudio>(buffer, volume);
	}

	Audio::Audio(const DataBuffer& buffer, float volume)
		: m_SoundGroup(SoundGroup::GetMasterGroup())
		, m_Volume(volume)
	{
		AudioEngine::CreateSoundFromBuffer(buffer, FMOD_DEFAULT, &m_Sound);
	}

	Sound::~Sound()
	{
		if (IsPlaying())
			Stop();
	}

	void Sound::Play()
	{
		FMOD::Sound* sound = m_Audio->GetFMODSound();
		if (sound && AudioEngine::PlaySound(sound, &m_Channel))
		{
			m_Channel->setLoopCount(m_Settings.LoopCount);
			m_Channel->setVolume(m_Settings.VolumeMultiplier * m_Audio->GetVolume());
			m_Channel->setMute(m_Settings.IsMuted);
			m_Channel->setPan(m_Settings.Pan);

			if (const auto& group = m_Audio->GetSoundGroup())
				m_Channel->setChannelGroup(group->GetFMODGroup());
		}
		else
			EG_CORE_WARN("[AudioEngine] Failed to play the sound");
	}

	void Sound::Stop()
	{
		if (IsPlaying())
		{
			auto res = m_Channel->stop();
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to stop the sound. Error: {}", FMOD_ErrorString(res));
		}
	}

	void Sound::SetPaused(bool bPaused)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPaused(bPaused);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to call 'SetPaused'. Error: {}", FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't call 'SetPaused'. Sound wasn't initialized by calling Sound::Play()");
	}

	bool Sound::IsPaused() const
	{
		if (m_Channel)
		{
			bool bPaused = false;
			auto res = m_Channel->getPaused(&bPaused);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to call 'GetPaused'. Error: {}", FMOD_ErrorString(res));

			return bPaused;
		}
		return false;
	}

	void Sound::SetPosition(uint32_t ms)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPosition(ms, FMOD_TIMEUNIT_MS);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to set sound position. Error: {}", FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't set sound position. Sound wasn't initialized by calling Sound::Play()");
	}

	uint32_t Sound::GetPosition() const
	{
		if (m_Channel)
		{
			uint32_t ms = 0;
			auto res = m_Channel->getPosition(&ms, FMOD_TIMEUNIT_MS);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to get sound position. Error: {}", FMOD_ErrorString(res));

			return ms;
		}
		return 0;
	}

	void Sound::SetLoopCount(int loopCount)
	{
		m_Settings.LoopCount = loopCount;
		if (m_Channel)
			m_Channel->setLoopCount(loopCount);
	}

	void Sound::SetVolumeMultiplier(float volume)
	{
		m_Settings.VolumeMultiplier = volume;
		if (m_Channel)
			m_Channel->setVolume(volume * m_Audio->GetVolume());
	}

	void Sound::SetMuted(bool bMuted)
	{
		m_Settings.IsMuted = bMuted;
		if (m_Channel)
			m_Channel->setMute(bMuted);
	}

	void Sound::SetPan(float pan)
	{
		m_Settings.Pan = pan;
		if (m_Channel)
		{
			auto res = m_Channel->setPan(pan);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to set pan. Error: {}", FMOD_ErrorString(res));
		}
	}

	bool Sound::IsPlaying() const
	{
		if (m_Channel)
		{
			bool res = false;
			m_Channel->isPlaying(&res);
			return res;
		}
		return false;
	}
}
