#pragma once

namespace FMOD
{
	class Sound;
	class Channel;
}

namespace Eagle
{
	struct AudioEngineSettings
	{
		int MaxChannels = 100; //The maximum number of channels to allocate. This controls how many sounds you are able to play simultaneously
	};

	//When you stream a sound, you can only have one instance of it playing at any time.
	//This limitation exists because there is only one decode buffer per stream.
	//As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
	//while most sound effects should be loaded into memory
	struct SoundSettings
	{
		float Volume = 1.f; // 0 = Silence; 1 = Max Volume
		int LoopCount = -1; // -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
		bool IsLooping = false;
		bool IsStreaming = false;
		bool Is3D = false;
		bool IsMuted = false;
	};

	class AudioEngine
	{
	public:
		static void Init(const AudioEngineSettings& settings = {});
		static void Shutdown();

		static void Update();

	private:
		AudioEngine() = default;
	};

	class Sound
	{
	public:
		Sound(const std::filesystem::path& path, SoundSettings settings = {});
		~Sound();

		static Ref<Sound> CreateSound(const std::filesystem::path& path, SoundSettings settings = {}) { return MakeRef<Sound>(path, settings); }

		void Play();
		void Stop();
		void SetPaused(bool bPaused);
		void SetPosition(uint32_t ms);
		void SetLoopCount(int loopCount);
		void SetVolume(float volume);
		void SetMuted(bool bMuted);
		bool IsPlaying() const;

	private:
		std::filesystem::path m_SoundPath;
		SoundSettings m_Settings;
		FMOD::Sound* m_Sound = nullptr;
		FMOD::Channel* m_Channel = nullptr;
	};
}