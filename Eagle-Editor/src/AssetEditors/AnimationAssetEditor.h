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

		enum class RootMotionPreview
		{
			InPlace,
			Loop,
			LoopAndReset,
		};

		// Where the root motion of one pass of the animation takes an entity, calculated the same way as in the game
		struct RootMotionStats
		{
			float HorizontalDistance = 0.f;
			float VerticalDistance = 0.f;
			float Speed = 0.f; // Horizontal, at the playback speed of 1
			float TurnDegrees = 0.f; // Around the world up. Positive - to the left
			bool bValid = false;
		};
		static RootMotionStats CalculateRootMotionStats(const SkeletalMeshAnimation& animation);

		void UpdateRootMotionPreview();
		void ResetRootMotionPreview();

		glm::vec3 GetPreviewCenter() const;
		void FocusCameraOnPreview();
		void DrawRootMotionPreview();
		void DrawRootMotionStats(const SkeletalMeshAnimation& animation);

	private:
		Ref<AssetAnimation> m_Asset;
		AssetEditTracker<AssetAnimation, AssetAnimation::Settings> m_EditTracker;
		SkeletalMeshComponent* m_Component = nullptr;
		bool bPlayAnimation = true;

		RootMotionPreview m_RootMotionPreview = RootMotionPreview::LoopAndReset;
		bool m_bCameraFollows = true;
		float m_CameraFollowDistance = 1.f; // From the camera to the preview's center, while the camera follows it
		glm::vec3 m_PreviewCenterOffset = glm::vec3(0.f); // The center of the mesh relative to its entity
		RootMotionPath m_RootMotionPath;
		float m_LastPoseTime = 0.f; // To detect when the animation loops

		// Recalculated when the root motion keys change
		RootMotionStats m_RootMotionStats;
		std::tuple<RootMotionMode, size_t, size_t, float, glm::vec3, glm::quat> m_RootMotionStatsKey{};
		bool m_bRootMotionStatsCalculated = false;
	};
}
