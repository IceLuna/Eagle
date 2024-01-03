#pragma once

namespace FMOD
{
	class System;
	class Sound;
	class ChannelGroup;
}

namespace Eagle
{
	class Sound;
	class Audio;

	//@ SoundGroup has default master group. You don't need to add groups and sounds to master group because that's done automatically.
	class SoundGroup : virtual public std::enable_shared_from_this<SoundGroup>
	{
	public:
		SoundGroup(const std::string& groupName);

		virtual ~SoundGroup();

		FMOD::ChannelGroup* GetFMODGroup() { return m_ChannelGroup; }
		const FMOD::ChannelGroup* GetFMODGroup() const { return m_ChannelGroup; }
		
		void AddAudio(const Ref<Audio>& audio);
		void AddGroup(const Ref<SoundGroup>& group);
		const std::string& GetName() const { return m_GroupName; }

		void Stop();
		void SetPaused(bool bPaused);
		void SetVolume(float volume);
		void SetMuted(bool bMuted);
		//@ Pitch. Any value between 0 and 10
		void SetPitch(float pitch);

		static Ref<SoundGroup> Create(const std::string& groupName) { return MakeRef<SoundGroup>(groupName); }
		static Ref<SoundGroup> GetMasterGroup();

	protected:
		SoundGroup(const std::string& groupName, FMOD::ChannelGroup* channelGroup);

	private:
		std::unordered_set<Ref<Audio>> m_Audios;
		std::string m_GroupName;
		FMOD::ChannelGroup* m_ChannelGroup = nullptr;

		friend class Sound;
	};
}
