#pragma once

#include "Eagle.h"
#include <imgui_internal.h>

namespace Eagle
{
	class UndoHistory;

	enum class SelectedComponent
	{
		None,
		SpriteComponent,
		StaticMeshComponent,
		SkeletalMeshComponent,
		BillboardComponent,
		TextComponent,
		CameraComponent,
		PointLightComponent,
		DirectionalLightComponent,
		SpotLightComponent,
		ScriptComponent,
		CharacterControllerComponent,
		RigidBodyComponent,
		BoxColliderComponent,
		SphereColliderComponent,
		CapsuleColliderComponent,
		MeshColliderComponent,
		AudioComponent,
		ReverbComponent,
		Text2DComponent,
		Image2DComponent,
		ParticleSystemComponent,
		DecalComponent,
		NavigationMeshComponent,
		NavigationCrowdAgentComponent,
		SceneSequenceComponent,
		EntityAssetComponent,
	};

	class EntityPropertiesPanel
	{
	public:
		EntityPropertiesPanel() = default;

		// @newEntitySelection. If not null, assigns an entity that should be selected
		// Returns true if something was changed.
		bool OnImGuiRender(Entity entity, bool bRuntime, bool bVolumetricsEnabled, Entity* newEntitySelection = nullptr);
		void SetEntitySelected(Entity entity, SelectedComponent selectedComponent);
		SelectedComponent GetSelectedComponentType() const { return m_SelectedComponent; }
		SceneComponent* GetSelectedComponent();

		void SetUndoContext(UndoHistory* history, const Ref<Scene>& scene) { m_UndoHistory = history; m_UndoScene = scene; }

	private:
		bool HasSelectedComponent() const;

		void DrawComponents(Entity entity, Entity* newEntitySelection);

		template <typename T, typename UIFunction>
		void DrawComponent(const std::string& name, Entity& entity, UIFunction&& function, bool canRemove = true)
		{
			if (entity.HasComponent<T>())
			{
				ImGui::PushID(int(typeid(T).hash_code()));

				ImGui::Separator();
				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
					| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

				const bool bTreeOpened = UI::PushTreeNode(name, true, true, "", typeid(T).hash_code());
				const ImVec2 headerMin = ImGui::GetItemRectMin();
				const ImVec2 headerMax = ImGui::GetItemRectMax();

				bool bRemoveComponent = false;

				if (canRemove)
				{
					const float buttonSize = headerMax.y - headerMin.y;
					ImGui::SameLine();
					ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize, headerMin.y));
					if (ImGui::Button("...", ImVec2{ buttonSize, buttonSize }))
					{
						ImGui::OpenPopup("ComponentSettings");
					}

					if (ImGui::BeginPopup("ComponentSettings"))
					{
						if (ImGui::MenuItem("Remove Component"))
						{
							bRemoveComponent = true;
							m_SelectedComponent = SelectedComponent::None;
						}
						ImGui::EndPopup();
					}
				}

				if (bTreeOpened)
				{
					T& component = entity.GetComponent<T>();
					function(component);

					UI::PopTreeNode();
				}

				ImGui::PopID();

				if (bRemoveComponent)
				{
					entity.RemoveComponent<T>();
					bEntityChanged = true;
				}
			}
		}

		template <typename T>
		bool DrawComponentLine(const std::string& name, Entity& entity, bool bSelected, bool bCanRemove = true)
		{
			bool bClicked = false;
			if (entity.HasComponent<T>())
			{
				ImGuiTreeNodeFlags childFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Leaf
					| ImGuiTreeNodeFlags_SpanAvailWidth | (bSelected ? ImGuiTreeNodeFlags_Selected : 0);
				bool treeOpened = ImGui::TreeNodeEx((void*)(typeid(T).hash_code() + typeid(Entity).hash_code()), childFlags, name.c_str());

				bClicked = ImGui::IsItemClicked();

				std::string popupID = std::to_string(entity.GetID()) + typeid(T).name();
				if (ImGui::BeginPopupContextItem(popupID.c_str()))
				{
					if (!bCanRemove)
						UI::PushItemDisabled();
					if (ImGui::MenuItem("Remove Component"))
					{
						m_SelectedComponent = SelectedComponent::None;
						entity.RemoveComponent<T>();
						bEntityChanged = true;
					}
					if (!bCanRemove)
						UI::PopItemDisabled();

					ImGui::EndPopup();
				}

				if (treeOpened)
				{
					ImGui::TreePop();
				}
			}
			return bClicked;
		}

		template <typename T>
		bool DrawAddComponentMenuItem(const char* name, const char* typeName)
		{
			if (!m_Entity)
				return false;

			const bool bDisable = m_Entity.HasComponent<T>() == true;
			if (bDisable)
				UI::PushItemDisabled();

			bool bAdded = false;
			if (ImGui::MenuItem(name))
			{
				m_Entity.AddComponent<T>();
				bAdded = true;
				EG_CORE_TRACE("Added '{}' to {}", typeName, m_Entity.GetName());

				ImGui::CloseCurrentPopup();
			}

			if (bDisable)
			{
				UI::PopItemDisabled();
				ImGui::SameLine();
				auto cursorPos = ImGui::GetCursorScreenPos();
				cursorPos.x -= 20.f;
				ImGui::SetCursorScreenPos(cursorPos);
				UI::HelpMarker("Already exists");
			}

			bEntityChanged |= bAdded;
			return bAdded;
		}

		void DrawEntityTransformNode(Entity& entity);
		void DrawComponentTransformNode(Entity& entity, SceneComponent& sceneComponent);

	private:
		Entity m_Entity;
		UndoHistory* m_UndoHistory = nullptr;
		Weak<Scene> m_UndoScene;
		SelectedComponent m_SelectedComponent = SelectedComponent::None;
		bool bRuntime = false;
		bool bVolumetricsEnabled = false;
		bool bEntityChanged = false;
	};
}
