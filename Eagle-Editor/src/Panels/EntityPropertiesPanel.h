#pragma once

#include "Eagle.h"

namespace Eagle
{
	enum class SelectedComponent
	{
		None,
		Sprite,
		StaticMesh,
		SkeletalMesh,
		Billboard,
		Text3D,
		Camera,
		PointLight,
		DirectionalLight,
		SpotLight,
		Script,
		RigidBody,
		BoxCollider,
		SphereCollider,
		CapsuleCollider,
		MeshCollider,
		AudioComponent,
		ReverbComponent,
		Text2D,
		Image2D,
	};

	class EntityPropertiesPanel
	{
	public:
		EntityPropertiesPanel() = default;

		// Returns true if something was changed
		bool OnImGuiRender(Entity entity, bool bRuntime, bool bVolumetricsEnabled, bool bDrawWorldTransform = true);
		void SetSelectedComponent(SelectedComponent selectedComponent) { m_SelectedComponent = selectedComponent; }
		SelectedComponent GetSelectedComponent() const { return m_SelectedComponent; }

	private:
		void DrawComponents(Entity& entity);

		template <typename T, typename UIFunction>
		void DrawComponent(const std::string& name, Entity& entity, UIFunction function, bool canRemove = true)
		{
			if (entity.HasComponent<T>())
			{
				ImGui::PushID(int(typeid(T).hash_code()));

				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
					| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
				float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale) + GImGui->Style.FramePadding.y * 2.f;
				ImGui::Separator();
				bool treeOpened = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), flags, name.c_str());

				ImGui::PopStyleVar();

				bool bRemoveComponent = false;

				if (canRemove)
				{
					ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
					if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight })) //•••
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

				ImGui::PopID();

				if (treeOpened)
				{
					T& component = entity.GetComponent<T>();
					function(component);

					ImGui::TreePop();
				}

				if (bRemoveComponent)
				{
					entity.RemoveComponent<T>();
					bEntityChanged = true;
				}
			}
		}

		template <typename T>
		bool DrawComponentLine(const std::string& name, Entity& entity, bool selected)
		{
			bool bClicked = false;
			if (entity.HasComponent<T>())
			{
				ImGuiTreeNodeFlags childFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Leaf
					| ImGuiTreeNodeFlags_SpanAvailWidth | (selected ? ImGuiTreeNodeFlags_Selected : 0);
				bool treeOpened = ImGui::TreeNodeEx((void*)(typeid(T).hash_code() + typeid(Entity).hash_code()), childFlags, name.c_str());

				bClicked = ImGui::IsItemClicked();

				std::string popupID = std::to_string(entity.GetID()) + typeid(T).name();
				if (ImGui::BeginPopupContextItem(popupID.c_str()))
				{
					if (ImGui::MenuItem("Remove Component"))
					{
						m_SelectedComponent = SelectedComponent::None;
						entity.RemoveComponent<T>();
						bEntityChanged = true;
					}
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
		void DrawAddComponentMenuItem(const char* name, const char* typeName)
		{
			if (!m_Entity) return;

			if (m_Entity.HasComponent<T>() == false)
			{
				if (ImGui::MenuItem(name))
				{
					bEntityChanged = true;

					m_Entity.AddComponent<T>();
					EG_CORE_TRACE("Added '{}' to {}", typeName, m_Entity.GetSceneName());

					ImGui::CloseCurrentPopup();
				}
			}
		}

		void DrawEntityTransformNode(Entity& entity);
		void DrawComponentTransformNode(Entity& entity, SceneComponent& sceneComponent);

	private:
		std::unordered_map<Entity, bool> m_InvertEntityRotation;
		std::unordered_map<Entity, bool> m_InvertComponentRotation;
		Entity m_Entity;
		SelectedComponent m_SelectedComponent = SelectedComponent::None;
		bool bRuntime = false;
		bool bVolumetricsEnabled = false;
		bool bDrawWorldTransform = true;
		bool bEntityChanged = false;
	};
}
