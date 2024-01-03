#pragma once

#include "Eagle/Core/DataBuffer.h"

namespace FMOD
{
	class Sound;
	class Channel;
	class ChannelGroup;
}

namespace Eagle
{
	//@ VolumeMultiplier. Gets multiplied by `Audio` volume to determine final volume.
	//@ Pan. -1 = Completely on the left. +1 = Completely on the right
	//@ LoopCount. -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
	//@ IsStreaming. When you stream a sound, you can only have one instance of it playing at any time.
	//	           This limitation exists because there is only one decode buffer per stream.
	//	           As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
	//	           while most sound effects should be loaded into memory
	struct SoundSettings
	{
		float VolumeMultiplier = 1.f;
		float Pan = 0.f;
		int LoopCount = -1;
		bool IsLooping = false;
		bool IsStreaming = false;
		bool IsMuted = false;
	};

	class SoundGroup;

	// It's a wrapper around FMOD::Sound.
	// This class is not used for playing, but rather passed to `Sound` classes to be used
	class Audio
	{
	public:
		virtual ~Audio();

		const FMOD::Sound* GetFMODSound() const { return m_Sound; }
		FMOD::Sound* GetFMODSound() { return m_Sound; }
		
		// Doesn't affect sounds that are already playing
		void SetSoundGroup(const Ref<SoundGroup>& soundGroup) { m_SoundGroup = soundGroup; }
		const Ref<SoundGroup>& GetSoundGroup() const { return m_SoundGroup; }

		// Doesn't affect sounds that are already playing
		void SetVolume(float volume) { m_Volume = volume; }
		float GetVolume() const { return m_Volume; }

		static Ref<Audio> Create(const DataBuffer& buffer, float volume = 1.f);

	protected:
		Audio(const DataBuffer& buffer, float volume = 1.f);

	private:
		FMOD::Sound* m_Sound = nullptr;
		Ref<SoundGroup> m_SoundGroup;
		float m_Volume = 1.f;
	};

	class Sound
	{
	public:
		virtual ~Sound();

		virtual void Play();
		void Stop();

		void SetPaused(bool bPaused);
		bool IsPaused() const;

		void SetPosition(uint32_t ms);
		uint32_t GetPosition() const;

		void SetLoopCount(int loopCount);
		int GetLoopCount() const { return m_Settings.LoopCount; }

		void SetVolumeMultiplier(float volume);
		float GetVolumeMultiplier() const { return m_Settings.VolumeMultiplier; }

		void SetMuted(bool bMuted);
		bool IsMuted() const { return m_Settings.IsMuted; }

		void SetPan(float pan);
		float GetPan() const { return m_Settings.Pan; }

		bool IsPlaying() const;

		virtual void SetLooping(bool bLooping) = 0;
		bool IsLooping() const { return m_Settings.IsLooping; }

		virtual void SetStreaming(bool bStreaming) = 0;
		bool IsStreaming() const { return m_Settings.IsLooping; }

		const SoundSettings& GetSettings() const { return m_Settings; }

		const Ref<Audio>& GetAudio() const { return m_Audio; }

	protected:
		Sound(const Ref<Audio>& audio, const SoundSettings& settings)
			: m_Audio(audio)
			, m_Settings(settings) {}

	protected:
		Ref<Audio> m_Audio;
		SoundSettings m_Settings;
		FMOD::Channel* m_Channel = nullptr;

		friend class SoundGroup;
	};
}
