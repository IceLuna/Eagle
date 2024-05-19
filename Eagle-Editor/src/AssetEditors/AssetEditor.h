#pragma once

namespace Eagle
{
	class Asset;
	class Scene;
	class SceneRenderer;
	class AssetTextureCube;

	class AssetEditor
	{
	public:
		AssetEditor(bool bNeedRenderer = false);

		virtual ~AssetEditor();

		// pOpen - In case X button is clicked, this flag will be set to false.
		// pOpen - if nullptr set, windows will not have X button 
		virtual void OnImGuiRender(bool* pOpen) {}

		virtual void SetInFocus();

		virtual void OnEvent(Event& e);

		virtual const Ref<Asset> GetAsset() const = 0;
		void DrawViewport(bool bForceAnimUpdate = false);

		// Called right before ImGui::End() of viewport to allow custom widgets
		virtual void OnViewportEnd() {}

	private:
		void AddSkybox();

	protected:
		Ref<Scene> m_Scene;
		Ref<SceneRenderer> m_Renderer;
		Ref<AssetTextureCube> m_Skybox;
		glm::vec2 m_ViewportBounds[2] = { glm::vec2(), glm::vec2() };

		bool bViewportVisible = false;
		bool bViewportFocused = false;
		bool bViewportHovered = false;
	};
}
