#include "egpch.h"
#include "SkeletalMeshAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	static bool HasBoneWithName(const BoneNode& node, const std::string& name)
	{
		if (node.Name == name)
			return true;

		for (const auto& child : node.Children)
			if (HasBoneWithName(child, name))
				return true;

		return false;
	}

	static std::string GetUniqueBoneName(const SkeletalMeshInfo& skeletalInfo)
	{
		const std::string defaultName = "Virtual Bone";
		uint32_t i = 0;

		std::string name = defaultName;
		while (HasBoneWithName(skeletalInfo.RootBone, name))
			name = defaultName + '_' + std::to_string(i++);

		return name;
	}

	// Returns true if found
	static bool GetRootBone_Internal(BoneNode& node, const BonesMap& bonesMap, BoneNode** outNode = nullptr)
	{
		if (auto it = bonesMap.find(node.Name); it != bonesMap.end())
		{
			*outNode = &node;
			return true;
		}

		for (auto& child : node.Children)
		{
			if (GetRootBone_Internal(child, bonesMap, outNode))
			{
				return true;
			}
		}

		return false;
	}

	static BoneNode& GetRootBone(BoneNode& node, const BonesMap& bonesMap)
	{
		BoneNode* rootNode = nullptr;
		if (GetRootBone_Internal(node, bonesMap, &rootNode))
		{
			return *rootNode;
		}
		return node;
	}

	// Returns true if it changed
	bool SkeletalMeshAssetEditor::DrawSkeletalTree(const SkeletalMeshInfo& skeletalInfo, BoneNode& node, size_t baseHash, bool* outDelete, const glm::mat4& baseTransform)
	{
		size_t hash = std::hash<std::string>()(node.Name);
		HashCombine(hash, baseHash);
		bool bChanged = false;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
			| (node.Children.size() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedBone && m_SelectedBone->Name == node.Name ? ImGuiTreeNodeFlags_Selected : 0);

		bool opened = ImGui::TreeNodeEx((void*)hash, flags, node.Name.c_str());
		const glm::mat4 worldTr = baseTransform * node.Transformation;

		if (ImGui::IsItemClicked())
		{
			m_SelectedBoneName = node.Name;
			m_SelectedBoneParentWorldTr = baseTransform;
			m_SelectedBone = &node;
		}

		if (ImGui::BeginPopupContextItem(nullptr))
		{
			if (ImGui::MenuItem("Add virtual bone"))
			{
				auto& bone = node.Children.emplace_back();
				bone.Name = GetUniqueBoneName(skeletalInfo);
				bone.Transformation = glm::mat4(1.f);
				bone.bVirtualBone = true;
				bChanged = true;
			}
			if (node.bVirtualBone)
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Delete"))
				{
					if (m_SelectedBone && m_SelectedBone->Name == node.Name)
					{
						m_SelectedBoneName.clear();
						m_SelectedBone = nullptr;
					}

					*outDelete = true;
					bChanged = true;
				}
			}

			ImGui::EndPopup();
		}

		if (opened)
		{
			for (auto it = node.Children.begin(); it != node.Children.end(); )
			{
				auto& child = *it;

				bool bDelete = false;
				bChanged |= DrawSkeletalTree(skeletalInfo, child, baseHash, &bDelete, worldTr);

				if (bDelete)
					it = node.Children.erase(it);
				else
					++it;
			}

			ImGui::TreePop();
		}

		return bChanged;
	}

	bool SkeletalMeshAssetEditor::DrawRagdollTree(SkeletalRagdollBones& node, size_t baseHash)
	{
		size_t hash = std::hash<std::string>()(node.Name);
		HashCombine(hash, baseHash);
		bool bChanged = false;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
			| (node.Children.size() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedRagdollBone && m_SelectedRagdollBone->Name == node.Name ? ImGuiTreeNodeFlags_Selected : 0);

		bool opened = ImGui::TreeNodeEx((void*)hash, flags, node.Name.c_str());

		if (ImGui::IsItemClicked())
		{
			m_SelectedRagdollBoneName = node.Name;
			m_SelectedRagdollBone = &node;
		}

		if (opened)
		{
			for (auto& child : node.Children)
			{
				bChanged |= DrawRagdollTree(child, baseHash);
			}

			ImGui::TreePop();
		}

		return bChanged;
	}

	SkeletalMeshAssetEditor::SkeletalMeshAssetEditor(const Ref<AssetSkeletalMesh>& asset)
		: AssetEditor(true), m_Asset(asset)
	{
		const auto& mesh = m_Asset->GetMesh();
		m_MinRagdollBoneSize = mesh->GetMinRagdollBoneSize();
		m_Twist = mesh->GetRagdollMaxTwist();
		m_Swing = mesh->GetRagdollMaxSwing();

		m_Entity = m_Scene->CreateEntity("SkeletalMeshAssetEditor");
		auto& component = m_Entity.AddComponent<SkeletalMeshComponent>();
		component.SetMeshAsset(m_Asset);
		component.SetRagdollEnabled(true);

		auto& camera = m_Scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = mesh->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.LookAt(center);
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
	}

	void SkeletalMeshAssetEditor::OnImGuiRender(bool* pOpen)
	{
		auto& component = m_Entity.GetComponent<SkeletalMeshComponent>();
		auto& mesh = m_Asset->GetMesh();
		const size_t verticesCount = mesh->GetVerticesCount();
		size_t indicesCount = 0;
		for (uint32_t i = 0; i < mesh->GetMaterialSlotsCount(); ++i)
			indicesCount += mesh->GetIndicesCount(i);
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("SkeletalMeshDetails");

		UI::TextWithSeparator("Data");
		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Skeletal Mesh");
		UI::Text("Vertices", std::to_string(verticesCount));
		UI::Text("Indices", std::to_string(indicesCount));
		UI::Text("Vertices Mem Usage (Kb)", std::to_string(verticesCount * sizeof(SkeletalVertex) / 1024));
		UI::Text("Indices Mem Usage (Kb)", std::to_string(indicesCount * sizeof(Index) / 1024));

		UI::TextWithSeparator("Materials");

		const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
		for (uint32_t i = 0; i < materialsCount; ++i)
		{
			auto materialAsset = mesh->GetMaterialAsset(i);
			if (UI::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
			{
				mesh->SetMaterialAsset(i, materialAsset);
				component.SetMaterialAsset(i, materialAsset);
				bChanged = true;
			}
		}

		UI::EndPropertyGrid();

		size_t assetHash = m_Asset->GetGUID().GetHash();


		ImGui::Separator();
		{
			if (ImGui::BeginTabBar("MyTabBar"))
			{
				bChanged |= DrawSkeletalTab(mesh, assetHash);
				bChanged |= DrawRagdollTab(mesh, assetHash);
				ImGui::EndTabBar();
			}
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		DrawViewport();
		bChanged |= bGuizmoChanged;
		bGuizmoChanged = false;

		if (bChanged)
			m_Asset->SetDirty(true);
	}

	bool SkeletalMeshAssetEditor::DrawSkeletalTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash)
	{
		const bool bTabOpened = ImGui::BeginTabItem("Skeletal tree");
		if (!bTabOpened)
			return false;

		if (m_OpenedTab != OpenedTabType::Skeletal)
		{
			m_Entity.GetComponent<SkeletalMeshComponent>().SetRagdollEnabled(false);
		}
		m_OpenedTab = OpenedTabType::Skeletal;

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		bool bChanged = false;
		const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash++), treeFlags, "Skeletal tree");
		if (bOpened)
		{
			auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
			auto& root = GetRootBone(skeletalInfo.RootBone, skeletalInfo.BoneInfoMap);
			bChanged |= DrawSkeletalTree(skeletalInfo, root, assetHash);

			if (m_SelectedBone)
			{
				ImGui::Separator();

				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

				bool bOpenedDetails = ImGui::TreeNodeEx((void*)(assetHash++), flags, "Details");
				if (bOpenedDetails)
				{
					if (m_SelectedBone->bVirtualBone == false)
						UI::PushItemDisabled();

					Transform boneTransform = Math::DecomposeTransformMatrix(m_SelectedBone->Transformation);

					bool bTransformChanged = false;

					const glm::quat q = boneTransform.Rotation.GetQuat();
					glm::vec4 quat(q.x, q.y, q.z, q.w);

					bool bStoppedEditing = false;
					if (UI::InputText("Name", m_SelectedBoneName, ImGuiInputTextFlags_EnterReturnsTrue, "Only user-created (virtual) bones can be modified"))
						bStoppedEditing = true;

					// Lost focus, stop editing
					if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						bStoppedEditing = true;

					if (bStoppedEditing && m_SelectedBoneName != m_SelectedBone->Name)
					{
						if (HasBoneWithName(skeletalInfo.RootBone, m_SelectedBoneName))
						{
							Application::Get().GetImGuiLayer()->AddMessage("Failed to rename the bone. The name is already taken!");
							m_SelectedBoneName = m_SelectedBone->Name;
						}
						else
						{
							m_SelectedBone->Name = m_SelectedBoneName;
							bChanged = true;
						}
					}

					bTransformChanged |= UI::DrawVec3Control("Location", boneTransform.Location, glm::vec3{ 0.f });
					if (UI::DrawVec4Control("Rotation (Quat)", quat, glm::vec4{ 0, 0, 0, 1 }))
					{
						if (glm::all(glm::epsilonEqual(quat, glm::vec4(0), 0.001f)))
							quat.w = 1.f;
						quat = glm::normalize(quat);
						boneTransform.Rotation = glm::quat(quat.w, quat.x, quat.y, quat.z);
						bTransformChanged = true;
					}
					bTransformChanged |= UI::DrawVec3Control("Scale", boneTransform.Scale3D, glm::vec3{ 1.f });

					if (bTransformChanged)
					{
						m_SelectedBone->Transformation = Math::ToTransformMatrix(boneTransform);
						bChanged = true;
					}

					if (m_SelectedBone->bVirtualBone == false)
						UI::PopItemDisabled();

					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}
	
		ImGui::EndTabItem();
		return bChanged;
	}

	bool SkeletalMeshAssetEditor::DrawRagdollTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash)
	{
		const bool bTabOpened = ImGui::BeginTabItem("Ragdoll tree");
		if (!bTabOpened)
			return false;

		if (m_OpenedTab != OpenedTabType::Ragdoll)
		{
			auto& comp = m_Entity.GetComponent<SkeletalMeshComponent>();
			comp.SetRagdollEnabled(true);
			comp.SetShowRagdollCollision(true);
		}
		m_OpenedTab = OpenedTabType::Ragdoll;

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		bool bChanged = false;
		const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash++), treeFlags, "Ragdoll tree");
		if (bOpened)
		{
			auto& mesh = m_Asset->GetMesh();
			DrawRagdollTree(mesh->GetRagdollRoot(), assetHash);

			if (m_SelectedRagdollBone)
			{
				ImGui::Separator();

				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

				bool bOpenedDetails = ImGui::TreeNodeEx((void*)(assetHash++), flags, "Details");
				if (bOpenedDetails)
				{
					bool bRagdollChanged = false;
					UI::BeginPropertyGrid("Details");
					UI::Text("Name", m_SelectedRagdollBoneName);
					if (UI::PropertyDrag("Mass", m_SelectedRagdollBone->Settings.Mass, 0.5f))
					{
						m_SelectedRagdollBone->Settings.Mass = glm::max(0.f, m_SelectedRagdollBone->Settings.Mass);
						bRagdollChanged = true;
					}
					if (UI::PropertyDrag("Linear Damping", m_SelectedRagdollBone->Settings.LinearDamping, 0.1f))
					{
						m_SelectedRagdollBone->Settings.LinearDamping = glm::max(0.f, m_SelectedRagdollBone->Settings.LinearDamping);
						bRagdollChanged = true;
					}
					if (UI::PropertyDrag("Angular Damping", m_SelectedRagdollBone->Settings.AngularDamping, 0.1f))
					{
						m_SelectedRagdollBone->Settings.AngularDamping = glm::max(0.f, m_SelectedRagdollBone->Settings.AngularDamping);
						bRagdollChanged = true;
					}
					bRagdollChanged |= UI::DrawAssetSelection("Material", m_SelectedRagdollBone->Settings.Material);
					bRagdollChanged |= UI::ComboEnum("Shape", m_SelectedRagdollBone->Settings.Shape);

					UI::EndPropertyGrid();
					ImGui::Separator();

					Transform& boneTransform = m_SelectedRagdollBone->Settings.UserOffset;

					bool bTransformChanged = false;
					const glm::quat q = boneTransform.Rotation.GetQuat();
					glm::vec4 quat(q.x, q.y, q.z, q.w);

					bTransformChanged |= UI::DrawVec3Control("Location", boneTransform.Location, glm::vec3{ 0.f });
					if (UI::DrawVec4Control("Rotation (Quat)", quat, glm::vec4{ 0, 0, 0, 1 }))
					{
						if (glm::all(glm::epsilonEqual(quat, glm::vec4(0), 0.001f)))
							quat.w = 1.f;
						quat = glm::normalize(quat);
						boneTransform.Rotation = glm::quat(quat.w, quat.x, quat.y, quat.z);
						bTransformChanged = true;
					}
					bTransformChanged |= UI::DrawVec3Control("Scale", boneTransform.Scale3D, glm::vec3{ 1.f });

					if (bTransformChanged)
					{
						bRagdollChanged = true;
					}

					if (bRagdollChanged)
						OnRagdollModified();

					bChanged |= bRagdollChanged;

					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}

		ImGui::Separator();

		UI::BeginPropertyGrid("Ragdoll props");
		if (UI::PropertyDrag("Min ragdoll bone size", m_MinRagdollBoneSize, 0.05f, 0.01f))
			m_MinRagdollBoneSize = glm::max(m_MinRagdollBoneSize, 0.05f);
		UI::PropertyDrag("Max Twist angle", m_Twist, 1.f, 0.01f, 180.f);
		UI::PropertyDrag("Max Swing angle", m_Swing, 1.f, 0.01f, 180.f);
		UI::EndPropertyGrid();

		ImGui::Separator();
		if (ImGui::Button("Regenerate"))
		{
			auto& mesh = m_Asset->GetMesh();
			mesh->SetRagdollMaxTwist(m_Twist);
			mesh->SetRagdollMaxSwing(m_Swing);
			mesh->RegenerateRagdollData(m_MinRagdollBoneSize);
			OnRagdollModified();
			bChanged = true;
		}

		ImGui::EndTabItem();
		return bChanged;
	}
	
	void SkeletalMeshAssetEditor::UpdateGuizmo()
	{
		const int id = int(m_Entity.GetID());

		if (m_OpenedTab == OpenedTabType::Skeletal && m_SelectedBone)
		{
			const bool bEnableModification = m_SelectedBone->bVirtualBone;
			Transform boneTransform = Math::DecomposeTransformMatrix(m_SelectedBoneParentWorldTr * m_SelectedBone->Transformation);
			if (DrawGuizmo(boneTransform, id, bEnableModification))
			{
				m_SelectedBone->Transformation = glm::inverse(m_SelectedBoneParentWorldTr) * Math::ToTransformMatrix(boneTransform);
				bGuizmoChanged = true;
			}
		}
		else if (m_OpenedTab == OpenedTabType::Ragdoll && m_SelectedRagdollBone)
		{
			const bool bEnableModification = true;
			const glm::vec3 worldLocation = GetSelectedRagdollBoneWorldTransform().Location;
			Transform boneTransform = m_SelectedRagdollBone->Settings.UserOffset;
			const glm::vec3 origOffsetLocation = boneTransform.Location;
			boneTransform.Location = worldLocation; // We wanna draw guizmo in WS
			if (DrawGuizmo(boneTransform, id, bEnableModification))
			{
				const glm::vec3 diff = boneTransform.Location - worldLocation;
				boneTransform.Location = origOffsetLocation + diff; // Back to local
				m_SelectedRagdollBone->Settings.UserOffset = boneTransform;
				OnRagdollModified();
				bGuizmoChanged = true;
			}
		}
	}
	
	Transform SkeletalMeshAssetEditor::GetSelectedRagdollBoneWorldTransform()
	{
		Transform transform = m_Entity.GetComponent<SkeletalMeshComponent>().GetRagdollBoneWorldTransform(m_SelectedRagdollBoneName);
		transform.Scale3D = m_SelectedRagdollBone->Settings.UserOffset.Scale3D; // Originally, bones don't have scale, so we restore it
		return transform;
	}
	
	void SkeletalMeshAssetEditor::OnRagdollModified()
	{
		m_Asset->OnModified();
		auto& comp = m_Entity.GetComponent<SkeletalMeshComponent>();
		comp.SetShowRagdollCollision(true);
	}
}
