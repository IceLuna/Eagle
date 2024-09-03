#include "egpch.h"
#include "AssetEditor.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Math/Math.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Core/Scene.h"

#include <imgui/imgui_internal.h>

namespace Eagle
{
	AssetEditor::AssetEditor(bool bNeedRenderer, bool bNeedSkybox)
	{
		if (bNeedRenderer)
		{
			SceneRendererSettings settings = SceneRendererSettings::GetBasicSettings();
			m_Renderer = MakeRef<SceneRenderer>(glm::uvec2{ 1, 1 }, settings);
			m_Scene = MakeRef<Scene>("AssetEditor", m_Renderer);
			m_Scene->SetSkyboxEnabled(bNeedSkybox);
			if (bNeedSkybox)
				AddSkybox();
		}
	}

	AssetEditor::~AssetEditor()
	{
		m_Scene.reset();
		m_Renderer.reset();
	}

	void AssetEditor::DrawViewport(bool bForceAnimUpdate)
	{
		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bViewportVisible = ImGui::Begin((GetAsset()->GetPath().u8string() + "_Viewport").c_str());

		if (bViewportVisible)
		{
			{
				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();

				m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
			}

			auto& renderer = m_Scene->GetSceneRenderer();
			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
			auto viewportSize = glm::uvec2(viewportPanelSize.x, viewportPanelSize.y);
			if (renderer->GetViewportSize() != viewportSize)
				m_Scene->OnViewportResize(viewportSize.x, viewportSize.y);

			m_Scene->OnUpdate(Application::Get().GetTimestep(), true, bForceAnimUpdate);
			const auto& render = renderer->GetOutput();
			const auto& size = render->GetSize();
			UI::Image(render, ImVec2(float(size.x), float(size.y)));

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				ImGui::SetWindowFocus();

			bViewportHovered = ImGui::IsWindowHovered();
			bViewportFocused = ImGui::IsWindowFocused();

			if (bViewportVisible)
			{
				if (ImGui::IsMouseReleased(1) || !bViewportFocused)
					m_Scene->bCanUpdateEditorCamera = false;
				else if (m_Scene->bCanUpdateEditorCamera || (bViewportHovered && ImGui::IsMouseClicked(1, true)))
					m_Scene->bCanUpdateEditorCamera = true;
			}
			else
				m_Scene->bCanUpdateEditorCamera = false;
		}

		OnViewportEnd();
		ImGui::End();
	}

	void AssetEditor::AddSkybox()
	{
		m_Skybox = AssetManager::GetPreviewSkybox();
		m_Scene->SetSkybox(m_Skybox);
		m_Scene->SetSkyboxEnabled(true);
		m_Scene->SetUseSkyAsBackground(false);
	}

	void AssetEditor::SetInFocus()
	{
		const auto& asset = GetAsset();
		if (!asset)
			return;

		if (ImGuiWindow* window = ImGui::FindWindowByName(asset->GetPath().u8string().c_str()))
			ImGui::FocusWindow(window);
	}
	
	void AssetEditor::OnEvent(Event& e)
	{
		if (bViewportVisible)
			m_Scene->OnEventEditor(e);
	}
}
