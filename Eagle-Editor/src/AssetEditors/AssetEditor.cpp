#include "egpch.h"
#include "AssetEditor.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Input/Input.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Core/Scene.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"
#include "../EditorLayer.h"
#include "../ImOGuizmo.h"

#include <imgui/imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	AssetEditor::AssetEditor(bool bNeedRenderer, bool bNeedSkybox)
	{
		if (bNeedRenderer)
		{
			// TODO: Basic renderer settings should be controled by the asset editors otherwise some effect won't be visible
			SceneRendererSettings settings = SceneRendererSettings::GetBasicSettings();
			m_Renderer = MakeRef<SceneRenderer>(glm::uvec2{ 1, 1 }, settings);
			m_Scene = MakeRef<Scene>("AssetEditor", m_Renderer);
			m_Scene->SetSkyboxEnabled(bNeedSkybox);
			m_Scene->SetRenderSkybox(false);
			if (bNeedSkybox)
				AddSkybox();

			m_CurrentScene = m_Scene;
		}
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	}

	AssetEditor::~AssetEditor()
	{
		if (m_SimulationScene)
			m_SimulationScene->OnRuntimeStop();

		m_CurrentScene.reset();
		m_Scene.reset();
		m_SimulationScene.reset();
		m_Renderer.reset();
	}

	void AssetEditor::DrawViewport(bool bForceAnimUpdate, const std::string_view parentName)
	{
		if (!m_CurrentScene)
			return;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);

		if (m_ViewportWindowName.empty())
			m_ViewportWindowName = Utils::AsString(GetAsset()->GetPath()) + "_Viewport";
		bViewportVisible = ImGui::Begin(m_ViewportWindowName.c_str());
		HandleFirstWindowRender(m_ViewportWindowName, parentName);

		if (bViewportVisible)
		{
			{
				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();

				m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
			}

			auto& renderer = m_CurrentScene->GetSceneRenderer();
			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
			auto viewportSize = glm::uvec2(viewportPanelSize.x, viewportPanelSize.y);
			if (renderer->GetViewportSize() != viewportSize)
				m_CurrentScene->OnViewportResize(viewportSize.x, viewportSize.y);

			m_CurrentScene->OnUpdate(Application::Get().GetTimestep(), true, bForceAnimUpdate);
			const auto& render = renderer->GetOutput();
			const auto& size = render->GetSize();
			UI::Image(render, ImVec2(float(size.x), float(size.y)));

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				ImGui::SetWindowFocus();

			// When we're moving the camera, we hide the mouse and disable ImGui mouse inputs, and `IsWindowHovered` starts returning false
			// which prevents us from changing the camera speed. So if the mouse is not visible, don't change the state
			if (Input::IsMouseVisible())
				bViewportHovered = ImGui::IsWindowHovered();
			bViewportFocused = ImGui::IsWindowFocused();
			HandleCameraFocus();
			DrawOGuizmo();
		}

		OnViewportEnd();
		ImGui::End();
	}

	bool AssetEditor::DrawGuizmo(Transform& transform, bool bEnabled, bool bWorld)
	{
		if (!m_CurrentScene || m_GuizmoType == -1)
			return false;
		
		ImGuizmo::PushID(m_CurrentScene.get());

		bool bChanged = false;
		const bool bWasEnabled = ImGuizmo::IsEnabled();

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

		//Camera
		const auto& editorCamera = m_CurrentScene->EditorCamera;
		const auto runtimeCamera = m_CurrentScene->GetRuntimeCamera();
		glm::mat4 cameraProjection = m_SimulationScene ? runtimeCamera->Camera.GetUnreversedProjection() : editorCamera.GetUnreversedProjection();
		const glm::mat4& cameraViewMatrix = m_SimulationScene ? runtimeCamera->GetViewMatrix() : editorCamera.GetViewMatrix();
		const bool bProjectionFlipped = m_SimulationScene ? runtimeCamera->Camera.IsProjectionFlipped() : editorCamera.IsProjectionFlipped();
		if (bProjectionFlipped)
			cameraProjection[1][1] *= -1.f; // Since in Vulkan [1][1] of Projection is flipped, we need to flip it back for Guizmo

		int snappingIndex = 0;
		if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
			snappingIndex = 1;
		else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
			snappingIndex = 2;

		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

		//Snapping
		const glm::vec3& snapping = EditorLayer::Get()->GetSnappingValues();
		const float snapValues[3] = { snapping[snappingIndex], snapping[snappingIndex], snapping[snappingIndex] };
		const bool bSnap = Input::IsKeyPressed(Key::LeftShift);

		ImGuizmo::Enable(bEnabled);
		ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
			bWorld ? ImGuizmo::WORLD : ImGuizmo::LOCAL, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);
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
		ImGuizmo::PopID();
		return bChanged;
	}

	void AssetEditor::DrawOGuizmo()
	{
		if (!m_CurrentScene)
			return;

		auto& editorCamera = m_CurrentScene->EditorCamera;
		glm::mat4 cameraProjection = editorCamera.GetProjection();
		glm::mat4 cameraViewMatrix = editorCamera.GetViewMatrix();
		const bool bProjectionFlipped = editorCamera.IsProjectionFlipped();
		if (bProjectionFlipped)
			cameraProjection[1][1] *= -1.f; // Since in Vulkan [1][1] of Projection is flipped, we need to flip it back for Guizmo

		const float shortestSide = glm::min(m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

		const float size = shortestSide * 0.1f; // Size is ~100 per 1000pixels
		const float halfSize = size * 0.5f;
		const float x = m_ViewportBounds[0].x + halfSize;
		const float y = m_ViewportBounds[1].y - size * 1.5f;
		if (ImOGuizmo::Render(x, y, size, glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection)))
		{
			editorCamera.SetTransform(Math::DecomposeTransformMatrix(glm::inverse(cameraViewMatrix)));
		}
	}

	void AssetEditor::SetSimulationEnabled(bool bEnabled)
	{
		if (!m_CurrentScene || (bEnabled == bool(m_SimulationScene)))
			return;

		if (bEnabled)
		{
			m_SimulationScene = MakeRef<Scene>(m_Scene, "Asset Editor. Simulation Scene");
			m_SimulationScene->OnRuntimeStart();
			m_CurrentScene = m_SimulationScene;
		}
		else
		{
			m_SimulationScene->OnRuntimeStop();
			m_SimulationScene.reset();
			m_CurrentScene = m_Scene;
		}
		m_CurrentScene->SetEverythingDirty();
	}

	void AssetEditor::HandleFirstWindowRender(std::string_view windowName, std::string_view parentName)
	{
		const bool bFirstUseEver = (ImGui::GetCurrentWindow()->SetWindowDockAllowFlags & ImGuiCond_FirstUseEver) == ImGuiCond_FirstUseEver;

		if (bFirstUseEver && !parentName.empty())
		{
			ImGuiID parent_node = ImGui::DockBuilderAddNode();
			ImGui::DockBuilderSetNodePos(parent_node, ImGui::GetWindowPos());
			ImGui::DockBuilderSetNodeSize(parent_node, ImGui::GetWindowSize());
			ImGuiID nodeDetails; // Main window
			ImGuiID nodeViewport;
			ImGui::DockBuilderSplitNode(parent_node, ImGuiDir_Left, 0.5f, &nodeViewport, &nodeDetails);

			ImGui::DockBuilderDockWindow(parentName.data(), nodeDetails);
			ImGui::DockBuilderDockWindow(windowName.data(), nodeViewport);

			// Disable tab bar for the viewport
			if (ImGuiDockNode* dock = ImGui::DockContextFindNodeByID(GImGui, nodeViewport))
			{
				dock->SetLocalFlags(ImGuiDockNodeFlags_NoTabBar);
			}

			ImGui::SetWindowSize(ImVec2(720.f * 2.f, 560.f));
		}
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

		if (ImGuiWindow* window = ImGui::FindWindowByName(m_ViewportWindowName.c_str()))
			ImGui::FocusWindow(window);
	}
	
	void AssetEditor::OnEvent(Event& e)
	{
		if (bViewportVisible && bViewportFocused && bViewportHovered)
			m_CurrentScene->OnEventEditor(e);

		Event::Dispatch<KeyPressedEvent>(e, EG_BIND_FN(AssetEditor::OnKeyPressed));
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

	std::string AssetEditor::GetAssetWindowName(const Ref<Asset>& asset)
	{
		return Utils::AsString(asset->GetPath().stem()) + "##" + std::to_string(asset->GetGUID().GetHash());
	}
	
	void AssetEditor::HandleCameraFocus()
	{
		if (m_SimulationScene)
		{
			CameraComponent* camera = m_SimulationScene->GetRuntimeCamera();
			const bool bHasCameraMovement = camera->Parent.HasComponent<NativeScriptComponent>();
			if (bViewportVisible)
			{
				if (ImGui::IsMouseReleased(1) || !bViewportFocused)
				{
					// Disable camera movement
					if (bHasCameraMovement)
					{
						camera->Parent.RemoveComponent<NativeScriptComponent>();
					}
				}
				else if (bHasCameraMovement || (bViewportHovered && ImGui::IsMouseClicked(1, true)))
				{
					// Enable camera movement
					if (!bHasCameraMovement)
					{
						camera->Parent.AddComponent<NativeScriptComponent>().Bind<CameraController>();
					}
				}
			}
			else
			{
				// Disable camera movement
				if (bHasCameraMovement)
				{
					camera->Parent.RemoveComponent<NativeScriptComponent>();
				}
			}
		}
		else
		{
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
	}

	glm::ivec2 AssetEditor::GetMousePosWithinViewport() const
	{
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;

		return glm::ivec2(mx, my);
	}
}
