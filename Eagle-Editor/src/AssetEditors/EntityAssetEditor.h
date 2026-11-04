#pragma once

#include "AssetEditor.h"
#include "../Panels/SceneHierarchyPanel.h"
#include "../Widgets/SelectionGuizmo.h"
#include "../Widgets/SelectionVisuals.h"
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
		void OnViewportEnd() override { UpdateGuizmo(); }
		void SetupViewportToolbar(ViewportToolbar& toolbar) override;

		bool HandleEntitySelection(MouseButtonPressedEvent& e);
		void OnEntityChanged();

		void BuildDefaultLayout(ImGuiID dockspaceID) override;

	private:
		const EditorLayer& m_EditorLayer;
		Ref<AssetEntity> m_Asset;
		SceneHierarchyPanel m_SceneHierarchy;
		SelectionGuizmo m_SelectionGuizmo;
		SelectionOutline m_SelectionOutline;
		AssetEditTracker<AssetEntity, NoAssetState> m_AssetSettingsTracker;
		int m_GuizmoMode = 1; // ImGuizmo::WORLD
		GuizmoPivot m_GuizmoPivot = GuizmoPivot::ActiveEntity;
		GuizmoGroupTransform m_GuizmoGroupTransform = GuizmoGroupTransform::AroundPivot;
		bool m_UpdateAnims = true;

		Entity m_Entity;
	};
}
