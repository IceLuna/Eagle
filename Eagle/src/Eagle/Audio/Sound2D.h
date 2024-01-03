#pragma once

#include "Sound.h"
#include "Eagle/Core/DataBuffer.h"

namespace Eagle
{
	class Sound2D : public Sound
	{
	public:
		virtual void SetLooping(bool bLooping) override;
		virtual void SetStreaming(bool bStreaming) override;

		virtual void Play() override;

	protected:
		Sound2D(const Ref<Audio>& audio, const SoundSettings& settings = {})
			: Sound(audio, settings) {}

	public:
		static Ref<Sound2D> Create(const Ref<Audio>& audio, const SoundSettings& settings = {});
	};
}