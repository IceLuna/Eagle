#include "egpch.h"
#include "SkeletalMeshAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Math/Math.h"

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

	// Returns true if it changed
	bool SkeletalMeshAssetEditor::DrawSkeletalTree(const SkeletalMeshInfo& skeletalInfo, BoneNode& node, size_t baseHash, bool* outDelete)
	{
		size_t hash = std::hash<std::string>()(node.Name);
		HashCombine(hash, baseHash);
		bool bChanged = false;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
			| (node.Children.size() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedBone && m_SelectedBone->Name == node.Name ? ImGuiTreeNodeFlags_Selected : 0);

		bool opened = ImGui::TreeNodeEx((void*)hash, flags, node.Name.c_str());

		if (ImGui::IsItemClicked())
		{
			m_SelectedBoneName = node.Name;
			m_SelectedBoneTransform = Math::DecomposeTransformMatrix(node.Transformation);
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
				bChanged |= DrawSkeletalTree(skeletalInfo, child, baseHash, &bDelete);

				if (bDelete)
					it = node.Children.erase(it);
				else
					++it;
			}

			ImGui::TreePop();
		}

		return bChanged;
	}

	void SkeletalMeshAssetEditor::OnImGuiRender(bool* pOpen)
	{
		auto& mesh = m_Asset->GetMesh();
		const size_t verticesCount = mesh->GetVerticesCount();
		const size_t indicesCount = mesh->GetIndicesCount();
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("SkeletalMeshDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Vertices", std::to_string(verticesCount));
		UI::Text("Indices", std::to_string(indicesCount));
		UI::Text("Vertices Mem Usage (Kb)", std::to_string(verticesCount * sizeof(SkeletalVertex) / 1024));
		UI::Text("Indices Mem Usage (Kb)", std::to_string(indicesCount * sizeof(Index) / 1024));
		ImGui::Separator();

		UI::EndPropertyGrid();

		size_t assetHash = m_Asset->GetGUID().GetHash();
		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		ImGui::Separator();
		{
			const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash++), treeFlags, "Skeletal Tree");
			if (bOpened)
			{
				auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
				auto& root = skeletalInfo.RootBone.Children.size() == 1 ? skeletalInfo.RootBone.Children[0] : skeletalInfo.RootBone;
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

						bool bTransformChanged = false;
						bool bRotationChanged = false;
						glm::vec3 rotationInDegrees = glm::degrees(m_SelectedBoneTransform.Rotation.EulerAngles());

						bool bStoppedEditing = false;
						if (UI::InputText("Name", m_SelectedBoneName, ImGuiInputTextFlags_EnterReturnsTrue))
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

						bTransformChanged |= UI::DrawVec3Control("Location", m_SelectedBoneTransform.Location, glm::vec3{ 0.f });
						bRotationChanged = UI::DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{ 0.f });
						bTransformChanged |= UI::DrawVec3Control("Scale", m_SelectedBoneTransform.Scale3D, glm::vec3{ 1.f });
						bTransformChanged |= bRotationChanged;

						if (bRotationChanged)
						{
							m_SelectedBoneTransform.Rotation = Rotator::FromEulerAngles(glm::radians(rotationInDegrees));
						}
						if (bTransformChanged)
						{
							m_SelectedBone->Transformation = Math::ToTransformMatrix(m_SelectedBoneTransform);
							bChanged = true;
						}

						if (m_SelectedBone->bVirtualBone == false)
							UI::PopItemDisabled();

						ImGui::TreePop();
					}
				}

				ImGui::TreePop();
			}
		}

		if (bChanged)
			m_Asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
	}
}
