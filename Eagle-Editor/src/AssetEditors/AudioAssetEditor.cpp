#include "egpch.h"
#include "AudioAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
	void Eagle::AudioAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& audio = m_Asset->GetAudio();
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("AudioDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Audio");

		float volume = audio->GetVolume();
		if (UI::PropertyDrag("Volume", volume, 0.05f))
		{
			audio->SetVolume(glm::max(volume, 0.f));
			bChanged = true;
		}

		auto soundGroupAsset = m_Asset->GetSoundGroupAsset();
		if (UI::DrawAssetSelection("Sound Group", soundGroupAsset))
		{
			m_Asset->SetSoundGroupAsset(soundGroupAsset);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		ImGui::Separator();

		if (ImGui::Button("Play"))
			audio->Play();

		ImGui::SameLine();

		if (bChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
	}
}
