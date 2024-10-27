#pragma once

#include "AssetEditor.h"
#include "../Panels/EntityPropertiesPanel.h"

namespace Eagle
{
	class AssetEntity;
	class EditorLayer;

	class EntityAssetEditor : public AssetEditor
	{
	public:
		EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		void UpdateGuizmo();
		void OnViewportEnd() override { UpdateGuizmo(); }

		bool OnKeyPressed(KeyPressedEvent& e);
		void OnEntityChanged();

	private:
		const EditorLayer& m_EditorLayer;
		Ref<AssetEntity> m_Asset;
		EntityPropertiesPanel m_EntityProperties;

		Entity m_Entity;
	};
}
