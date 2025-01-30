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
		static constexpr size_t s_InvalidIndex = size_t(-1);

		Ref<AssetParticleSystem> m_Asset;
		std::vector<ParticleEmitter> m_Emitters;
		size_t m_SelectedEmitterIndex = s_InvalidIndex;
		bool bGuizmoChanged = false;
	};
}
