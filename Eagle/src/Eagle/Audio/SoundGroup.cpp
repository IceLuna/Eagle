#include "egpch.h"
#include "SoundGroup.h"

#include "AudioEngine.h"
#include "Sound.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	SoundGroup::SoundGroup(const std::string& groupName)
	: m_GroupName(groupName)
	{
		AudioEngine::GetSystem()->createChannelGroup(groupName.c_str(), &m_ChannelGroup);
	}

	SoundGroup::SoundGroup(const std::string& groupName, FMOD::ChannelGroup* channelGroup)
	: m_GroupName(groupName)
	, m_ChannelGroup(channelGroup)
	{}

	SoundGroup::~SoundGroup()
	{
		m_ChannelGroup->release();
	}

	void SoundGroup::AddSound(const Ref<Sound>& sound)
	{
		m_Sounds.insert(sound);
		sound->SetSoundGroup(this);
	}

	void SoundGroup::AddGroup(const Ref<SoundGroup>& group)
	{
		m_ChannelGroup->addGroup(group->m_ChannelGroup);
	}

	void SoundGroup::Stop()
	{
		m_ChannelGroup->stop();
	}

	void SoundGroup::SetPaused(bool bPaused)
	{
		m_ChannelGroup->setPaused(bPaused);
	}

	void SoundGroup::SetVolume(float volume)
	{
		m_ChannelGroup->setVolume(volume);
	}

	void SoundGroup::SetMuted(bool bMuted)
	{
		m_ChannelGroup->setMute(bMuted);
	}

	void SoundGroup::SetPitch(float pitch)
	{
		m_ChannelGroup->setPitch(pitch);
	}
	
	Ref<SoundGroup> SoundGroup::GetMasterGroup()
	{
		static FMOD::ChannelGroup* masterChannelGroup = nullptr;
		if (!masterChannelGroup)
			AudioEngine::GetSystem()->getMasterChannelGroup(&masterChannelGroup);
		static Ref<SoundGroup> masterGroup = MakeRef<SoundGroup>("Master", masterChannelGroup);
		return masterGroup;
	}
}
