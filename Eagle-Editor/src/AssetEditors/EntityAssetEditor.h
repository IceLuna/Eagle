#pragma once

#include "AssetEditor.h"
#include "../Panels/SceneHierarchyPanel.h"
#include "../Undo/SceneUndo.h"
#include "../Undo/AssetUndo.h"

namespace Eagle
{
	class AssetEntity;
	class EditorLayer;

	class EntityAssetEditor : public AssetEditor
	{
	public:
		EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer);

		void OnImGuiRender(bool* pOpen) override;
		void OnEvent(Event& e) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }
		bool UsesGuizmo() const override { return true; }

	private:
		void UpdateGuizmo();
		bool DrawSelectionGuizmo(); // Returns true if the guizmo is being dragged
		void OnViewportEnd() override { UpdateGuizmo(); }

		bool HandleEntitySelection(MouseButtonPressedEvent& e);
		void OnEntityChanged();

		void BuildDefaultLayout(ImGuiID dockspaceID) override;

	private:
		const EditorLayer& m_EditorLayer;
		Ref<AssetEntity> m_Asset;
		SceneHierarchyPanel m_SceneHierarchy;
		std::optional<SceneTransaction> m_GuizmoTransaction; // Open while the guizmo is being dragged
		AssetEditTracker<AssetEntity, NoAssetState> m_AssetSettingsTracker;
		bool m_UpdateAnims = true;

		Entity m_Entity;
	};
}
