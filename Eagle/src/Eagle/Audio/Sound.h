#pragma once

namespace FMOD
{
	class Sound;
	class Channel;
	class ChannelGroup;
}

namespace Eagle
{
	//@ Volume. 0.0 = Silence; 1.0 = Max Volume
	//@ Pan. -1 = Completely on the left. +1 = Completely on the right
	//@ LoopCount. -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
	//@ IsStreaming. When you stream a sound, you can only have one instance of it playing at any time.
	//	           This limitation exists because there is only one decode buffer per stream.
	//	           As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
	//	           while most sound effects should be loaded into memory
	struct SoundSettings
	{
		float Volume = 1.f;
		float Pan = 0.f;
		int LoopCount = -1;
		bool IsLooping = false;
		bool IsStreaming = false;
		bool IsMuted = false;
	};

	class SoundGroup;

	class Sound
	{
	public:
		virtual ~Sound();

		virtual void Play();
		void Stop();
		void SetPaused(bool bPaused);
		void SetPosition(uint32_t ms);
		void SetLoopCount(int loopCount);
		void SetVolume(float volume);
		void SetMuted(bool bMuted);
		void SetPan(float pan);
		bool IsPlaying() const;
		virtual void SetLooping(bool bLooping) = 0;
		virtual void SetStreaming(bool bStreaming) = 0;

		const std::filesystem::path& GetSoundPath() const { return m_SoundPath; }

	protected:
		Sound(const std::filesystem::path& path, SoundSettings settings) : m_SoundPath(path), m_Settings(settings) {}

		void SetSoundGroup(const SoundGroup* soundGroup);

	protected:
		std::filesystem::path m_SoundPath;
		SoundSettings m_Settings;
		FMOD::Sound* m_Sound = nullptr;
		FMOD::Channel* m_Channel = nullptr;
		const SoundGroup* m_SoundGroup = nullptr;

		friend class SoundGroup;
	};
}
