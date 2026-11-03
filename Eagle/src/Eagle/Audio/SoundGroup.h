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

	struct SoundGroupSettings
	{
		float Volume = 1.f;
		float Pitch = 1.f;
		bool bPaused = false;
		bool bMuted = false;

		bool operator==(const SoundGroupSettings& other) const = default;
	};

	//@ SoundGroup has default master group. You don't need to add groups and sounds to master group because that's done automatically.
	class SoundGroup : virtual public std::enable_shared_from_this<SoundGroup>
	{
	public:
		SoundGroup();

		virtual ~SoundGroup();

		FMOD::ChannelGroup* GetFMODGroup() { return m_ChannelGroup; }
		const FMOD::ChannelGroup* GetFMODGroup() const { return m_ChannelGroup; }

		void Stop();

		const SoundGroupSettings& GetSettings() const { return m_Settings; }
		void SetSettings(const SoundGroupSettings& settings);

		void SetPaused(bool bPaused);
		void SetVolume(float volume);
		void SetMuted(bool bMuted);
		//@ Pitch. Any value between 0 and 10
		void SetPitch(float pitch);

		float GetVolume() const { return m_Settings.Volume; }
		float GetPitch() const { return m_Settings.Pitch; }
		bool IsPaused() const { return m_Settings.bPaused; }
		bool IsMuted() const { return m_Settings.bMuted; }

		static Ref<SoundGroup> Create() { return MakeRef<SoundGroup>(); }

	protected:
		SoundGroup(FMOD::ChannelGroup* channelGroup);

		void AddGroup(const Ref<SoundGroup>& group);

	private:
		FMOD::ChannelGroup* m_ChannelGroup = nullptr;
		SoundGroupSettings m_Settings;
	};
}
