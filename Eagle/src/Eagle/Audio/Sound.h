#pragma once

#include "Eagle/Core/DataBuffer.h"

namespace FMOD
{
	class Sound;
	class Channel;
	class ChannelGroup;
	class DSP;
}

namespace Eagle
{
	enum class FFTWindowType
	{
		Rect,
		Triangle,
		Hamming,
		Hanning,
		Blackman,
		BlackmanHarris
	};

	//@ VolumeMultiplier. Gets multiplied by `Audio` volume to determine final volume. 0 = silent, 1 = full. Negative level inverts the signal. Values larger than 1 amplify the signal.
	//@ Pan. -1 = Completely on the left. +1 = Completely on the right
	//@ Pitch. Any value between 0 and 10. Gets multiplied by `Audio` pitch to determine final pitch.
	//@ LoopCount. -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
	//@ FFTSamples. Number of samples in the output of spectrum data. Must be the power of 2 between 64 and 8192
	//@ FFTType. Defines the method that will be used to calculate the spectrum data. https://www.fmod.com/docs/2.00/api/core-api-common-dsp-effects.html#fmod_dsp_fft
	//@ IsStreaming. When you stream a sound, you can only have one instance of it playing at any time.
	//	           This limitation exists because there is only one decode buffer per stream.
	//	           As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
	//	           while most sound effects should be loaded into memory
	//@ bEnableFFT. If set to true, you can get the spectrum data of the sound
	struct SoundSettings
	{
		float VolumeMultiplier = 1.f;
		float Pan = 0.f;
		float Pitch = 1.f;
		int LoopCount = -1;
		uint32_t FFTSamples = 256;
		FFTWindowType FFTType = FFTWindowType::Hamming;
		bool IsLooping = false;
		bool IsStreaming = false;
		bool IsMuted = false;
		bool bEnableFFT = false;
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
		
		void SetSoundGroup(const Ref<SoundGroup>& soundGroup);
		const Ref<SoundGroup>& GetSoundGroup() const { return m_SoundGroup; }

		void SetVolume(float volume);
		float GetVolume() const { return m_Volume; }

		void SetPitch(float pitch);
		float GetPitch() const { return m_Pitch; }

		void SetPan(float pan);
		float GetPan() const { return m_Pan; }

		int GetChannelsCount() const;

		void Play();
		void Stop();
		bool IsPlaying() const;

		static Ref<Audio> Create(const ScopedDataBuffer& buffer, float volume = 1.f);

	protected:
		Audio(const ScopedDataBuffer& buffer, float volume = 1.f);

	private:
		FMOD::Sound* m_Sound = nullptr;
		FMOD::Channel* m_Channel = nullptr;
		Ref<SoundGroup> m_SoundGroup;
		float m_Volume = 1.f;
		float m_Pitch = 1.f;
		float m_Pan = 0.f;
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

		void SetPitch(float pitch);
		float GetPitch() const { return m_Settings.Pitch; }

		void SetMuted(bool bMuted);
		bool IsMuted() const { return m_Settings.IsMuted; }

		void SetPan(float pan);
		float GetPan() const { return m_Settings.Pan; }

		bool IsPlaying() const;

		virtual void SetLooping(bool bLooping) = 0;
		bool IsLooping() const { return m_Settings.IsLooping; }

		virtual void SetStreaming(bool bStreaming) = 0;
		bool IsStreaming() const { return m_Settings.IsLooping; }

		void SetFFTEnabled(bool bEnable);
		bool IsFFTEnabled() const { return m_Settings.bEnableFFT; }

		void SetFFTSamples(uint32_t samples);
		uint32_t GetFFTSamples() const { return m_Settings.FFTSamples; }

		void SetFFTType(FFTWindowType type);
		FFTWindowType GetFFTType() const { return m_Settings.FFTType; }

		// dataCount. Array size of `outData`
		// channelIndex. Allows to get data from a specific audio channel. Starts from 0. If -1, get average over all channels
		bool GetSpectrumData(float* outData, uint32_t dataCount, int channelIndex = -1) const;

		float GetSampleRate() const;
		int GetChannelsCount() const { return m_Audio->GetChannelsCount(); }

		const SoundSettings& GetSettings() const { return m_Settings; }

		const Ref<Audio>& GetAudio() const { return m_Audio; }

	private:
		void CreateDSP();

	protected:
		Sound(const Ref<Audio>& audio, const SoundSettings& settings)
			: m_Audio(audio)
			, m_Settings(settings) {}

	protected:
		Ref<Audio> m_Audio;
		SoundSettings m_Settings;
		FMOD::Channel* m_Channel = nullptr;
		FMOD::DSP* m_DSP = nullptr;
		mutable std::vector<char> m_DSPTempBuffer;

		friend class SoundGroup;
	};
}
