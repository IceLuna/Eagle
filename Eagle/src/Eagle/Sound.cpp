#include "egpch.h"
#include "Sound.h"
#include "SDL.h"

#include <filesystem>

namespace Eagle
{
	struct SoundData
	{
		SDL_AudioDeviceID deviceId;
		SDL_AudioSpec wavSpec;
		Uint32 wavLength;
		Uint8* wavBuffer;
	};

	enum class SoundState
	{
		None,
		Initialized,
		Shutdown
	};
	
	static SoundData s_Data;
	static SoundState s_State = SoundState::None;

	void Sound::Init()
	{
		if (s_State != SoundState::Initialized)
		{
			SDL_Init(SDL_INIT_AUDIO);
			s_State = SoundState::Initialized;
		}
	}

	void Sound::playAudio(const char* filename)
	{
		EG_CORE_ASSERT(std::filesystem::exists(filename), "AudioFile does not exist!");


		SDL_LoadWAV(filename, &s_Data.wavSpec, &s_Data.wavBuffer, &s_Data.wavLength);

		s_Data.deviceId = SDL_OpenAudioDevice(NULL, 0, &s_Data.wavSpec, NULL, 0);

		int success = SDL_QueueAudio(s_Data.deviceId, s_Data.wavBuffer, s_Data.wavLength);
		if (success == 0)
		{
			EG_CORE_INFO("Playing {0}", filename);
		}
		SDL_PauseAudioDevice(s_Data.deviceId, 0);

	}

	void Sound::Shutdown()
	{
		if (s_State != SoundState::Shutdown)
		{
			if (s_Data.wavBuffer)
			{
				SDL_CloseAudioDevice(s_Data.deviceId);
				SDL_FreeWAV(s_Data.wavBuffer);
				SDL_Quit();
			}
			s_Data.wavBuffer = nullptr;
			s_State = SoundState::Shutdown;
		}
	}
}
