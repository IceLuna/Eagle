#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

#include "Eagle/Animation/Animation.h"

namespace Eagle
{
	class AssetAnimation;
	class SkeletalMeshComponent;

	class AnimationAssetEditor : public AssetEditor
	{
	public:
		AnimationAssetEditor(const Ref<AssetAnimation>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		// Edits of the settings are tracked by `m_EditTracker`.
		// Extracting & removing the root motion changes the keyframes, so they're recorded separately
		struct AnimationData
		{
			SkeletalMeshAnimation Animation;

			size_t GetMemoryUsage() const;
		};

		static std::string DescribeState(const AssetAnimation::Settings& before, const AssetAnimation::Settings& after);

	private:
		Ref<AssetAnimation> m_Asset;
		AssetEditTracker<AssetAnimation, AssetAnimation::Settings> m_EditTracker;
		SkeletalMeshComponent* m_Component = nullptr;
		bool bPlayAnimation = true;
		bool bInPlace = true;
	};
}
