#pragma once

#include "../EditorResources.h"

#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class Asset;
	class Scene;
	class SceneRenderer;
	class AssetTextureCube;

	class AssetEditor
	{
	public:
		AssetEditor(bool bNeedRenderer = false, bool bNeedSkybox = true);

		virtual ~AssetEditor();

		// pOpen - In case X button is clicked, this flag will be set to false.
		// pOpen - if nullptr set, windows will not have X button 
		virtual void OnImGuiRender(bool* pOpen) {}

		virtual void SetInFocus();

		virtual void OnEvent(Event& e);

		virtual const Ref<Asset> GetAsset() const = 0;
		void DrawViewport(bool bForceAnimUpdate = false, const std::string_view parentName = "");

		// Called right before ImGui::End() of viewport to allow custom widgets
		virtual void OnViewportEnd() {}

		bool DrawGuizmo(Transform& transform, bool bEnabled, bool bWorld = true);
		int GetGuizmoType() const { return m_GuizmoType; }

		void DrawOGuizmo();

		void SetSimulationEnabled(bool bEnabled);

		const Ref<Scene>& GetCurrentScene() const { return m_CurrentScene; }

	protected:
		virtual void HandleFirstWindowRender(std::string_view windowName, std::string_view parentName);

		void HandleCameraFocus();
		glm::ivec2 GetMousePosWithinViewport() const;

		static std::string GetAssetWindowName(const Ref<Asset>& asset, std::string_view ending = "");
		static ImVec2 GetDefaultWindowSize() { return ImVec2(920.f, 760.f); }

	private:
		void AddSkybox();
		bool OnKeyPressed(KeyPressedEvent& e);

	private:
		Ref<Scene> m_Scene;
		Ref<Scene> m_SimulationScene;
		Ref<Scene> m_CurrentScene;

	protected:
		std::string m_ViewportWindowName;
		Ref<SceneRenderer> m_Renderer;
		Ref<AssetTextureCube> m_Skybox;
		glm::vec2 m_ViewportBounds[2] = { glm::vec2(), glm::vec2() };
		int m_GuizmoType = 7; // TRANSLATE;

		bool bViewportVisible = false;
		bool bViewportFocused = false;
		bool bViewportHovered = false;
		bool bUsingImGuizmoOrHovered = false;
	};
}
