#include "egpch.h"
#include "SoundGroupAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Audio/SoundGroup.h"

namespace Eagle
{
	SoundGroupAssetEditor::SoundGroupAssetEditor(const Ref<AssetSoundGroup>& asset)
		: m_Asset(asset)
	{
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void SoundGroupAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& soundGroup = m_Asset->GetSoundGroup();
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_WindowName.c_str(), pOpen);
		UI::BeginPropertyGrid("SoundGroupDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Sound Group");

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
	}
}
