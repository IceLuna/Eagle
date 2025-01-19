#include "egpch.h"
#include "AnimationAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	AnimationAssetEditor::AnimationAssetEditor(const Ref<AssetAnimation>& asset)
		: AssetEditor(true)
		, m_Asset(asset)
	{
		const auto& skeletalMeshAsset = m_Asset->GetSkeletal();

		Entity entity = m_Scene->CreateEntity("AnimationAssetEditor");
		m_Component = &entity.AddComponent<SkeletalMeshComponent>();
		m_Component->SetMeshAsset(skeletalMeshAsset);
		m_Component->SetAnimationAsset(m_Asset);
		m_Component->AnimType = SkeletalMeshComponent::AnimationType::Clip;
		m_Component->SetRootMotionLockFlag(bInPlace ? RootMotionLockFlag::Position : RootMotionLockFlag::None);

		auto& camera = m_Scene->GetEditorCamera();
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

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("AnimationDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Animation");
		UI::Text("Duration", std::to_string(animation->Duration));
		UI::Text("Ticks per Second", std::to_string(animation->TicksPerSecond));

		bool bExtractRootMotion = animation->HasRootMotion();
		if (UI::Property("Extract Root Motion", bExtractRootMotion))
		{
			const auto& skeletalInfo = m_Asset->GetSkeletal()->GetMesh()->GetSkeletalMeshInfo();
			if (bExtractRootMotion)
				bChanged |= animation->ExtractRootMotion(skeletalInfo);
			else
				bChanged |= animation->RemoveRootMotion(skeletalInfo);
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		// Events tree
		{
			const size_t assetHash = m_Asset->GetGUID().GetHash();
			size_t hashOffset = 0;
			constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

			const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
			const float lineHeight = (ImGui::GetFont()->FontSize * ImGui::GetFont()->Scale) + ImGui::GetStyle().FramePadding.y * 2.f;

			const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash + hashOffset++), treeFlags, "Events");

			// Add Event button
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.45f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.7f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.35f, 0.f, 1.f));

				constexpr char* addEventText = "Add Event";
				const float textWidth = ImGui::CalcTextSize(addEventText, NULL, true).x;
				ImGui::SameLine(contentRegionAvailable.x - textWidth);
				if (ImGui::Button(addEventText, ImVec2{ textWidth + ImGui::GetStyle().FramePadding.y * 2.f, lineHeight }))
				{
					animation->Events.emplace_back();
					bChanged = true;
				}

				ImGui::PopStyleColor(3);
			}

			if (bOpened)
			{
				constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
				for (auto it = animation->Events.begin(); it != animation->Events.end();)
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
						it = animation->Events.erase(it);
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
		if (UI::Property("In place", bInPlace))
		{
			if (bInPlace)
			{
				auto transform = m_Component->Parent.GetWorldTransform();
				transform.Location = glm::vec3(0.f);
				m_Component->Parent.SetWorldTransform(transform);
				m_Component->SetRootMotionLockFlag(RootMotionLockFlag::Position);
			}
			m_Component->SetRootMotionLockFlag(bInPlace ? RootMotionLockFlag::Position : RootMotionLockFlag::None);
		}
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

		DrawViewport(true);
		if (!bPlayAnimation)
			m_Component->CurrentClipPlayTime = m_Component->PrevClipPlayTime; // Prevent animation from advancing
	}
}
