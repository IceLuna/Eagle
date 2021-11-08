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

		m_System = AudioEngine::GetSystem();

		auto res = m_System->createSound(path.u8string().c_str(), playMode, 0, &m_Sound);
		if (res != FMOD_OK)
			EG_CORE_ERROR("[AudioEngine] Failed to create sound. Audio filepath: {0}. Error: {1}", path, FMOD_ErrorString(res));
	}
}