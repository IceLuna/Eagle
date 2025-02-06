#pragma once

#include "AssetEditor.h"
#include "Eagle/Math/Transform.h"
#include "Eagle/Core/Entity.h"

namespace Eagle
{
	class SkeletalMesh;
	class AssetSkeletalMesh;
	class AssetAnimation;
	struct SkeletalMeshInfo;
	struct BoneNode;

	class SkeletalMeshAssetEditor : public AssetEditor
	{
	public:
		SkeletalMeshAssetEditor(const Ref<AssetSkeletalMesh>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		bool DrawSkeletalTree(const SkeletalMeshInfo& skeletalInfo, BoneNode& node, size_t baseHash, bool* outDelete = nullptr, const glm::mat4& baseTransform = glm::mat4(1.f));
		bool DrawRagdollTree(SkeletalRagdollBones& node, size_t baseHash);
		bool DrawSkeletalTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash);
		bool DrawRagdollTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash);
		void UpdateGuizmo();
		void OnViewportEnd() override { UpdateGuizmo(); }
		Transform GetSelectedRagdollBoneWorldTransform();

		void CreatePlane();
		void DeletePlane();
		void OnSimulateRagdollChanged();

		enum class OpenedTabType
		{
			Skeletal, Ragdoll
		};

	private:
		Ref<AssetSkeletalMesh> m_Asset;
		Ref<AssetAnimation> m_PreviewAnimation;

		std::string m_SelectedBoneName;
		glm::mat4 m_SelectedBoneParentWorldTr = glm::mat4(1.f);
		BoneNode* m_SelectedBone = nullptr;

		std::string m_SelectedRagdollBoneName;
		SkeletalRagdollBones* m_SelectedRagdollBone = nullptr;

		OpenedTabType m_OpenedTab = OpenedTabType::Ragdoll;
		Entity m_Entity;
		Entity m_Plane;
		float m_MinRagdollBoneSize = 0.1f;
		float m_Twist = 22.5f;
		float m_Swing = 45.f;
		bool bGuizmoChanged = false;
		bool bSimulate = false;
	};
}
