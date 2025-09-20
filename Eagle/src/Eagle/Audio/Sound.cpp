#include "egpch.h"
#include "Sound.h"
#include "AudioEngine.h"
#include "SoundGroup.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	static FMOD_DSP_FFT_WINDOW ToFMODType(FFTWindowType type)
	{
		switch (type)
		{
		case Eagle::FFTWindowType::Rect:
			return FMOD_DSP_FFT_WINDOW_RECT;
		case Eagle::FFTWindowType::Triangle:
			return FMOD_DSP_FFT_WINDOW_TRIANGLE;
		case Eagle::FFTWindowType::Hamming:
			return FMOD_DSP_FFT_WINDOW_HAMMING;
		case Eagle::FFTWindowType::Hanning:
			return FMOD_DSP_FFT_WINDOW_HANNING;
		case Eagle::FFTWindowType::Blackman:
			return FMOD_DSP_FFT_WINDOW_BLACKMAN;
		case Eagle::FFTWindowType::BlackmanHarris:
			return FMOD_DSP_FFT_WINDOW_BLACKMANHARRIS;
		default:
			EG_CORE_ASSERT(false);
			EG_CORE_ERROR("Unknown FFTWindowType");
			return FMOD_DSP_FFT_WINDOW_RECT;
		}
	}

	Audio::Audio(const ScopedDataBuffer& buffer, float volume)
		: m_SoundGroup(SoundGroup::GetMasterGroup())
		, m_Volume(volume)
	{
		AudioEngine::CreateSoundFromBuffer(buffer.GetDataBuffer(), FMOD_DEFAULT, &m_Sound);
	}

	Audio::~Audio()
	{
		if (m_Sound)
		{
			m_Sound->release();
			m_Sound = nullptr;
		}
	}

	void Audio::SetSoundGroup(const Ref<SoundGroup>& soundGroup)
	{
		m_SoundGroup = soundGroup;
		if (m_Channel)
			m_Channel->setChannelGroup(m_SoundGroup ? m_SoundGroup->GetFMODGroup() : nullptr);
	}

	void Audio::SetVolume(float volume)
	{
		m_Volume = volume;
		if (m_Channel)
			m_Channel->setVolume(m_Volume);
	}

	void Audio::SetPitch(float pitch)
	{
		m_Pitch = std::clamp(pitch, 0.f, 10.f);
		if (m_Channel)
			m_Channel->setPitch(m_Pitch);
	}

	void Audio::SetPan(float pan)
	{
		m_Pan = pan;
		if (m_Channel)
			m_Channel->setPan(m_Pan);
	}

	int Audio::GetChannelsCount() const
	{
		int channels = 0;
		m_Sound->getFormat(nullptr, nullptr, &channels, nullptr);
		return channels;
	}

	void Audio::Play()
	{
		AudioEngine::PlaySound(m_Sound, &m_Channel);
		m_Channel->setVolume(m_Volume);
		m_Channel->setPitch(m_Pitch);
		m_Channel->setPan(m_Pan);

		if (const auto& group = GetSoundGroup())
			m_Channel->setChannelGroup(group->GetFMODGroup());
	}

	void Audio::Stop()
	{
		if (IsPlaying())
		{
			auto res = m_Channel->stop();
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to stop the audio. Error: {}", FMOD_ErrorString(res));
		}
	}

	bool Audio::IsPlaying() const
	{
		bool bPlaying = false;

		if (m_Channel)
			m_Channel->isPlaying(&bPlaying);
		
		return bPlaying;
	}

	Ref<Audio> Audio::Create(const ScopedDataBuffer& buffer, float volume)
	{
		class LocalAudio : public Audio
		{
		public:
			LocalAudio(const ScopedDataBuffer& buffer, float volume) : Audio(buffer, volume) {}
		};

		return MakeRef<LocalAudio>(buffer, volume);
	}

	Sound::~Sound()
	{
		if (IsPlaying())
			Stop();
	}

	void Sound::Play()
	{
		FMOD::Sound* sound = m_Audio->GetFMODSound();
		if (sound && AudioEngine::PlaySound(sound, &m_Channel))
		{
			m_Channel->setLoopCount(m_Settings.LoopCount);
			m_Channel->setVolume(m_Settings.VolumeMultiplier * m_Audio->GetVolume());
			m_Channel->setMute(m_Settings.IsMuted);
			m_Channel->setPan(m_Settings.Pan);

			if (const auto& group = m_Audio->GetSoundGroup())
				m_Channel->setChannelGroup(group->GetFMODGroup());

			if (IsFFTEnabled())
			{
				CreateDSP();
				m_Channel->addDSP(0, m_DSP);
			}
		}
		else
			EG_CORE_WARN("[AudioEngine] Failed to play the sound");
	}

	void Sound::Stop()
	{
		if (IsPlaying())
		{
			auto res = m_Channel->stop();
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to stop the sound. Error: {}", FMOD_ErrorString(res));

			if (m_DSP)
			{
				m_DSP->release();
				m_DSP = nullptr;
			}
		}
	}

	void Sound::SetPaused(bool bPaused)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPaused(bPaused);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to call 'SetPaused'. Error: {}", FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't call 'SetPaused'. Sound wasn't initialized by calling Sound::Play()");
	}

	bool Sound::IsPaused() const
	{
		if (m_Channel)
		{
			bool bPaused = false;
			auto res = m_Channel->getPaused(&bPaused);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to call 'GetPaused'. Error: {}", FMOD_ErrorString(res));

			return bPaused;
		}
		return false;
	}

	void Sound::SetPosition(uint32_t ms)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPosition(ms, FMOD_TIMEUNIT_MS);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to set sound position. Error: {}", FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't set sound position. Sound wasn't initialized by calling Sound::Play()");
	}

	uint32_t Sound::GetPosition() const
	{
		if (m_Channel)
		{
			uint32_t ms = 0;
			auto res = m_Channel->getPosition(&ms, FMOD_TIMEUNIT_MS);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to get sound position. Error: {}", FMOD_ErrorString(res));

			return ms;
		}
		return 0;
	}

	void Sound::SetLoopCount(int loopCount)
	{
		m_Settings.LoopCount = loopCount;
		if (m_Channel)
			m_Channel->setLoopCount(loopCount);
	}

	void Sound::SetVolumeMultiplier(float volume)
	{
		m_Settings.VolumeMultiplier = volume;
		if (m_Channel)
			m_Channel->setVolume(volume * m_Audio->GetVolume());
	}

	void Sound::SetPitch(float pitch)
	{
		m_Settings.Pitch = std::clamp(pitch, 0.f, 10.f);
		if (m_Channel)
			m_Channel->setPitch(std::clamp(pitch * m_Audio->GetPitch(), 0.f, 10.f));
	}

	void Sound::SetMuted(bool bMuted)
	{
		m_Settings.IsMuted = bMuted;
		if (m_Channel)
			m_Channel->setMute(bMuted);
	}

	void Sound::SetPan(float pan)
	{
		m_Settings.Pan = pan;
		if (m_Channel)
		{
			auto res = m_Channel->setPan(pan);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to set pan. Error: {}", FMOD_ErrorString(res));
		}
	}

	bool Sound::IsPlaying() const
	{
		if (m_Channel)
		{
			bool res = false;
			m_Channel->isPlaying(&res);
			return res;
		}
		return false;
	}
	
	void Sound::SetFFTEnabled(bool bEnable)
	{
		if (m_Settings.bEnableFFT == bEnable)
			return;

		m_Settings.bEnableFFT = bEnable;
		if (bEnable)
		{
			CreateDSP();
			if (m_Channel)
				m_Channel->addDSP(0, m_DSP);
		}
		else
		{
			if (m_Channel)
				m_Channel->removeDSP(m_DSP);
			m_DSP->release();
			m_DSP = nullptr;
		}
	}
	
	void Sound::SetFFTSamples(uint32_t samples)
	{
		if (m_Settings.FFTSamples == samples)
			return;

		uint32_t power = 0u;
		if (samples > m_Settings.FFTSamples)
			power = (uint32_t)std::ceil(std::log2(samples));
		else
			power = (uint32_t)std::floor(std::log2(samples));
		samples = (uint32_t)glm::pow(2u, power);
		samples = glm::clamp(samples, 64u, 8192u);

		m_Settings.FFTSamples = samples;
		if (m_DSP)
		{
			m_DSP->setParameterInt(FMOD_DSP_FFT_WINDOWSIZE, int(m_Settings.FFTSamples * 2u));
			m_DSPTempBuffer.resize(m_Settings.FFTSamples * 2u);
		}
	}
	
	void Sound::SetFFTType(FFTWindowType type)
	{
		if (m_Settings.FFTType == type)
			return;
		
		m_Settings.FFTType = type;
		if (m_DSP)
			m_DSP->setParameterInt(FMOD_DSP_FFT_WINDOWTYPE, ToFMODType(m_Settings.FFTType));
	}

	bool Sound::GetSpectrumData(float* outData, uint32_t dataCount, int channelIndex) const
	{
		if (!IsFFTEnabled())
		{
			EG_CORE_ERROR("[AudioEngine] Failed to get spectrum data. FFT is not enabled");
			return false;
		}

		const int channelsCount = m_Audio->GetChannelsCount();
		if (channelIndex >= channelsCount)
		{
			EG_CORE_ERROR("[AudioEngine] Failed to get spectrum data. Invalid channel index: {}. Channels count is: {}", channelIndex, channelsCount);
			return false;
		}

		FMOD_DSP_PARAMETER_FFT* fftparameter = nullptr;
		uint32_t length = 0;

		auto res = m_DSP->getParameterData(FMOD_DSP_FFT_SPECTRUMDATA, (void**)&fftparameter, &length, m_DSPTempBuffer.data(), int(m_DSPTempBuffer.size()));
		if (res != FMOD_OK)
		{
			EG_CORE_WARN("[AudioEngine] Failed to get spectrum data. Error: {}", FMOD_ErrorString(res));
			return false;
		}

		if (fftparameter->numchannels == 0 || fftparameter->numchannels <= channelIndex)
		{
			EG_CORE_WARN("[AudioEngine] Failed to get spectrum data. Probably, FFT is not ready yet");
			return false;
		}

		const uint32_t numSamples = std::min(dataCount, m_Settings.FFTSamples);
		if (channelIndex == -1)
		{
			for (uint32_t i = 0; i < numSamples; ++i)
			{
				float totalChannelData = 0.f;
				for (int c = 0; c < fftparameter->numchannels; ++c)
					totalChannelData += fftparameter->spectrum[c][i];

				outData[i] = totalChannelData / fftparameter->numchannels;
			}
		}
		else
		{
			for (uint32_t i = 0; i < numSamples; ++i)
			{
				outData[i] = fftparameter->spectrum[channelIndex][i];
			}
		}

		return true;
	}

	float Sound::GetSampleRate() const
	{
		float sampleRate = 0.f;
		m_Audio->GetFMODSound()->getDefaults(&sampleRate, nullptr);
		return sampleRate;
	}
	
	void Sound::CreateDSP()
	{
		if (m_DSP)
			return;

		AudioEngine::GetSystem()->createDSPByType(FMOD_DSP_TYPE_FFT, &m_DSP);
		m_DSP->setActive(true);
		m_DSP->setParameterInt(FMOD_DSP_FFT_WINDOWTYPE, ToFMODType(m_Settings.FFTType));
		m_DSP->setParameterInt(FMOD_DSP_FFT_WINDOWSIZE, int(m_Settings.FFTSamples * 2u));
		m_DSPTempBuffer.resize(m_Settings.FFTSamples * 2u);
	}
}
