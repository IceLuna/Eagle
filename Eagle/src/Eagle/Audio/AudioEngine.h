#pragma once

namespace FMOD
{
	class System;
}

namespace Eagle
{
	//@ MaxChannels. The maximum number of channels to allocate. This controls how many sounds you are able to play simultaneously
	struct AudioEngineSettings
	{
		int MaxChannels = 100;
	};

	class AudioEngine
	{
	public:
		static void Init(const AudioEngineSettings& settings = {});
		static void Shutdown();

		static void Update();

	private:
		AudioEngine() = default;

		static FMOD::System* GetSystem();

		friend class Sound;
		friend class Sound2D;
		friend class Sound3D;
		friend class SoundGroup;
	};
}
