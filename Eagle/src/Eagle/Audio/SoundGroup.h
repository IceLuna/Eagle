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
		SoundGroup();

		virtual ~SoundGroup();

		FMOD::ChannelGroup* GetFMODGroup() { return m_ChannelGroup; }
		const FMOD::ChannelGroup* GetFMODGroup() const { return m_ChannelGroup; }

		void Stop();
		void SetPaused(bool bPaused);
		void SetVolume(float volume);
		void SetMuted(bool bMuted);
		//@ Pitch. Any value between 0 and 10
		void SetPitch(float pitch);

		float GetVolume() const;
		float GetPitch() const;
		bool IsPaused() const;
		bool IsMuted() const;

		static Ref<SoundGroup> Create() { return MakeRef<SoundGroup>(); }
		static Ref<SoundGroup> GetMasterGroup();

	protected:
		SoundGroup(FMOD::ChannelGroup* channelGroup);

		void AddGroup(const Ref<SoundGroup>& group);

	private:
		FMOD::ChannelGroup* m_ChannelGroup = nullptr;
	};
}
