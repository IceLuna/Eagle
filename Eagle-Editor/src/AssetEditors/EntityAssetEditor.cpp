#include "egpch.h"
#include "EntityAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "../EditorLayer.h"

#include <ImGuizmo/ImGuizmo.h>

namespace Eagle
{
	static const char* s_ReloadHelpMsg = "The scene needs to be re-saved to save reloaded assets.\n"
		"Warning: if an entity is passed to C# scripts through UI, it will be invalidated since entities are basically recreated. So, update your C# script components if required";

	EntityAssetEditor::EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer)
		: AssetEditor(true), m_Asset(asset), m_EditorLayer(editorLayer)
	{
		const auto& scene = GetCurrentScene();
		m_Entity = scene->CreateFromEntityAsset(m_Asset, true);

		auto& camera = scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		AABB aabb;
		if (m_Entity.HasComponent<StaticMeshComponent>())
		{
			const auto& comp = m_Entity.GetComponent<StaticMeshComponent>();
			if (const auto& asset = comp.GetMeshAsset())
				aabb.Grow(AABB::Transformed(asset->GetMesh()->GetAABB(), comp.GetWorldTransform()));
		}
		if (m_Entity.HasComponent<SkeletalMeshComponent>())
		{
			const auto& comp = m_Entity.GetComponent<SkeletalMeshComponent>();
			if (const auto& asset = comp.GetMeshAsset())
				aabb.Grow(AABB::Transformed(asset->GetMesh()->GetAABB(), comp.GetWorldTransform()));
		}
		if (m_Entity.HasComponent<SpriteComponent>())
		{
			const auto& comp = m_Entity.GetComponent<SpriteComponent>();
			const auto& transform = comp.GetWorldTransform();
			const glm::vec3 halfScale = transform.Scale3D * 0.5f;
			aabb.Grow(AABB{ transform.Location - halfScale, transform.Location + halfScale });
		}

		if (aabb.IsValid())
		{
			const glm::vec3 center = aabb.Center();
			camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
			camera.LookAt(center);
		}

		m_SceneHierarchy.SetContext(scene, asset->GetGUID().GetHigh());
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void EntityAssetEditor::OnImGuiRender(bool* pOpen)
	{
		constexpr bool bRuntime = false;
		constexpr bool bVolumetricsEnabled = true;
		constexpr bool bDrawTransform = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 160.f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(m_WindowName.c_str(), pOpen))
		{
			const bool bEntityChanged = m_SceneHierarchy.OnImGuiRender(bRuntime, true, &bVolumetricsEnabled);
			if (bEntityChanged)
				OnEntityChanged();

			{
				UI::TextWithSeparator("Visualization settings");

				UI::BeginPropertyGrid("EntityDetails");
				UI::Property("Update animations", m_UpdateAnims);
				UI::EndPropertyGrid();

				ImGui::Separator();

				if (ImGui::Button("Save asset"))
					Asset::Save(m_Asset);

				const bool bDisableReload = m_EditorLayer.GetEditorState() != EditorState::Edit;
				if (bDisableReload)
					UI::PushItemDisabled();

				ImGui::SameLine();

				if (ImGui::Button("Reload entities"))
				{
					auto& scene = Scene::GetCurrentScene();
					scene->ReloadEntitiesCreatedFromAsset(m_Asset);
					if (auto& sceneAsset = m_EditorLayer.GetOpenedSceneAsset())
						sceneAsset->SetDirty(true);
				}
				ImGui::SameLine();
				UI::HelpMarker(s_ReloadHelpMsg);

				if (bDisableReload)
					UI::PopItemDisabled();

				UI::Tooltip("On the opened scene, all entities created from this assets will be reloaded to match this asset");
			}

		}
		ImGui::End(); // Entity Editor

		DrawViewport(m_UpdateAnims, m_WindowName);
	}

	void EntityAssetEditor::OnEvent(Event& e)
	{
		AssetEditor::OnEvent(e);

		const bool bEntityChanged = m_SceneHierarchy.OnEvent(e, bViewportFocused);
		if (bEntityChanged)
			OnEntityChanged();
	}

	void EntityAssetEditor::UpdateGuizmo()
	{
		Entity selectedEntity = m_SceneHierarchy.GetSelectedEntity();
		SceneComponent* selectedComponent = m_SceneHierarchy.GetSelectedComponent();
		if (selectedComponent)
		{
			const auto selectedType = m_SceneHierarchy.GetSelectedComponentType();
			if (selectedType == SelectedComponent::DecalComponent)
			{
				const AABB aabb(glm::vec3(-0.5f), glm::vec3(0.5f));
				GetCurrentScene()->DrawAABB(aabb, selectedComponent->GetWorldTransform());
			}
		}

		if (!selectedEntity || (m_GuizmoType == -1))
			return;

		Transform transform;
		bool bRelative = false; // Only used for rotations
		if (selectedComponent)
		{
			transform = selectedComponent->GetWorldTransform();
			bRelative = m_GuizmoType == ImGuizmo::OPERATION::ROTATE;
		}
		else
		{
			transform = selectedEntity.GetWorldTransform();
			bRelative = selectedEntity.HasParent() && (m_GuizmoType == ImGuizmo::OPERATION::ROTATE);
		}

		// If relative, we only get relative rotation, since other params need to be in world coords.
		// Otherwise, for example, guizmo will be renderer in the position if we used relative location
		if (bRelative)
			transform.Rotation = (selectedComponent ? selectedComponent->GetRelativeTransform() : selectedEntity.GetRelativeTransform()).Rotation;

		if (DrawGuizmo(transform, true, !bRelative))
		{
			if (bRelative && GetGuizmoType() == ImGuizmo::OPERATION::ROTATE)
			{
				Rotator newRotation = transform.Rotation;
				transform = selectedComponent ? selectedComponent->GetRelativeTransform() : selectedEntity.GetRelativeTransform();
				transform.Rotation = newRotation;
			}
			if (selectedComponent)
				bRelative ? selectedComponent->SetRelativeTransform(transform) : selectedComponent->SetWorldTransform(transform);
			else
				bRelative ? selectedEntity.SetRelativeTransform(transform) : selectedEntity.SetWorldTransform(transform);
			OnEntityChanged();
		}
	}
	
	void EntityAssetEditor::OnEntityChanged()
	{
		const auto& assetEntity = m_Asset->GetEntity();
		const auto& assetEntityScene = assetEntity->GetScene();
		assetEntityScene->DestroyEntityImmediately(*assetEntity.get(), true);
		*assetEntity = assetEntityScene->CreateFromEntity(m_Entity, true);

		m_Asset->SetDirty(true);
		m_Asset->OnModified();
	}
	
	void EntityAssetEditor::HandleFirstWindowRender(std::string_view windowName, std::string_view parentName)
	{
		const bool bFirstUseEver = (ImGui::GetCurrentWindow()->SetWindowDockAllowFlags & ImGuiCond_FirstUseEver) == ImGuiCond_FirstUseEver;

		if (bFirstUseEver && !parentName.empty())
		{
			ImGuiID parent_node = ImGui::DockBuilderAddNode();
			ImGui::DockBuilderSetNodePos(parent_node, ImGui::GetWindowPos());
			ImGui::DockBuilderSetNodeSize(parent_node, ImGui::GetWindowSize());
			ImGuiID nodeDetails; // Main window
			ImGuiID nodeViewport;
			ImGui::DockBuilderSplitNode(parent_node, ImGuiDir_Left, 0.5f, &nodeDetails, &nodeViewport);

			ImGui::DockBuilderDockWindow(parentName.data(), nodeDetails);
			ImGui::DockBuilderDockWindow(windowName.data(), nodeViewport);

			ImGuiID nodeSceneHierarchy;
			ImGui::DockBuilderSplitNode(nodeDetails, ImGuiDir_Up, 0.2f, &nodeDetails, &nodeSceneHierarchy);
			ImGui::DockBuilderDockWindow(parentName.data(), nodeDetails);
			ImGui::DockBuilderDockWindow(m_SceneHierarchy.GetSceneHierarchyWindowName().c_str(), nodeSceneHierarchy);

			ImGuiID nodeEntityProperties;
			ImGui::DockBuilderSplitNode(nodeSceneHierarchy, ImGuiDir_Up, 0.5f, &nodeSceneHierarchy, &nodeEntityProperties);
			ImGui::DockBuilderDockWindow(m_SceneHierarchy.GetSceneHierarchyWindowName().c_str(), nodeSceneHierarchy);
			ImGui::DockBuilderDockWindow(m_SceneHierarchy.GetPropertiesWindowName().c_str(), nodeEntityProperties);

			// Disable tab bar for the viewport
			if (ImGuiDockNode* dock = ImGui::DockContextFindNodeByID(GImGui, nodeViewport))
			{
				dock->SetLocalFlags(ImGuiDockNodeFlags_NoTabBar);
			}

			ImGui::SetWindowSize(ImVec2(720.f * 2.f, 560.f));
		}
	}
}
