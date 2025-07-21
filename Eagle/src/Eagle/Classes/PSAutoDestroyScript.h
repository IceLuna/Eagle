#pragma once

#include "Eagle/Core/ScriptableEntity.h"

namespace Eagle
{
	class Asset;

	class PSAutoDestroyScript : public ScriptableEntity
	{
	public:
		PSAutoDestroyScript() = default;
		PSAutoDestroyScript(const PSAutoDestroyScript& other) = delete;
		PSAutoDestroyScript(PSAutoDestroyScript&& other) noexcept;

		PSAutoDestroyScript& operator=(const PSAutoDestroyScript& other) = delete;
		PSAutoDestroyScript& operator=(PSAutoDestroyScript&& other) noexcept;

		void OnCreate() override;
		void OnUpdate(Timestep ts) override;
		void OnDestroy() override;

		void OnAssetChanged();

	private:
		void RecalculateLifetime();

	private:
		Ref<Asset> m_ParticleAsset;
		GUID m_CallbackID = {};
		
		float m_Lifetime = 0.f;
		float m_Timer = 0.f;
	};
}
