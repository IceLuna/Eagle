#pragma once

#include "Sound.h"

namespace Eagle
{
	class Sound2D : public Sound
	{
	public:
		virtual void SetLooping(bool bLooping) override;
		virtual void SetStreaming(bool bStreaming) override;

		Sound2D(const Path& path, const SoundSettings& settings = {});

		static Ref<Sound2D> Create(const Path& path, const SoundSettings& settings = {}) { return MakeRef<Sound2D>(path, settings); }
	};
}