#pragma once

#include "AssetEditor.h"
#include "../Panels/SceneHierarchyPanel.h"

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

	private:
		void UpdateGuizmo();
		void OnViewportEnd() override { UpdateGuizmo(); }

		bool OnKeyPressed(KeyPressedEvent& e);
		void OnEntityChanged();

		void HandleFirstWindowRender(std::string_view windowName, std::string_view parentName) override;

	private:
		const EditorLayer& m_EditorLayer;
		Ref<AssetEntity> m_Asset;
		SceneHierarchyPanel m_SceneHierarchy;
		std::string m_WindowName;

		Entity m_Entity;
	};
}
