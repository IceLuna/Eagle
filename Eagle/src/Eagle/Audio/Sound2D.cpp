#include "egpch.h"
#include "Sound2D.h"
#include "AudioEngine.h"

#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace Eagle
{
	Sound2D::Sound2D(const std::filesystem::path& path, SoundSettings settings)
		: Sound(path, settings)
	{
		FMOD_MODE playMode = FMOD_DEFAULT;
		playMode |= settings.IsLooping * FMOD_LOOP_NORMAL;
		playMode |= settings.IsStreaming * FMOD_CREATESTREAM;
		playMode |= FMOD_2D;

		AudioEngine::CreateSound(path, playMode, &m_Sound);
	}
}