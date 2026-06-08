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

	struct AttachedMeshData
	{
		Ref<AssetBaseMesh> Mesh;
		Entity Entity;
	};

	struct AttachedColliderData
	{
		Entity Entity;
		bool bHasBox = false;
		bool bHasSphere = false;
		bool bHasCapsule = false;
	};

	class SkeletalMeshAssetEditor : public AssetEditor
	{
	public:
		SkeletalMeshAssetEditor(const Ref<AssetSkeletalMesh>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		bool DrawSkeletalTree(const SkeletalMeshInfo& skeletalInfo, BoneNode& node, size_t baseHash, bool* outDelete = nullptr, const glm::mat4& baseTransform = glm::mat4(1.f), const std::string& parentName = "");
		bool DrawRagdollTree(SkeletalRagdollBone& node, size_t baseHash);
		bool DrawSkeletalTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash);
		bool DrawRagdollTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash);
		void UpdateGuizmo();
		void OnViewportEnd() override { UpdateGuizmo(); }
		Transform GetSelectedRagdollBoneWorldTransform();
		Transform GetBoneWorldTransform(const std::string& name);

		void CreatePlane();
		void DeletePlane();
		void OnSimulateRagdollChanged();

		void OnBoneNodeDeletion(const BoneNode& node);

		enum class OpenedTabType
		{
			Unknown, Skeletal, Ragdoll
		};

	private:
		Ref<AssetSkeletalMesh> m_Asset;
		Ref<AssetAnimation> m_PreviewAnimation;
		std::string m_WindowName;

		std::string m_SelectedBoneName;
		std::string m_SelectedBoneParentName;
		BoneNode* m_SelectedBone = nullptr;

		std::string m_SelectedRagdollBoneName;
		SkeletalRagdollBone* m_SelectedRagdollBone = nullptr;

		// Key - bone name; value - attached mesh
		std::unordered_map<std::string, AttachedMeshData> m_AttachedToBonesMeshes;
		std::unordered_map<std::string, AttachedColliderData> m_AttachedToBonesColliders;

		OpenedTabType m_OpenedTab = OpenedTabType::Unknown;
		Entity m_Entity;
		GUID m_PlaneEntityGUID;
		float m_MinRagdollBoneSize = 0.1f;
		float m_Twist = 22.5f;
		float m_Swing = 45.f;
		CollisionDetectionType m_CollisionDetection = CollisionDetectionType::Discrete;
		uint32_t m_CollisionGroup = 0;
		uint32_t m_InteractingCollisionGroup = 0;
		bool bGuizmoChanged = false;
		bool bSimulate = false;
		bool bVisualizeRagdollBones = true;
		bool bVisualizeBoneDirection = false;
		bool bEnableDebugLinesDepthTest = false;
	};
}
