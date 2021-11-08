#include "egpch.h"
#include "Sound.h"
#include "AudioEngine.h"
#include "SoundGroup.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	Sound::~Sound()
	{
		if (IsPlaying())
			Stop();

		m_Sound->release();
		m_Sound = nullptr;
	}

	void Sound::Play()
	{
		auto res = m_System->playSound(m_Sound, nullptr, false, &m_Channel);
		if (res != FMOD_OK)
			EG_CORE_WARN("[AudioEngine] Failed to play sound. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));

		m_Channel->setLoopCount(m_Settings.LoopCount);
		m_Channel->setVolume(m_Settings.Volume);
		m_Channel->setMute(m_Settings.IsMuted);
		res = m_Channel->setPan(m_Settings.Pan);
		SetSoundGroup(m_SoundGroup);

		if (res != FMOD_OK)
			EG_CORE_WARN("[AudioEngine] Failed to set pan. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
	}

	void Sound::Stop()
	{
		if (m_Channel)
		{
			auto res = m_Channel->stop();
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to stopp sound. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't stop sound. Sound wasn't initialized by calling Sound::Play(). Path: {0}", m_SoundPath);
	}

	void Sound::SetPaused(bool bPaused)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPaused(bPaused);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to call 'SetPaused'. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't call 'SetPaused'. Sound wasn't initialized by calling Sound::Play(). Path: {0}", m_SoundPath);
	}

	void Sound::SetPosition(uint32_t ms)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPosition(ms, FMOD_TIMEUNIT_MS);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to set sound position. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't set sound position. Sound wasn't initialized by calling Sound::Play(). Path: {0}", m_SoundPath);
	}

	void Sound::SetLoopCount(int loopCount)
	{
		m_Settings.LoopCount = loopCount;
		if (m_Channel)
			m_Channel->setLoopCount(loopCount);
	}

	void Sound::SetVolume(float volume)
	{
		m_Settings.Volume = volume;
		if (m_Channel)
			m_Channel->setVolume(volume);
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
				EG_CORE_WARN("[AudioEngine] Failed to set pan. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
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
	
	void Sound::SetSoundGroup(const SoundGroup* soundGroup)
	{
		m_SoundGroup = soundGroup;

		if (m_Channel && m_SoundGroup)
			m_Channel->setChannelGroup(m_SoundGroup->m_ChannelGroup);
	}
}
