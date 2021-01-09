#pragma once

namespace Eagle
{
	class Sound
	{
	public:
		static void Init();
		static void playAudio(const char* filename);
		static void Shutdown();
	};
}
