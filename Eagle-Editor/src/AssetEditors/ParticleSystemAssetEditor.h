#pragma once

#include "Eagle/Renderer/ParticleEmitter.h"

#include "AssetEditor.h"

namespace Eagle
{
	class AssetParticleSystem;
	class ParticleSystemComponent;

	class ParticleSystemAssetEditor : public AssetEditor
	{
	public:
		ParticleSystemAssetEditor(const Ref<AssetParticleSystem>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		void OnViewportEnd() override { UpdateGuizmo(); }
		void UpdateGuizmo();

	private:
		Ref<AssetParticleSystem> m_Asset;
		ParticleSystemComponent* m_Component = nullptr; // Not owning
		std::vector<ParticleEmitter> m_Emitters;
		ParticleEmitter* m_SelectedEmitter = nullptr;
		bool bGuizmoChanged = false;
	};
}
