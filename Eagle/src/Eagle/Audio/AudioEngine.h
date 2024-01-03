#pragma once

#include "glm/glm.hpp"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/Timestep.h"

namespace FMOD
{
	class System;
	class Sound;
	class Channel;
	class Reverb3D;
}

namespace Eagle
{
	//@ MaxChannels. The maximum number of channels to allocate. This controls how many sounds you are able to play simultaneously
	struct AudioEngineSettings
	{
		int MaxChannels = 1024;
	};
	struct SoundSettings;
	enum class RollOffModel;

	class AudioEngine
	{
	public:
		static void Init(const AudioEngineSettings& settings = {});
		static void Shutdown();

		static void Update(Timestep ts);
		static void SetListenerData(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

	private:
		AudioEngine() = default;
		static FMOD::System* GetSystem();
		static bool PlaySound(FMOD::Sound* sound, FMOD::Channel** channel);
		static uint32_t CreateSoundFromBuffer(const DataBuffer& buffer, uint32_t playMode, FMOD::Sound** sound);
		static bool CreateReverb(FMOD::Reverb3D** reverb);

	private:
		friend class Audio;
		friend class Sound;
		friend class Sound2D;
		friend class Sound3D;
		friend class SoundGroup;
		friend class Reverb3D;
	};
}
