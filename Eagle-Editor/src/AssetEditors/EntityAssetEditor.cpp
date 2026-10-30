#include "egpch.h"
#include "EntityAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "../EditorLayer.h"

#include <ImGuizmo.h>

namespace Eagle
{
	static const char* s_ReloadHelpMsg = "ONLY on the opened scene, all entities created from this assets will be reloaded to match this asset. The scene needs to be re-saved to save reloaded assets.\n"
		"Warning: if children of this asset are passed to C# scripts through UI, they will be invalidated since entities are basically recreated. So, update your C# script components if required. "
		"So, on reload, ONLY the root entity is properly resolved for C# public `Entity` fields.";

	EntityAssetEditor::EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer)
		: AssetEditor(true), m_Asset(asset), m_EditorLayer(editorLayer)
	{
		m_FirstUseViewportRatio = 0.6f;
		const auto& scene = GetCurrentScene();
		m_Entity = scene->CreateFromEntityAsset(m_Asset, true);

		auto& camera = scene->EditorCamera;
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

		m_SceneHierarchy.SetHashID(asset->GetGUID().GetHigh());
		m_SceneHierarchy.SetWindowClass(&GetPanelWindowClass());
	}

	void EntityAssetEditor::OnImGuiRender(bool* pOpen)
	{
		constexpr bool bRuntime = false;
		constexpr bool bVolumetricsEnabled = true;
		constexpr bool bDrawTransform = false;

		DrawEditorWindow(pOpen);

		const bool bEntityChanged = m_SceneHierarchy.OnImGuiRender(GetCurrentScene(), bRuntime, true, &bVolumetricsEnabled);
		if (bEntityChanged)
			OnEntityChanged();

		if (BeginPanel(GetDetailsPanelName()))
		{
			const auto& scene = GetCurrentScene();

			{
				UI::TextWithSeparator("Asset settings");
				UI::BeginPropertyGrid("EntityAssetSettings");
				AssetEditor::DrawAlwaysIncludeInGameBuilds(m_Asset);
				UI::EndPropertyGrid();
			}

			{
				bool bEnableSkybox = scene->IsSkyboxEnabled();
				UI::TextWithSeparator("Visualization settings");

				UI::BeginPropertyGrid("EntityDetails");
				UI::Property("Update animations", m_UpdateAnims);
				if (UI::Property("Enable IBL", bEnableSkybox))
					scene->SetSkyboxEnabled(bEnableSkybox);
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
					Scene::GetCurrentScene()->ReloadEntitiesCreatedFromAsset(m_Asset);
					if (auto& sceneAsset = m_EditorLayer.GetOpenedSceneAsset())
						sceneAsset->SetDirty(true);
				}
				ImGui::SameLine();
				UI::HelpMarker(s_ReloadHelpMsg);

				if (bDisableReload)
					UI::PopItemDisabled();
			}

		}
		ImGui::End(); // Entity Editor

		{
			// Outlines of text boxes and 2D UI
			const auto& scene = GetCurrentScene();
			Entity selectedEntity = m_SceneHierarchy.GetSelectedEntity();
			scene->TextUIDebugSelectedEntity = selectedEntity ? selectedEntity.GetGUID() : GUID(0, 0);
			scene->bDrawAllTextUIBounds = m_EditorLayer.IsDrawingAllTextUIBounds();
		}

		DrawViewport(m_UpdateAnims);
	}

	void EntityAssetEditor::OnEvent(Event& e)
	{
		AssetEditor::OnEvent(e);
		Event::Dispatch<MouseButtonPressedEvent>(e, EG_BIND_FN(EntityAssetEditor::HandleEntitySelection));

		const auto& scene = GetCurrentScene();
		const bool bEntityChanged = m_SceneHierarchy.OnEvent(scene, e, bViewportFocused);
		if (bEntityChanged)
			OnEntityChanged();
	}

	bool EntityAssetEditor::HandleEntitySelection(MouseButtonPressedEvent& e)
	{
		// Entity Selection
		Entity selectedEntity = m_SceneHierarchy.GetSelectedEntity();
		bool bUsingImGuizmo = selectedEntity && bUsingImGuizmoOrHovered;
		const auto& scene = GetCurrentScene();
		if (bViewportHovered && !bViewportToolbarHovered && !bUsingImGuizmo && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
		{
			const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			const glm::ivec2 mouse = GetMousePosWithinViewport();

			if (mouse.x >= 0 && mouse.y >= 0 && mouse.x < (int)viewportSize.x && mouse.y < (int)viewportSize.y)
			{
				int32_t data = -1;
				scene->GetSceneRenderer()->GetObjectIDUnderMouse(data);
				m_SceneHierarchy.SetEntitySelected(data == -1 ? Entity::Null : Entity{ (entt::entity)data, scene.get() });
				return true;
			}
		}
		return false;
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
				GetCurrentScene()->DrawBox(aabb, selectedComponent->GetWorldTransform());
				const auto& start = selectedComponent->GetWorldTransform().Location;
				GetCurrentScene()->DrawArrow(start, start + selectedComponent->GetForwardVector() * 0.25f, selectedComponent->GetUpVector());
			}
			else if (selectedType == SelectedComponent::CameraComponent)
			{
				CameraComponent& camera = *(CameraComponent*)selectedComponent;
				GetCurrentScene()->DrawFrustum(camera);
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
	
	void EntityAssetEditor::BuildDefaultLayout(ImGuiID dockspaceID)
	{
		// Left: details (top), scene hierarchy, properties (bottom). Right: viewport
		ImGuiID nodeDetails;
		ImGuiID nodeViewport;
		ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 1.f - m_FirstUseViewportRatio, &nodeDetails, &nodeViewport);

		ImGuiID nodeSceneHierarchy;
		ImGui::DockBuilderSplitNode(nodeDetails, ImGuiDir_Up, 0.2f, &nodeDetails, &nodeSceneHierarchy);

		ImGuiID nodeEntityProperties;
		ImGui::DockBuilderSplitNode(nodeSceneHierarchy, ImGuiDir_Up, 0.5f, &nodeSceneHierarchy, &nodeEntityProperties);

		ImGui::DockBuilderDockWindow(GetDetailsPanelName().c_str(), nodeDetails);
		ImGui::DockBuilderDockWindow(m_SceneHierarchy.GetSceneHierarchyWindowName().c_str(), nodeSceneHierarchy);
		ImGui::DockBuilderDockWindow(m_SceneHierarchy.GetPropertiesWindowName().c_str(), nodeEntityProperties);
		ImGui::DockBuilderDockWindow(GetViewportPanelName().c_str(), nodeViewport);
		HideTabBar(nodeViewport);
		SetCentralNode(dockspaceID, nodeViewport);
	}
}
