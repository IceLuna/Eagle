#pragma once

#include "AssetEditor.h"
#include "Eagle/Core/Transform.h"

namespace Eagle
{
	class AssetSkeletalMesh;
	struct SkeletalMeshInfo;
	struct BoneNode;

	class SkeletalMeshAssetEditor : public AssetEditor
	{
	public:
		SkeletalMeshAssetEditor(const Ref<AssetSkeletalMesh>& asset) : m_Asset(asset) {}

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		bool DrawSkeletalTree(const SkeletalMeshInfo& skeletalInfo, BoneNode& node, size_t baseHash, bool* outDelete = nullptr);

	private:
		Ref<AssetSkeletalMesh> m_Asset;
		std::string m_SelectedBoneName;
		Transform m_SelectedBoneTransform;
		BoneNode* m_SelectedBone = nullptr;
	};
}
