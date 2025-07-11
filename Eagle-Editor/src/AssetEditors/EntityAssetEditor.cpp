#include "egpch.h"
#include "EntityAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "../EditorLayer.h"

#include <ImGuizmo/ImGuizmo.h>

namespace Eagle
{
	static glm::vec3 notUsed1;
	static glm::vec4 notUsed2;

	EntityAssetEditor::EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer)
		: AssetEditor(true), m_Asset(asset), m_EditorLayer(editorLayer)
	{
		const auto& scene = GetCurrentScene();
		m_Entity = scene->CreateFromEntityAsset(m_Asset);

		auto& camera = scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		AABB aabb;
		if (m_Entity.HasComponent<StaticMeshComponent>())
		{
			const auto& comp = m_Entity.GetComponent<StaticMeshComponent>();
			if (const auto& asset = comp.GetMeshAsset())
				aabb.Grow(asset->GetMesh()->GetAABB());
		}
		if (m_Entity.HasComponent<SkeletalMeshComponent>())
		{
			const auto& comp = m_Entity.GetComponent<SkeletalMeshComponent>();
			if (const auto& asset = comp.GetMeshAsset())
				aabb.Grow(asset->GetMesh()->GetAABB());
		}
		if (m_Entity.HasComponent<SpriteComponent>())
		{
			const auto& comp = m_Entity.GetComponent<SpriteComponent>();
			const auto& transform = comp.GetWorldTransform();
			const glm::vec3 halfScale = transform.Scale3D * 0.5f;
			aabb.Grow(AABB{ transform.Location - halfScale, transform.Location + halfScale });
		}

		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
		camera.LookAt(center);
	}

	void EntityAssetEditor::OnImGuiRender(bool* pOpen)
	{
		constexpr bool bRuntime = false;
		constexpr bool bVolumetricsEnabled = true;
		constexpr bool bDrawTransform = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		const std::string windowName = m_Asset->GetPath().u8string();
		if (ImGui::Begin(windowName.c_str(), pOpen))
		{
			const bool bEntityChanged = m_EntityProperties.OnImGuiRender(*m_Asset->GetEntity().get(), bRuntime, bVolumetricsEnabled, bDrawTransform);
			if (bEntityChanged)
				OnEntityChanged();

			ImGui::Separator();
			ImGui::Separator();

			{
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
				}
				ImGui::SameLine();
				UI::HelpMarker("The scene needs to be saved to store reloaded assets");

				if (bDisableReload)
					UI::PopItemDisabled();

				UI::Tooltip("On the opened scene, all entities created from this assets will be reloaded to match this asset");
			}

		}
		ImGui::End(); // Entity Editor

		DrawViewport(false, windowName);
	}

	void EntityAssetEditor::UpdateGuizmo()
	{
		SceneComponent* selectedComponent = m_EntityProperties.GetSelectedComponent();
		if (!selectedComponent || m_GuizmoType == -1)
			return;

		Transform transform = selectedComponent->GetWorldTransform();
		const bool bRelative = m_GuizmoType == ImGuizmo::OPERATION::ROTATE;

		// If relative, we only get relative rotation, since other params need to be in world coords.
		// Otherwise, for example, guizmo will be renderer in the position if we used relative location
		if (bRelative)
			transform.Rotation = selectedComponent->GetRelativeTransform().Rotation;

		if (DrawGuizmo(transform, true))
		{
			if (bRelative)
				selectedComponent->SetRelativeTransform(transform);
			else
				selectedComponent->SetWorldTransform(transform);
			OnEntityChanged();
		}
	}
	
	void EntityAssetEditor::OnEntityChanged()
	{
		const auto& scene = GetCurrentScene();

		m_Asset->SetDirty(true);
		m_Asset->OnModified();
		scene->DestroyEntity(m_Entity);
		m_Entity = scene->CreateFromEntityAsset(m_Asset);
	}
}
