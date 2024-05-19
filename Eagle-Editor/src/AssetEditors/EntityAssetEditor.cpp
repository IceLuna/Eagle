#include "egpch.h"
#include "EntityAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "../EditorLayer.h"

#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Eagle
{
	static glm::vec3 notUsed1;
	static glm::vec4 notUsed2;

	EntityAssetEditor::EntityAssetEditor(const Ref<AssetEntity>& asset, const EditorLayer& editorLayer)
		: AssetEditor(true), m_Asset(asset), m_EditorLayer(editorLayer)
	{
		m_Entity = m_Scene->CreateFromEntityAsset(m_Asset);
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	}

	void EntityAssetEditor::OnImGuiRender(bool* pOpen)
	{
		constexpr bool bRuntime = false;
		constexpr bool bVolumetricsEnabled = true;
		constexpr bool bDrawTransform = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen))
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

		DrawViewport();
	}

	void EntityAssetEditor::OnEvent(Event& e)
	{
		AssetEditor::OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EntityAssetEditor::OnKeyPressed));
	}

	void EntityAssetEditor::UpdateGuizmo()
	{
		SceneComponent* selectedComponent = m_EntityProperties.GetSelectedComponent();
		if (!selectedComponent || m_GuizmoType == -1)
			return;

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

		//Camera
		const auto& editorCamera = m_Scene->GetEditorCamera();
		const glm::mat4& cameraViewMatrix = editorCamera.GetViewMatrix();
		glm::mat4 cameraProjection = editorCamera.GetProjection();
		cameraProjection[1][1] *= -1.f; // Since in Vulkan [1][1] of Projection is flipped, we need to flip it back for Guizmo

		Transform transform = selectedComponent->GetWorldTransform();
		Transform finalTransform = transform;
		const bool bRelative = m_GuizmoType == ImGuizmo::OPERATION::ROTATE;

		// If relative, we only get relative rotation, since other params need to be in world coords.
		// Otherwise, for example, guizmo will be renderer in the position if we used relative location
		if (bRelative)
			transform.Rotation = selectedComponent->GetRelativeTransform().Rotation;

		int snappingIndex = 0;
		if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
			snappingIndex = 1;
		else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
			snappingIndex = 2;

		//Snapping
		const glm::vec3& snappingSettings = m_EditorLayer.GetSnappingValues();
		const float snapValues[3] = { snappingSettings[snappingIndex], snappingSettings[snappingIndex], snappingSettings[snappingIndex] };
		const bool bSnap = Input::IsKeyPressed(Key::LeftShift);

		ImGuizmo::SetID(int(m_Asset->GetEntity()->GetID()));

		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
		ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
			ImGuizmo::WORLD, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);

		if (ImGuizmo::IsUsing())
		{
			glm::quat newRotation;

			glm::decompose(transformMatrix, transform.Scale3D, newRotation, transform.Location, notUsed1, notUsed2);

			if (m_GuizmoType == ImGuizmo::OPERATION::TRANSLATE)
				finalTransform.Location = transform.Location;
			if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
			{
				if (bRelative)
					finalTransform = selectedComponent->GetRelativeTransform();
				finalTransform.Rotation = newRotation;
			}
			if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
				finalTransform.Scale3D = transform.Scale3D;

			if (bRelative)
				selectedComponent->SetRelativeTransform(finalTransform);
			else
				selectedComponent->SetWorldTransform(finalTransform);
			OnEntityChanged();
		}
	}
	
	bool EntityAssetEditor::OnKeyPressed(KeyPressedEvent& e)
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
	
	void EntityAssetEditor::OnEntityChanged()
	{
		m_Asset->SetDirty(true);
		m_Scene->DestroyEntity(m_Entity);
		m_Entity = m_Scene->CreateFromEntityAsset(m_Asset);
	}
}
