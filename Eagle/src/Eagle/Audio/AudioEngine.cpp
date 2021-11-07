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
	
	FMOD::System* AudioEngine::GetSystem()
	{
		return s_CoreData.System;
	}
}
