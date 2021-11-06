#include "egpch.h"
#include "AudioEngine.h"
#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	struct AudioCoreData
	{
		FMOD::System* System = nullptr;
	};

	static AudioCoreData s_CoreData;

	void AudioEngine::Init(const AudioEngineSettings& settings)
	{
		FMOD::System_Create(&s_CoreData.System);
		auto result = s_CoreData.System->init(settings.MaxChannels, FMOD_INIT_NORMAL, 0);
		if (result != FMOD_OK)
		{
			EG_CORE_CRITICAL("[AudioEngine] Failed to init Audio System. Error: {0}", FMOD_ErrorString(result));
			EG_CORE_ASSERT(false, "Audio init failure");
		}
	}
	
	void AudioEngine::Shutdown()
	{
		s_CoreData.System->release();
		s_CoreData.System = nullptr;
	}

	void AudioEngine::Update()
	{
		s_CoreData.System->update();
	}

	//Sound class
	Sound::Sound(const std::filesystem::path& path, SoundSettings settings)
	: m_SoundPath(path)
	, m_Settings(settings)
	{
		FMOD_MODE playMode = FMOD_DEFAULT;
		playMode |= settings.IsLooping * FMOD_LOOP_NORMAL;
		playMode |= settings.IsStreaming * FMOD_CREATESTREAM;
		playMode |= settings.Is3D * FMOD_3D;

		auto res = s_CoreData.System->createSound(path.u8string().c_str(), playMode, 0, &m_Sound);
		if (res != FMOD_OK)
			EG_CORE_ERROR("[AudioEngine] Failed to create sound. Audio filepath: {0}. Error: {1}", path, FMOD_ErrorString(res));
	}
	
	Sound::~Sound()
	{
		if (IsPlaying())
			Stop();

		m_Sound->release();
		m_Sound = nullptr;
	}
	
	void Sound::Play()
	{
		auto res = s_CoreData.System->playSound(m_Sound, nullptr, false, &m_Channel);
		if (res != FMOD_OK)
			EG_CORE_WARN("[AudioEngine] Failed to play sound. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		
		m_Channel->setLoopCount(m_Settings.LoopCount);
		m_Channel->setVolume(m_Settings.Volume);
		m_Channel->setMute(m_Settings.bMuted);
	}

	void Sound::Stop()
	{
		if (m_Channel)
		{
			auto res = m_Channel->stop();
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to stopp sound. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't stop sound. Sound wasn't initialized by calling Sound::Play(). Path: {0}", m_SoundPath);
	}
	
	void Sound::SetPaused(bool bPaused)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPaused(bPaused);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to call 'SetPaused'. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't call 'SetPaused'. Sound wasn't initialized by calling Sound::Play(). Path: {0}", m_SoundPath);
	}

	void Sound::SetPosition(uint32_t ms)
	{
		if (m_Channel)
		{
			auto res = m_Channel->setPosition(ms, FMOD_TIMEUNIT_MS);
			if (res != FMOD_OK)
				EG_CORE_WARN("[AudioEngine] Failed to set sound position. Path: {0}. Error: {1}", m_SoundPath, FMOD_ErrorString(res));
		}
		else
			EG_CORE_WARN("[AudioEngine] Can't set sound position. Sound wasn't initialized by calling Sound::Play(). Path: {0}", m_SoundPath);
	}

	void Sound::SetLoopCount(int loopCount)
	{
		m_Settings.LoopCount = loopCount;
		if (m_Channel)
			m_Channel->setLoopCount(loopCount);
	}

	void Sound::SetVolume(float volume)
	{
		m_Settings.Volume = volume;
		if (m_Channel)
			m_Channel->setVolume(volume);
	}

	void Sound::SetMuted(bool bMuted)
	{
		m_Settings.IsMuted = bMuted;
		if (m_Channel)
			m_Channel->setMute(bMuted);
	}
	
	bool Sound::IsPlaying() const
	{
		if (m_Channel)
		{
			bool res = false;
			m_Channel->isPlaying(&res);
			return res;
		}
		else
			return false;
	}
}