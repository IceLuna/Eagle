#include "egpch.h"
#include "AnimationAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Math/Math.h"

namespace Eagle
{
	static const char* s_VisualizationSettingsHelpMsg = "These settings are only for the preview in this editor.\n"
		"They don't change the animation asset or any scene, and they aren't saved";

	static const char* s_RootMotionPreviewHelpMsg = "In Place - root motion isn't applied.\n"
		"Loop - root motion moves the preview, as it moves an entity in the game.\n"
		"Loop And Reset - the same, but the preview goes back to where it started whenever the animation loops.\n\n"
		"While root motion is applied, the viewport shows:\n"
		"- White cross - where the preview started (or was last reset).\n"
		"- Orange line - the path its root motion moved it along.\n"
		"- Blue arrow - where the entity faces (its forward, -Z). Only root motion rotation turns it. "
		"If it doesn't point where the character faces while it walks forward, the mesh doesn't face -Z";

	AnimationAssetEditor::AnimationAssetEditor(const Ref<AssetAnimation>& asset)
		: AssetEditor(true)
		, m_Asset(asset)
		, m_EditTracker(&DescribeState)
	{
		const auto& skeletalMeshAsset = m_Asset->GetSkeletal();
		const auto& scene = GetCurrentScene();

		Entity entity = scene->CreateEntity("AnimationAssetEditor");
		m_Component = &entity.AddComponent<SkeletalMeshComponent>();
		m_Component->SetMeshAsset(skeletalMeshAsset);
		m_Component->SetAnimationAsset(m_Asset);
		m_Component->AnimType = AnimationType::Clip;
		m_Component->SetRootMotionLockFlag(RootMotionLockFlag::None);

		auto& camera = scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = skeletalMeshAsset->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		m_CameraFollowDistance = aabb.MaxSide() * 2.f;
		m_PreviewCenterOffset = center;
		camera.SetLocation(center - cameraDir * m_CameraFollowDistance); // Move back
		camera.LookAt(center);

		ResetRootMotionPreview();
	}

	void AnimationAssetEditor::OnImGuiRender(bool* pOpen)
	{
		auto& animation = m_Asset->GetAnimation();
		bool bChanged = false;
		const std::string durationSec = std::to_string(animation->Duration / animation->TicksPerSecond);

		DrawEditorWindow(pOpen);
		BeginPanel(GetDetailsPanelName());
		m_EditTracker.BeginFrame(&m_UndoHistory, m_Asset);

		UI::BeginPropertyGrid("AnimationDetails");

		UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
		UI::Text("Type", "Animation");
		AssetEditor::DrawAlwaysIncludeInGameBuilds(m_Asset);
		UI::Text("Duration", std::to_string(animation->Duration) + " ticks (" + durationSec + " seconds)");
		UI::Text("Ticks per Second", std::to_string(animation->TicksPerSecond));

		ImGui::Separator();

		bChanged |= UI::Property("In Place", animation->Settings.bInPlace);

		RootMotionMode rootMotionMode = animation->RootMotionType;
		if (UI::ComboEnum("Root Motion Mode", rootMotionMode))
		{
			// It changes the keyframes, so the whole animation is recorded. It's too heavy to be captured by the edit tracker on every edit.
			// It's recorded manually, so the edit in progress is recorded first
			m_UndoHistory.FlushPendingEdits();

			AnimationData before{ *animation };

			const auto& skeletalInfo = m_Asset->GetSkeletal()->GetMesh()->GetSkeletalMeshInfo();
			bool bRootMotionChanged = false;
			if (rootMotionMode != RootMotionMode::Disabled)
				bRootMotionChanged = animation->ExtractRootMotion(skeletalInfo, rootMotionMode);
			else
				bRootMotionChanged = animation->RemoveRootMotion(skeletalInfo);

			if (bRootMotionChanged)
			{
				m_UndoHistory.Record(rootMotionMode != RootMotionMode::Disabled ? "Extract Root Motion" : "Remove Root Motion",
					MakeScope<AssetStateCommand<AssetAnimation, AnimationData>>(m_Asset, std::move(before), AnimationData{ *animation },
						[](const Ref<AssetAnimation>& asset, const AnimationData& data) { *asset->GetAnimation() = data.Animation; }));
				bChanged = true;
				ResetRootMotionPreview();
			}
		}

		DrawRootMotionStats(*animation);

		UI::EndPropertyGrid();

		ImGui::Separator();
		if (UI::PushTreeNode("Metadata"))
		{
			UI::BeginPropertyGrid("AnimationDetails");
			const std::string path = Utils::AsString(m_Asset->GetPathToRaw());
			UI::Text("Path to raw", path);
			if (!path.empty())
				UI::Tooltip(path);
			UI::EndPropertyGrid();
			if (ImGui::Button("Change..."))
			{
				Path path = FileDialog::OpenFile(FileDialog::IMPORT_FILTER);
				if (std::filesystem::exists(path))
				{
					m_Asset->SetPathToRaw(path);
					bChanged = true;
				}
			}

			UI::PopTreeNode();
		}

		ImGui::Separator();
		// Events tree
		{
			const size_t assetHash = m_Asset->GetGUID().GetHash();
			size_t hashOffset = 0;
			constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

			const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
			const float lineHeight = (ImGui::GetFontBaked()->Size * ImGui::GetFont()->Scale) + ImGui::GetStyle().FramePadding.y * 2.f;

			const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash + hashOffset++), treeFlags, "Events");

			// Add Event button
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.45f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.7f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.35f, 0.f, 1.f));

				const char* addEventText = "Add Event";
				const float textWidth = ImGui::CalcTextSize(addEventText, NULL, true).x;
				ImGui::SameLine(contentRegionAvailable.x - textWidth);
				if (ImGui::Button(addEventText, ImVec2{ textWidth + ImGui::GetStyle().FramePadding.y * 2.f, lineHeight }))
				{
					auto& event = animation->Settings.Events.emplace_back();
					event.Time = m_Component->GetClipPlayTime();
					bChanged = true;
				}

				ImGui::PopStyleColor(3);
			}

			if (bOpened)
			{
				constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
				for (auto it = animation->Settings.Events.begin(); it != animation->Settings.Events.end();)
				{
					auto& event = *it;

					const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash + hashOffset++), flags, event.Name.c_str());

					bool bDelete = false;
					if (ImGui::BeginPopupContextItem(nullptr))
					{
						bDelete = ImGui::MenuItem("Delete event");
						ImGui::EndPopup();
					}

					if (bOpened)
					{
						UI::BeginPropertyGrid("Animation Events");
						bChanged |= UI::PropertyText("Name", event.Name, "When the event is triggered, C# `Entity.OnAnimationEvent()` is called with this name as a parameter");
						if (UI::PropertySlider("Time", event.Time, 0, animation->Duration, "A value between [0; Duration] when an event should be triggered"))
						{
							event.Time = glm::clamp(event.Time, 0.f, animation->Duration);
							bChanged = true;
						}
						UI::EndPropertyGrid();

						ImGui::TreePop();
					}

					if (bDelete)
					{
						it = animation->Settings.Events.erase(it);
						bChanged = true;
					}
					else
						++it;
				}

				ImGui::TreePop();
			}
		}

		if (bChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		ImGui::Separator();
		if (UI::PushTreeNode("Visualization Settings", true, true, s_VisualizationSettingsHelpMsg))
		{
			UI::BeginPropertyGrid("AnimationDetails");

			UI::Property("Play animation", bPlayAnimation);
			UI::Property("Looping", m_Component->bClipLooping);
			UI::PropertyDrag("Playback Speed", m_Component->ClipPlaybackSpeed, 0.1f);

			const float duration = m_Asset->GetAnimation()->Duration;
			float playbackPosition = m_Component->GetClipPlayTime();
			if (UI::PropertySlider("Playback Position", playbackPosition, 0.f, duration))
			{
				m_Component->SetClipPlayTime(playbackPosition);
				m_LastPoseTime = m_Component->GetClipPlayTime(); // Moving it back isn't a loop
			}

			ImGui::Separator();
			if (UI::ComboEnum("Root Motion Preview", m_RootMotionPreview, s_RootMotionPreviewHelpMsg))
				ResetRootMotionPreview();

			const bool bDisable = m_RootMotionPreview == RootMotionPreview::InPlace;
			if (bDisable)
				UI::PushItemDisabled();
			{
				static const std::vector<std::string> s_LockStrings = { "X", "Y", "Z" };
				const RootMotionLockFlag lockFlags = m_Component->GetRootMotionLockFlags();
				bool bLockPositions[3] = { HasFlags(lockFlags, RootMotionLockFlag::PositionX), HasFlags(lockFlags, RootMotionLockFlag::PositionY), HasFlags(lockFlags, RootMotionLockFlag::PositionZ) };
				if (UI::Property("Root Motion Lock Position", s_LockStrings, bLockPositions, "Locked axes of the root motion aren't applied. For example, lock Y to see how it moves on the ground"))
				{
					m_Component->SetRootMotionLockFlag(RootMotionLockFlag::PositionX, bLockPositions[0]);
					m_Component->SetRootMotionLockFlag(RootMotionLockFlag::PositionY, bLockPositions[1]);
					m_Component->SetRootMotionLockFlag(RootMotionLockFlag::PositionZ, bLockPositions[2]);
				}

				if (UI::Property("Camera Follows", m_bCameraFollows, "The camera moves with the preview, so it stays in view") && m_bCameraFollows)
					FocusCameraOnPreview();
				if (UI::Button("Preview Location", "Reset"))
					ResetRootMotionPreview();
			}
			if (bDisable)
				UI::PopItemDisabled();

			UI::EndPropertyGrid();

			if (!animation->HasRootMotion())
				ImGui::TextWrapped("The animation has no root motion enabled. To extract it, set its `Root Motion Mode`");
			else if (animation->Settings.bInPlace)
				ImGui::TextWrapped("The animation is `In Place`, so its root motion isn't applied");

			UI::PopTreeNode();
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
		m_EditTracker.EndFrame();

		DrawRootMotionPreview(); // Before the scene is rendered
		DrawViewport(true);
		UpdateRootMotionPreview();

		// Remembered, so that the camera is put back at the same distance when it starts following again
		if (m_bCameraFollows)
			m_CameraFollowDistance = glm::max(glm::distance(GetCurrentScene()->EditorCamera.GetLocation(), GetPreviewCenter()), 0.1f);

		if (!bPlayAnimation)
			m_Component->SetClipPlayTime(m_Component->GetPrevClipPlayTime()); // Prevent animation from advancing
	}

	void AnimationAssetEditor::UpdateRootMotionPreview()
	{
		// The scene is updated only while the viewport is visible. Otherwise, the root motion of the last update would be applied again
		if (!bViewportVisible)
			return;

		// The previous time is the time of the pose that was just calculated
		const float poseTime = m_Component->GetPrevClipPlayTime();
		const float speed = m_Component->ClipPlaybackSpeed;
		const bool bLooped = m_Component->bClipLooping && (speed > 0.f ? poseTime < m_LastPoseTime : (speed < 0.f && poseTime > m_LastPoseTime));
		m_LastPoseTime = poseTime;

		if (m_RootMotionPreview == RootMotionPreview::InPlace || !m_Component->LastPose.HasRootMotion())
			return;

		if (bLooped && m_RootMotionPreview == RootMotionPreview::LoopAndReset)
		{
			ResetRootMotionPreview();
			return;
		}

		Entity& entity = m_Component->Parent;
		const glm::vec3 before = entity.GetWorldLocation();
		AnimationSystem::ApplyRootMotion(m_Component, Application::Get().GetTimestep());
		const glm::vec3 after = entity.GetWorldLocation();

		if (m_bCameraFollows)
		{
			auto& camera = GetCurrentScene()->EditorCamera;
			camera.SetLocation(camera.GetLocation() + (after - before));
		}

		constexpr float minTrailStep = 0.02f;
		constexpr size_t maxTrailPoints = 4096;
		if (m_Trail.empty() || glm::distance(m_Trail.back(), after) > minTrailStep)
		{
			if (m_Trail.size() >= maxTrailPoints)
				m_Trail.erase(m_Trail.begin(), m_Trail.begin() + maxTrailPoints / 2);
			m_Trail.push_back(after);
		}
	}

	void AnimationAssetEditor::ResetRootMotionPreview()
	{
		Entity& entity = m_Component->Parent;
		const glm::vec3 before = entity.GetWorldLocation();
		entity.SetWorldTransform(Transform{});

		if (m_bCameraFollows)
		{
			auto& camera = GetCurrentScene()->EditorCamera;
			camera.SetLocation(camera.GetLocation() - before);
		}

		m_Trail.clear();
		m_Trail.push_back(glm::vec3(0.f));
	}

	glm::vec3 AnimationAssetEditor::GetPreviewCenter() const
	{
		const Transform& transform = m_Component->Parent.GetWorldTransform();
		return transform.Location + glm::rotate(transform.Rotation.GetQuat(), m_PreviewCenterOffset * transform.Scale3D);
	}

	void AnimationAssetEditor::FocusCameraOnPreview()
	{
		auto& camera = GetCurrentScene()->EditorCamera;
		camera.SetLocation(GetPreviewCenter() - camera.GetForwardVector() * m_CameraFollowDistance);
	}

	void AnimationAssetEditor::DrawRootMotionPreview()
	{
		if (m_RootMotionPreview == RootMotionPreview::InPlace)
			return;

		auto& scene = GetCurrentScene();
		auto drawLine = [&scene](const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
		{
			RendererLine line;
			line.Start.Location = start;
			line.Start.Color = color;
			line.End.Location = end;
			line.End.Color = color;
			scene->DrawDebugLine(line);
		};

		// Slightly above the ground, so that the grid doesn't hide it
		const glm::vec3 lift = glm::vec3(0.f, 0.01f, 0.f);

		// Where it started
		const glm::vec3 startColor = glm::vec3(1.f);
		constexpr float crossSize = 0.15f;
		drawLine(lift + glm::vec3(-crossSize, 0.f, -crossSize), lift + glm::vec3(crossSize, 0.f, crossSize), startColor);
		drawLine(lift + glm::vec3(-crossSize, 0.f, crossSize), lift + glm::vec3(crossSize, 0.f, -crossSize), startColor);

		// Where it went
		const glm::vec3 trailColor = glm::vec3(1.f, 0.75f, 0.1f);
		for (size_t i = 1; i < m_Trail.size(); ++i)
			drawLine(m_Trail[i - 1] + lift, m_Trail[i] + lift, trailColor);

		// Where it faces, to see root motion rotation
		Entity& entity = m_Component->Parent;
		const glm::vec3 location = entity.GetWorldLocation() + lift;
		glm::vec3 forward = entity.GetForwardVector();
		forward.y = 0.f;
		if (glm::dot(forward, forward) > 1e-6f)
		{
			forward = glm::normalize(forward);
			const glm::vec3 right = glm::vec3(-forward.z, 0.f, forward.x);
			const glm::vec3 tip = location + forward * 0.6f;
			const glm::vec3 arrowColor = glm::vec3(0.2f, 0.8f, 1.f);
			drawLine(location, tip, arrowColor);
			drawLine(tip, tip - forward * 0.15f + right * 0.08f, arrowColor);
			drawLine(tip, tip - forward * 0.15f - right * 0.08f, arrowColor);
		}
	}

	AnimationAssetEditor::RootMotionStats AnimationAssetEditor::CalculateRootMotionStats(const SkeletalMeshAnimation& animation)
	{
		RootMotionStats stats;
		if (!animation.HasRootMotion() || animation.Duration <= 0.f || animation.TicksPerSecond <= 0.f)
			return stats;

		// It's stepped through like the game plays it: each step's root motion is converted with the rotation so far
		const float seconds = animation.Duration / animation.TicksPerSecond;
		const uint32_t steps = glm::clamp(uint32_t(seconds * 60.f), 1u, 2048u);

		Transform entityTransform;
		Transform totalRootMotion;
		float prevTime = 0.f;
		for (uint32_t i = 1; i <= steps; ++i)
		{
			const float time = animation.Duration * (float(i) / float(steps));
			Transform rootMotion = AnimationSystem::CalculateRootMotion(&animation, time, prevTime, 1.f, 0.f, &totalRootMotion);
			rootMotion.Location = Math::RootMotionLocationToWorld(rootMotion.Location, totalRootMotion.Rotation, entityTransform.Rotation);
			entityTransform += rootMotion; // The same as `AnimationSystem::ApplyRootMotion` for an entity without physics
			prevTime = time;
		}

		const glm::vec3& location = entityTransform.Location;
		stats.HorizontalDistance = glm::length(glm::vec2(location.x, location.z));
		stats.VerticalDistance = location.y;
		stats.Speed = stats.HorizontalDistance / seconds;

		// The yaw of where it faces, compared to where it faced at the start (forward is -Z)
		const glm::vec3 forward = Math::GetForwardVector(entityTransform.Rotation);
		if (forward.x * forward.x + forward.z * forward.z > 1e-6f)
			stats.TurnDegrees = glm::degrees(std::remainder(std::atan2(forward.x, forward.z) - std::atan2(0.f, -1.f), glm::two_pi<float>()));

		stats.bValid = true;
		return stats;
	}

	void AnimationAssetEditor::DrawRootMotionStats(const SkeletalMeshAnimation& animation)
	{
		if (!animation.HasRootMotion())
			return;

		// The keys change when the root motion is extracted again or the change is undone
		const auto& keys = animation.RootMotion;
		const auto statsKey = std::make_tuple(animation.RootMotionType, keys.Locations.size(), keys.Rotations.size(), animation.Duration,
			keys.Locations.empty() ? glm::vec3(0.f) : keys.Locations.back().Location,
			keys.Rotations.empty() ? glm::quat(1.f, 0.f, 0.f, 0.f) : keys.Rotations.back().Rotation);
		if (!m_bRootMotionStatsCalculated || statsKey != m_RootMotionStatsKey)
		{
			m_RootMotionStats = CalculateRootMotionStats(animation);
			m_RootMotionStatsKey = statsKey;
			m_bRootMotionStatsCalculated = true;
		}

		if (!m_RootMotionStats.bValid)
			return;

		const auto& stats = m_RootMotionStats;
		UI::Text("Root Motion Distance", std::format("{:.2f} horizontal, {:.2f} vertical", stats.HorizontalDistance, stats.VerticalDistance),
			"Where the root motion of the whole animation takes an entity, from where it starts");
		UI::Text("Root Motion Speed", std::format("{:.2f} per second", stats.Speed),
			"The average horizontal speed at the playback speed of 1.\nFor example, use it for the Max Speed of a crowd agent that this animation moves");
		const char* side = stats.TurnDegrees > 0.05f ? " (left)" : (stats.TurnDegrees < -0.05f ? " (right)" : "");
		UI::Text("Root Motion Turn", std::format("{:.1f} degrees{}", stats.TurnDegrees, side),
			"How far the root motion of the whole animation turns an entity around the world up");
	}

	size_t AnimationAssetEditor::AnimationData::GetMemoryUsage() const
	{
		size_t result = sizeof(*this);
		auto addBone = [&result](const BoneAnimation& bone)
		{
			result += sizeof(BoneAnimation) + bone.Locations.capacity() * sizeof(KeyPosition)
				+ bone.Rotations.capacity() * sizeof(KeyRotation) + bone.Scales.capacity() * sizeof(KeyScale);
		};
		for (const auto& [name, bone] : Animation.GetAnimationBones())
		{
			result += name.capacity();
			addBone(bone);
		}
		addBone(Animation.RootMotion);
		result += Animation.PreRootMotionLocations.capacity() * sizeof(glm::vec3);
		result += Animation.Settings.GetMemoryUsage() - sizeof(AnimationSettings); // `sizeof(*this)` includes it
		return result;
	}

	std::string AnimationAssetEditor::DescribeState(const AssetAnimation::Settings& before, const AssetAnimation::Settings& after)
	{
		if (before.bInPlace != after.bInPlace)
			return before.Events.size() == after.Events.size() ? "Edit In Place" : "Edit Animation";
		if (after.Events.size() > before.Events.size())
			return "Add Animation Event";
		if (after.Events.size() < before.Events.size())
			return "Delete Animation Event";
		return "Edit Animation Event";
	}
}
