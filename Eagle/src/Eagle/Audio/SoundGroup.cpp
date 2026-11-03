#include "egpch.h"
#include "SoundGroup.h"

#include "AudioEngine.h"
#include "Sound.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	SoundGroup::SoundGroup()
	{
		AudioEngine::GetSystem()->createChannelGroup("SoundGroup", &m_ChannelGroup);
	}

	SoundGroup::SoundGroup(FMOD::ChannelGroup* channelGroup)
	: m_ChannelGroup(channelGroup)
	{
		m_ChannelGroup->getVolume(&m_Settings.Volume);
		m_ChannelGroup->getPitch(&m_Settings.Pitch);
		m_ChannelGroup->getPaused(&m_Settings.bPaused);
		m_ChannelGroup->getMute(&m_Settings.bMuted);
	}

	SoundGroup::~SoundGroup()
	{
		m_ChannelGroup->release();
	}

	void SoundGroup::AddGroup(const Ref<SoundGroup>& group)
	{
		m_ChannelGroup->addGroup(group->m_ChannelGroup);
	}

	void SoundGroup::Stop()
	{
		m_ChannelGroup->stop();
	}

	void SoundGroup::SetSettings(const SoundGroupSettings& settings)
	{
		m_Settings = settings;
		m_ChannelGroup->setVolume(m_Settings.Volume);
		m_ChannelGroup->setPitch(m_Settings.Pitch);
		m_ChannelGroup->setPaused(m_Settings.bPaused);
		m_ChannelGroup->setMute(m_Settings.bMuted);
	}

	void SoundGroup::SetPaused(bool bPaused)
	{
		SoundGroupSettings settings = m_Settings;
		settings.bPaused = bPaused;
		SetSettings(settings);
	}

	void SoundGroup::SetVolume(float volume)
	{
		SoundGroupSettings settings = m_Settings;
		settings.Volume = volume;
		SetSettings(settings);
	}

	void SoundGroup::SetMuted(bool bMuted)
	{
		SoundGroupSettings settings = m_Settings;
		settings.bMuted = bMuted;
		SetSettings(settings);
	}

	void SoundGroup::SetPitch(float pitch)
	{
		SoundGroupSettings settings = m_Settings;
		settings.Pitch = pitch;
		SetSettings(settings);
	}
}
