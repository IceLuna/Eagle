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
	{}

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

	bool SoundGroup::IsPaused() const
	{
		bool value = false;
		m_ChannelGroup->getPaused(&value);
		return value;
	}

	float SoundGroup::GetVolume() const
	{
		float value = 0.f;
		m_ChannelGroup->getVolume(&value);
		return value;
	}

	bool SoundGroup::IsMuted() const
	{
		bool value = false;
		m_ChannelGroup->getMute(&value);
		return value;
	}

	float SoundGroup::GetPitch() const
	{
		float value = 0.f;
		m_ChannelGroup->getPitch(&value);
		return value;
	}
	
	Ref<SoundGroup> SoundGroup::GetMasterGroup()
	{
		class LocalSoundGroup : public SoundGroup
		{
		public:
			LocalSoundGroup(FMOD::ChannelGroup* channelGroup)
				: SoundGroup(channelGroup) {}
		};

		static Ref<SoundGroup> masterGroup;
		if (!masterGroup)
		{
			FMOD::ChannelGroup* masterChannelGroup = nullptr;
			AudioEngine::GetSystem()->getMasterChannelGroup(&masterChannelGroup);

			masterGroup = MakeRef<LocalSoundGroup>(masterChannelGroup);
		}

		return masterGroup;
	}
}
