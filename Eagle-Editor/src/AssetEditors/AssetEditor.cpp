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
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	static const char* s_AlwaysIncludeInGameBuildsHelpMsg = "Game builds only include the assets that the game uses: the startup scene, the assets it references, the assets they reference, and so on.\n"
		"Assets whose paths are written in C# scripts (Asset.Get(\"Content/...\")) are included as well.\n"
		"Enable it if the game loads this asset in a way that can't be detected. "
		"For example, if a script loads it by a path that's built at runtime: Asset.Get(\"Content/Levels/\" + name + \".egasset\").\n"
		"The assets that this asset references are included too";

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

	void AssetEditor::DrawViewport(bool bForceAnimUpdate)
	{
		if (!m_CurrentScene)
			return;

		bViewportVisible = BeginPanel(m_ViewportPanelName, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, true);

		if (bViewportVisible)
		{
			{
				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();

				m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
				// Used by the scene to know which pixel is under the mouse (object picking)
				m_CurrentScene->ViewportBounds[0] = m_ViewportBounds[0];
				m_CurrentScene->ViewportBounds[1] = m_ViewportBounds[1];
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
		
		ImGuizmo::PushID(this);

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

		bUsingImGuizmoOrHovered = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
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

	void AssetEditor::InitEditorWindowNames()
	{
		const Ref<Asset> asset = GetAsset();
		const std::string assetID = std::to_string(asset->GetGUID().GetHash());

		m_EditorWindowName = Utils::AsString(asset->GetPath().stem()) + "###AssetEditor_" + assetID;
		m_DetailsPanelName = GetPanelName(asset, "Details");
		m_ViewportPanelName = GetPanelName(asset, "Viewport");

		// Panes can only be docked with each other, inside this editor's dockspace
		m_PanelWindowClass = ImGuiWindowClass();
		m_PanelWindowClass.ClassId = ImHashStr(m_EditorWindowName.c_str());
		m_PanelWindowClass.DockingAllowUnclassed = false;
	}

	bool AssetEditor::DrawEditorWindow(bool* pOpen)
	{
		if (m_EditorWindowName.empty())
			InitEditorWindowNames();

		// Size & position of the editor when it's opened for the first time. Proportional to the main window, and centered
		{
			constexpr ImVec2 minSize = ImVec2(1120.f, 680.f);
			const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
			const ImVec2 workSize = mainViewport->WorkSize;
			ImVec2 size = ImVec2(ImMax(workSize.x * m_FirstUseSizeRatio.x, minSize.x), ImMax(workSize.y * m_FirstUseSizeRatio.y, minSize.y));
			size = ImVec2(ImMin(size.x, workSize.x * 0.95f), ImMin(size.y, workSize.y * 0.95f));

			ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(mainViewport->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		const bool bVisible = ImGui::Begin(m_EditorWindowName.c_str(), pOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar();

		m_DockSpaceID = ImGui::GetID("AssetEditorDockSpace");
		if (ImGui::DockBuilderGetNode(m_DockSpaceID) == nullptr)
		{
			// No layout yet, build the default one
			const ImVec2 contentSize = ImGui::GetContentRegionAvail();
			const ImVec2 nodeSize = (contentSize.x > 0.f && contentSize.y > 0.f) ? contentSize : ImGui::GetWindowSize();

			ImGui::DockBuilderAddNode(m_DockSpaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(m_DockSpaceID, nodeSize);
			BuildDefaultLayout(m_DockSpaceID);
			ImGui::DockBuilderFinish(m_DockSpaceID);
		}

		UpdateCentralNode();

		// The dockspace must be submitted even when the editor isn't visible,
		// otherwise the panes would be undocked and become floating windows
		const ImGuiDockNodeFlags dockSpaceFlags = bVisible ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly;
		ImGui::DockSpace(m_DockSpaceID, ImVec2(0.f, 0.f), dockSpaceFlags, &m_PanelWindowClass);

		ImGui::End();

		return bVisible;
	}

	bool AssetEditor::BeginPanel(const std::string& panelName, ImGuiWindowFlags flags, bool bNoPadding)
	{
		EG_CORE_ASSERT(m_DockSpaceID != 0, "`DrawEditorWindow` must be called first");

		ImGui::SetNextWindowClass(&m_PanelWindowClass);
		if (bNoPadding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		const bool bVisible = ImGui::Begin(panelName.c_str(), nullptr, flags);
		if (bNoPadding)
			ImGui::PopStyleVar();
		return bVisible;
	}

	void AssetEditor::BuildDefaultLayout(ImGuiID dockspaceID)
	{
		ImGuiID nodeViewport;
		ImGuiID nodeDetails;
		ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, m_FirstUseViewportRatio, &nodeViewport, &nodeDetails);

		ImGui::DockBuilderDockWindow(m_ViewportPanelName.c_str(), nodeViewport);
		ImGui::DockBuilderDockWindow(m_DetailsPanelName.c_str(), nodeDetails);
		HideTabBar(nodeViewport);
		SetCentralNode(dockspaceID, nodeViewport);
	}

	void AssetEditor::HideTabBar(ImGuiID dockNodeID)
	{
		if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockNodeID))
			node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_HiddenTabBar);
	}

	// Note: `ImGuiDockNode::CentralNode` of the root can't be used to find the central node. It's only updated when the dockspace is submitted,
	// and it can point to a deleted node until then (for example, after nodes were merged because a panel was undocked)
	static ImGuiDockNode* FindCentralNode(ImGuiDockNode* node)
	{
		if (!node)
			return nullptr;
		if (node->IsLeafNode())
			return (node->LocalFlags & ImGuiDockNodeFlags_CentralNode) ? node : nullptr;
		if (ImGuiDockNode* centralNode = FindCentralNode(node->ChildNodes[0]))
			return centralNode;
		return FindCentralNode(node->ChildNodes[1]);
	}

	void AssetEditor::SetCentralNode(ImGuiID dockspaceID, ImGuiID dockNodeID)
	{
		ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspaceID);
		if (!root)
			return;

		ImGuiDockNode* node = dockNodeID ? ImGui::DockBuilderGetNode(dockNodeID) : nullptr;
		if (node && (!node->IsLeafNode() || ImGui::DockNodeGetRootNode(node) != root))
			return;

		// Note: after `DockBuilderSplitNode()`, the central node is the child on the opposite side of the split direction
		// (for example, the details when splitting the viewport to the left), so undocking that panel would leave an empty space behind
		if (ImGuiDockNode* oldCentralNode = FindCentralNode(root); oldCentralNode && oldCentralNode != node)
			oldCentralNode->SetLocalFlags(oldCentralNode->LocalFlags & ~ImGuiDockNodeFlags_CentralNode);
		if (node)
			node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_CentralNode);
		root->CentralNode = node;
	}

	void AssetEditor::UpdateCentralNode()
	{
		// Keeps the viewport's node as the central node (see `SetCentralNode`), also after the user rearranged the panes.
		// If the viewport isn't docked in this editor (it's floating), there's no central node, so no empty space is left behind.
		// It's only done once the viewport panel exists, so the panels are already bound to their dock nodes (otherwise every node looks empty)
		ImGuiWindow* viewportWindow = ImGui::FindWindowByName(m_ViewportPanelName.c_str());
		ImGuiDockNode* root = ImGui::DockBuilderGetNode(m_DockSpaceID);
		if (!viewportWindow || !root)
			return;

		const bool bViewportDockedHere = viewportWindow->DockNode && ImGui::DockNodeGetRootNode(viewportWindow->DockNode) == root;
		ImGuiDockNode* newCentralNode = bViewportDockedHere ? viewportWindow->DockNode : nullptr;
		ImGuiDockNode* oldCentralNode = FindCentralNode(root);
		if (newCentralNode == oldCentralNode)
			return;

		// An empty node is only kept if it's the central node. Every other empty node is removed (and its neighbour takes its space)
		const bool bOldCentralNodeEmpty = oldCentralNode && oldCentralNode->ParentNode && oldCentralNode->IsLeafNode() && oldCentralNode->Windows.Size == 0;
		if (!newCentralNode && !bOldCentralNodeEmpty)
			return; // The viewport is floating, but the current central node isn't empty, so it doesn't leave an empty space. Keep it

		SetCentralNode(m_DockSpaceID, newCentralNode ? newCentralNode->ID : 0);

		// ImGui only removes an empty node when its last window leaves it. The old central node was kept back then, so remove it now
		if (bOldCentralNodeEmpty)
			ImGui::DockBuilderRemoveNode(oldCentralNode->ID);
	}

	std::string AssetEditor::GetPanelName(const Ref<Asset>& asset, std::string_view panel)
	{
		return std::string(panel) + "##" + std::to_string(asset->GetGUID().GetHash());
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

		// The host window, then the viewport panel if there's one
		if (ImGuiWindow* window = ImGui::FindWindowByName(m_EditorWindowName.c_str()))
			ImGui::FocusWindow(window);
		if (ImGuiWindow* window = ImGui::FindWindowByName(m_ViewportPanelName.c_str()))
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

	std::string AssetEditor::GetAssetWindowName(const Ref<Asset>& asset, std::string_view ending)
	{
		return Utils::AsString(asset->GetPath().stem()) + (ending.empty() ? "" : ending.data()) + "##" + std::to_string(asset->GetGUID().GetHash());
	}
	
	bool AssetEditor::DrawAlwaysIncludeInGameBuilds(const Ref<Asset>& asset)
	{
		bool bAlwaysInclude = asset->IsAlwaysIncludedInGameBuilds();
		if (!UI::Property("Always build", bAlwaysInclude, s_AlwaysIncludeInGameBuildsHelpMsg))
			return false;

		asset->SetAlwaysIncludedInGameBuilds(bAlwaysInclude);
		return true;
	}

	bool AssetEditor::DrawAlwaysIncludeInGameBuildsCheckbox(const Ref<Asset>& asset)
	{
		bool bAlwaysInclude = asset->IsAlwaysIncludedInGameBuilds();
		const bool bChanged = ImGui::Checkbox("Always build", &bAlwaysInclude);
		ImGui::SameLine();
		UI::HelpMarker(s_AlwaysIncludeInGameBuildsHelpMsg);
		if (bChanged)
			asset->SetAlwaysIncludedInGameBuilds(bAlwaysInclude);
		return bChanged;
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
