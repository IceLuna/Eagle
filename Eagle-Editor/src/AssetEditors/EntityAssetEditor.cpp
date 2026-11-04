#include "egpch.h"
#include "EntityAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "../EditorLayer.h"
#include "../Undo/UndoUI.h"
#include "../Widgets/ViewportToolbar.h"

#include <ImGuizmo.h>

namespace Eagle
{
	static const char* s_ReloadHelpMsg = "ONLY on the opened scene, all entities created from this assets will be reloaded to match this asset. The scene needs to be re-saved to save reloaded assets.\n"
		"Warning: if children of this asset are passed to C# scripts through UI, they will be invalidated since entities are basically recreated. So, update your C# script components if required. "
		"So, on reload, ONLY the root entity is properly resolved for C# public `Entity` fields.";

	EntityAssetEditor::EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer)
		: AssetEditor(true), m_Asset(asset), m_EditorLayer(editorLayer)
		, m_AssetSettingsTracker([](const NoAssetState&, const NoAssetState&) { return std::string("Edit Asset Settings"); })
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
		m_SceneHierarchy.SetPropertiesInHierarchyWindow(true);
		m_SceneHierarchy.SetUndoHistory(&m_UndoHistory);
		m_SceneHierarchy.SetMultiSelectionEnabled(true);
		m_GuizmoMode = ImGuizmo::MODE::WORLD;

		m_UndoHistory.SetOnAppliedCallback([this](const UndoTransaction& transaction, bool)
		{
			m_SceneHierarchy.OnUndoApplied(transaction);
			OnEntityChanged();
		});
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
				m_AssetSettingsTracker.BeginFrame(&m_UndoHistory, m_Asset);

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
					// It changes the level, so it's recorded by the level editor. The entities are recreated and references to them are updated,
					// so any entity of the level can change
					EditorLayer* editorLayer = EditorLayer::Get();
					SceneTransaction transaction(editorLayer->GetUndoHistory(), Scene::GetCurrentScene(), "Reload Entities from Asset");
					transaction.TrackAll();
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
		m_AssetSettingsTracker.EndFrame();

		{
			// Outlines of text boxes and 2D UI
			const auto& scene = GetCurrentScene();
			Entity selectedEntity = m_SceneHierarchy.GetSelectedEntity();
			scene->TextUIDebugSelectedEntity = selectedEntity ? selectedEntity.GetGUID() : GUID(0, 0);
			scene->bDrawAllTextUIBounds = m_EditorLayer.IsDrawingAllTextUIBounds();

			if (bViewportVisible && selectedEntity && selectedEntity.GetScene() == scene.get())
				EditorUtils::DrawSelectedComponentHelpers(*scene, m_SceneHierarchy.GetSelectedComponent());
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
		bool bUsingImGuizmo = selectedEntity && m_SelectionGuizmo.IsUsingOrHovered();
		const auto& scene = GetCurrentScene();
		if (bViewportHovered && !bViewportToolbarHovered && !bUsingImGuizmo && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
		{
			const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			const glm::ivec2 mouse = GetMousePosWithinViewport();

			if (mouse.x >= 0 && mouse.y >= 0 && mouse.x < (int)viewportSize.x && mouse.y < (int)viewportSize.y)
			{
				int32_t data = -1;
				scene->GetSceneRenderer()->GetObjectIDUnderMouse(data);
				m_SceneHierarchy.OnViewportEntityClicked(data == -1 ? Entity::Null : Entity{ (entt::entity)data, scene.get() });
				return true;
			}
		}
		return false;
	}

	void EntityAssetEditor::UpdateGuizmo()
	{
		const auto& scene = GetCurrentScene();
		SelectionGuizmo::Viewport viewport;
		if (bViewportVisible && GetGuizmoCameraMatrices(viewport.View, viewport.Projection))
		{
			viewport.Min = m_ViewportBounds[0];
			viewport.Max = m_ViewportBounds[1];

			SelectionGuizmo::Settings settings;
			settings.Operation = m_GuizmoType;
			settings.Mode = m_GuizmoMode;
			settings.Pivot = m_GuizmoPivot;
			settings.GroupTransform = m_GuizmoGroupTransform;
			settings.Snap = &EditorLayer::Get()->GetSnapSettings();

			SelectionGuizmo::Selection selection;
			selection.Active = m_SceneHierarchy.GetSelectedEntity();
			selection.Entities = m_SceneHierarchy.GetSelectedEntities();
			selection.Component = m_SceneHierarchy.GetSelectedComponent();

			if (m_SelectionGuizmo.Update(selection, scene, viewport, settings, &m_UndoHistory))
				OnEntityChanged();
		}
		m_SelectionGuizmo.EndFrame();
	}

	void EntityAssetEditor::SetupViewportToolbar(ViewportToolbar& toolbar)
	{
		toolbar.GuizmoMode = &m_GuizmoMode;
		toolbar.Pivot = &m_GuizmoPivot;
		toolbar.GroupTransform = &m_GuizmoGroupTransform;
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
		// Left: details (top), scene hierarchy with the properties of the selected entity (bottom). Right: viewport
		ImGuiID nodeDetails;
		ImGuiID nodeViewport;
		ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 1.f - m_FirstUseViewportRatio, &nodeDetails, &nodeViewport);

		ImGuiID nodeSceneHierarchy;
		ImGui::DockBuilderSplitNode(nodeDetails, ImGuiDir_Up, 0.2f, &nodeDetails, &nodeSceneHierarchy);

		ImGui::DockBuilderDockWindow(GetDetailsPanelName().c_str(), nodeDetails);
		ImGui::DockBuilderDockWindow(m_SceneHierarchy.GetSceneHierarchyWindowName().c_str(), nodeSceneHierarchy);
		ImGui::DockBuilderDockWindow(GetViewportPanelName().c_str(), nodeViewport);
		HideTabBar(nodeViewport);
		SetCentralNode(dockspaceID, nodeViewport);
	}
}
