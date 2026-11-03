#include "egpch.h"
#include "SoundGroupAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Audio/SoundGroup.h"

namespace Eagle
{
	SoundGroupAssetEditor::SoundGroupAssetEditor(const Ref<AssetSoundGroup>& asset)
		: m_Asset(asset)
		, m_EditTracker(&DescribeState)
	{
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
		SetTabWindowName(m_WindowName);
	}

	void SoundGroupAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& soundGroup = m_Asset->GetSoundGroup();
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		PrepareTabWindow();
		bool bHidden = !ImGui::Begin(m_WindowName.c_str(), pOpen, EditorFrames::TabWindowFlags);
		m_EditTracker.BeginFrame(&m_UndoHistory, m_Asset);

		UI::BeginPropertyGrid("SoundGroupDetails");

		UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
		UI::Text("Type", "Sound Group");
		AssetEditor::DrawAlwaysIncludeInGameBuilds(m_Asset);

		float volume = soundGroup->GetVolume();
		if (UI::PropertyDrag("Volume", volume, 0.05f))
		{
			soundGroup->SetVolume(glm::max(volume, 0.f));
			bChanged = true;
		}

		float pitch = soundGroup->GetPitch();
		if (UI::PropertyDrag("Pitch", pitch, 0.05f, 0.f, 10.f))
		{
			soundGroup->SetPitch(glm::clamp(pitch, 0.f, 10.f));
			bChanged = true;
		}

		bool bPaused = soundGroup->IsPaused();
		if (UI::Property("Is Paused", bPaused))
		{
			soundGroup->SetPaused(bPaused);
			bChanged = true;
		}

		bool bMuted = soundGroup->IsMuted();
		if (UI::Property("Is Muted", bMuted))
		{
			soundGroup->SetMuted(bMuted);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		if (bChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
		m_EditTracker.EndFrame();
	}

	std::string SoundGroupAssetEditor::DescribeState(const AssetSoundGroup::Settings& before, const AssetSoundGroup::Settings& after)
	{
		return UndoUtils::Describe({
			{ "Volume", before.Volume != after.Volume },
			{ "Pitch", before.Pitch != after.Pitch },
			{ "Is Paused", before.bPaused != after.bPaused },
			{ "Is Muted", before.bMuted != after.bMuted },
		}, "Edit Sound Group");
	}
}
