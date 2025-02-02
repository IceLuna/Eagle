#include "egpch.h"
#include "AssetEditor.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Input/Input.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Core/Scene.h"

#include <imgui/imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	AssetEditor::AssetEditor(bool bNeedRenderer, bool bNeedSkybox, bool bSimulate)
	{
		if (bNeedRenderer)
		{
			SceneRendererSettings settings = SceneRendererSettings::GetBasicSettings();
			m_Renderer = MakeRef<SceneRenderer>(glm::uvec2{ 1, 1 }, settings);
			m_Scene = MakeRef<Scene>("AssetEditor", m_Renderer, bSimulate);
			m_Scene->SetSkyboxEnabled(bNeedSkybox);
			m_Scene->SetRenderSkybox(false);
			if (bNeedSkybox)
				AddSkybox();

			if (bSimulate)
			{
				m_Scene->OnRuntimeStart();
			}
		}
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	}

	AssetEditor::~AssetEditor()
	{
		if (m_Scene && m_Scene->IsPlaying())
			m_Scene->OnRuntimeStop();

		m_Scene.reset();
		m_Renderer.reset();
	}

	void AssetEditor::DrawViewport(bool bForceAnimUpdate, const std::string_view parentName)
	{
		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);

		const std::string windowName = GetAsset()->GetPath().u8string() + "_Viewport";
		bViewportVisible = ImGui::Begin(windowName.c_str());
		const bool bFirstUseEver = (ImGui::GetCurrentWindow()->SetWindowDockAllowFlags & ImGuiCond_FirstUseEver) == ImGuiCond_FirstUseEver;

		if (bFirstUseEver && !parentName.empty())
		{
			ImGuiID parent_node = ImGui::DockBuilderAddNode();
			ImGui::DockBuilderSetNodePos(parent_node, ImGui::GetWindowPos());
			ImGui::DockBuilderSetNodeSize(parent_node, ImGui::GetWindowSize());
			ImGuiID nodeA;
			ImGuiID nodeB;
			ImGui::DockBuilderSplitNode(parent_node, ImGuiDir_Right, 0.5f, &nodeB, &nodeA);

			ImGui::DockBuilderDockWindow(parentName.data(), nodeA);
			ImGui::DockBuilderDockWindow(windowName.c_str(), nodeB);

			ImGui::SetWindowSize(ImVec2(720.f * 2.f, 560.f));
		}

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

	bool AssetEditor::DrawGuizmo(Transform& transform, int ID, bool bEnabled)
	{
		if (m_GuizmoType == -1)
			return false;
		
		bool bChanged = false;
		const bool bWasEnabled = ImGuizmo::IsEnabled();

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

		//Camera
		const auto& editorCamera = m_Scene->GetEditorCamera();
		const glm::mat4& cameraViewMatrix = editorCamera.GetViewMatrix();
		glm::mat4 cameraProjection = editorCamera.GetProjection();
		cameraProjection[1][1] *= -1.f; // Since in Vulkan [1][1] of Projection is flipped, we need to flip it back for Guizmo

		const bool bRelative = m_GuizmoType == ImGuizmo::OPERATION::ROTATE;

		int snappingIndex = 0;
		if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
			snappingIndex = 1;
		else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
			snappingIndex = 2;

		//Snapping
		ImGuizmo::SetID(ID);

		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

		ImGuizmo::Enable(bEnabled);
		ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
			ImGuizmo::WORLD, glm::value_ptr(transformMatrix));
		ImGuizmo::Enable(bWasEnabled); // Restore state

		if (ImGuizmo::IsUsing())
		{
			Transform finalTransform = Math::DecomposeTransformMatrix(transformMatrix);

			if (m_GuizmoType == ImGuizmo::OPERATION::TRANSLATE)
				transform.Location = finalTransform.Location;
			else if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				transform.Rotation = finalTransform.Rotation;
			else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
				transform.Scale3D = finalTransform.Scale3D;

			bChanged = true;
		}
		return bChanged;
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

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(AssetEditor::OnKeyPressed));
	}

	bool AssetEditor::OnKeyPressed(KeyPressedEvent& e)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
			return false;

		//Shortcuts
		if (e.GetRepeatCount() > 0)
			return false;

		//Gizmos
		const Key pressedKey = e.GetKey();
		if (bViewportHovered && !ImGuizmo::IsUsing())
		{
			switch (pressedKey)
			{
			case Key::Q:
				m_GuizmoType = -1;
				break;

			case Key::W:
				m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;

			case Key::E:
				m_GuizmoType = ImGuizmo::OPERATION::ROTATE;
				break;

			case Key::R:
				m_GuizmoType = ImGuizmo::OPERATION::SCALE;
				break;
			}
		}
		return false;
	}
}
