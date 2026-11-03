#include "egpch.h"
#include "AnimationAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
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
		m_Component->SetRootMotionLockFlag(bInPlace ? RootMotionLockFlag::Position : RootMotionLockFlag::None);

		auto& camera = scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = skeletalMeshAsset->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
		camera.LookAt(center);

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
			}
		}

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
					event.Time = m_Component->CurrentClipPlayTime;
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
		UI::TextWithSeparator("Visualization settings");
		UI::BeginPropertyGrid("AnimationDetails");

		UI::Property("Play animation", bPlayAnimation);
		UI::Property("Looping", m_Component->bClipLooping);
		UI::PropertyDrag("Playback Speed", m_Component->ClipPlaybackSpeed, 0.1f);

		const float duration = m_Asset->GetAnimation()->Duration;
		if (UI::PropertySlider("Playback Position", m_Component->CurrentClipPlayTime, 0.f, duration))
		{
			m_Component->CurrentClipPlayTime = glm::clamp(m_Component->CurrentClipPlayTime, 0.f, duration);
			m_Component->PrevClipPlayTime = m_Component->CurrentClipPlayTime;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
		m_EditTracker.EndFrame();

		DrawViewport(true);
		if (!bPlayAnimation)
			m_Component->CurrentClipPlayTime = m_Component->PrevClipPlayTime; // Prevent animation from advancing
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
