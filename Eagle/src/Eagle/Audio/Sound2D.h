#pragma once

#include "Sound.h"

namespace Eagle
{
	class Sound2D : public Sound
	{
	public:
		Sound2D(const std::filesystem::path& path, SoundSettings settings = {});

		static Ref<Sound2D> Create(const std::filesystem::path& path, SoundSettings settings = {}) { return MakeRef<Sound2D>(path, settings); }
	};
}