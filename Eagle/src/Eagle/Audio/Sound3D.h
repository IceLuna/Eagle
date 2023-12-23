#pragma once

#include "Sound.h"
#include "glm/glm.hpp"

namespace Eagle
{
	enum class RollOffModel
	{
		Linear, Inverse, LinearSquare, InverseTapered,
		Default = Inverse
	};

	class Sound3D : public Sound
	{
	public:
		Sound3D(const Path& path, const glm::vec3& position, RollOffModel rollOff = RollOffModel::Default, const SoundSettings& settings = {});

		void SetPosition(const glm::vec3& position);
		void SetVelocity(const glm::vec3& velocity);
		void SetPositionAndVelocity(const glm::vec3& position, const glm::vec3& velocity);

		glm::vec3 GetWorldPosition() const { return m_SoundData.Position; }
		glm::vec3 GetVelocity() const { return m_SoundData.Velocity; }

		virtual void Play() override;

		//The minimum distance is the point at which the sound starts attenuating.
		//If the listener is any closer to the source than the minimum distance,
		//the sound will play at full volume.
		void SetMinDistance(float minDistance);
		float GetMinDistance() const { return m_SoundData.MinDistance; }

		//The maximum distance is the point at which the sound stops
		//attenuatingand its volume remains constant(a volume which is not
		//necessarily zero)
		void SetMaxDistance(float maxDistance);
		float GetMaxDistance() const { return m_SoundData.MaxDistance; }

		//@ minDistance. The minimum distance is the point at which the sound starts attenuating.
		//If the listener is any closer to the source than the minimum distance,
		//the sound will play at full volume.
		//@ maxDistance. The maximum distance is the point at which the sound stops
		//attenuatingand its volume remains constant(a volume which is not
		//necessarily zero)
		void SetMinMaxDistance(float minDistance, float maxDistance);

		void SetRollOffModel(RollOffModel rollOff);
		RollOffModel GetRollOffModel() const { return m_SoundData.RollOff; }

		virtual void SetLooping(bool bLooping) override;
		virtual void SetStreaming(bool bStreaming) override;

		static Ref<Sound3D> Create(const Path& path, const glm::vec3& position, RollOffModel rollOff = RollOffModel::Default, const SoundSettings& settings = {}) { return MakeRef<Sound3D>(path, position, rollOff, settings); }
		
		friend class AudioComponent;

	protected:
		
		struct Sound3DData
		{
			glm::vec3 Position = glm::vec3(0.f);
			glm::vec3 Velocity = glm::vec3(0.f);
			float MinDistance = 1.f;
			float MaxDistance = 10000.f;
			RollOffModel RollOff;
		};

	protected:
		Sound3DData m_SoundData;
	};
}