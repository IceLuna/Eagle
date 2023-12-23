#include "egpch.h"
#include "AudioEngine.h"
#include "Eagle/Utils/PlatformUtils.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"
#include "fmod/fmod_common.h"
#include "Sound2D.h"
#include "Sound3D.h"

namespace Eagle
{
	struct AudioCoreData
	{
		FMOD::System* System = nullptr;
	};

	std::unordered_map<std::string, DataBuffer> AudioEngine::s_LoadedSounds;

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
		for (auto& it : s_LoadedSounds)
			it.second.Release();
		s_LoadedSounds.clear();

		s_CoreData.System->release();
		s_CoreData.System = nullptr;
	}

	bool AudioEngine::CreateSound(const Path& path, uint32_t playMode, FMOD::Sound** sound)
	{
		*sound = nullptr;

		std::string absolutePath = path.u8string();
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("[AudioEngine] Failed to create sound. Filepath doesn't exist: {0}", absolutePath);
			return false;
		}
		
		bool bSuccess = true;
		auto it = s_LoadedSounds.find(absolutePath);
		if (it != s_LoadedSounds.end())
		{
			DataBuffer& data = it->second; 
			auto res = CreateSoundFromBuffer(data, playMode, sound);
			if (res != FMOD_OK)
			{
				bSuccess = false;
				EG_CORE_ERROR("[AudioEngine] Failed to create sound. Audio filepath: {0}. Error: {1}", path, FMOD_ErrorString((FMOD_RESULT)res));
			}
		}
		else
		{
			if (std::filesystem::exists(path))
			{
				DataBuffer data = FileSystem::Read(path);
				auto res = CreateSoundFromBuffer(data, playMode, sound);
				if (res == FMOD_OK)
					s_LoadedSounds.insert({absolutePath, data});
				else
				{
					bSuccess = false;
					EG_CORE_ERROR("[AudioEngine] Failed to create sound. Audio filepath: {0}. Error: {1}", path, FMOD_ErrorString((FMOD_RESULT)res));
				}
			}
			else
			{
				bSuccess = false;
				EG_CORE_ERROR("[AudioEngine] Failed to create sound. Audio doesn't exist. Path: {0}", path);
			}
		}

		return bSuccess;
	}

	uint32_t AudioEngine::CreateSoundFromBuffer(const DataBuffer& buffer, uint32_t playMode, FMOD::Sound** sound)
	{
		const char* audioBuffer = (const char*)buffer.Data;
		FMOD_CREATESOUNDEXINFO exinfo;
		memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
		exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		exinfo.length = (uint32_t)buffer.Size;
		auto res = s_CoreData.System->createSound(audioBuffer, FMOD_OPENMEMORY | playMode, &exinfo, sound);
		return res;
	}

	bool AudioEngine::CreateReverb(FMOD::Reverb3D** reverb)
	{
		auto res = s_CoreData.System->createReverb3D(reverb);
		if (res != FMOD_OK)
		{
			EG_CORE_ERROR("[AudioEngine] Failed to create reverb. Error: {0}", FMOD_ErrorString((FMOD_RESULT)res));
			return false;
		}
		return true;
	}

	bool AudioEngine::PlaySound(FMOD::Sound* sound, FMOD::Channel** channel)
	{
		auto res = s_CoreData.System->playSound(sound, nullptr, false, channel);
		return res == FMOD_OK;
	}

	void AudioEngine::Update(Timestep ts)
	{
		s_CoreData.System->update();
	}

	void AudioEngine::SetListenerData(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
	{
		static const FMOD_VECTOR vel = { 0.f, 0.f, 0.f };
		s_CoreData.System->set3DListenerAttributes(0, (FMOD_VECTOR*)&position.x, &vel, (FMOD_VECTOR*)&forward.x, (FMOD_VECTOR*)&up.x);
	}
	
	FMOD::System* AudioEngine::GetSystem()
	{
		return s_CoreData.System;
	}
}
