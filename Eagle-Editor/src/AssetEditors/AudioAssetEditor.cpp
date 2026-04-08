#include "egpch.h"
#include "AudioAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
	AudioAssetEditor::AudioAssetEditor(const Ref<AssetAudio>& asset)
		: m_Asset(asset)
	{
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void Eagle::AudioAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& audio = m_Asset->GetAudio();
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_WindowName.c_str(), pOpen);
		UI::BeginPropertyGrid("AudioDetails");

		UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
		UI::Text("Type", "Audio");
		UI::Text("Channels", std::to_string(m_Asset->GetAudio()->GetChannelsCount()));

		float volume = audio->GetVolume();
		if (UI::PropertyDrag("Volume", volume, 0.05f))
		{
			audio->SetVolume(glm::max(volume, 0.f));
			bChanged = true;
		}

		float pitch = audio->GetPitch();
		if (UI::PropertyDrag("Pitch", pitch, 0.05f))
		{
			audio->SetPitch(pitch);
			bChanged = true;
		}

		float pan = audio->GetPan();
		if (UI::PropertySlider("Pan", pan, -1.f, 1.f))
		{
			audio->SetPan(pan);
			bChanged = true;
		}

		auto soundGroupAsset = m_Asset->GetSoundGroupAsset();
		if (EditorResources::DrawAssetSelection("Sound Group", soundGroupAsset))
		{
			m_Asset->SetSoundGroupAsset(soundGroupAsset);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		ImGui::Separator();

		const bool bPlaying = audio->IsPlaying();
		const char* buttonName = bPlaying ? "Stop" : "Play";
		if (ImGui::Button(buttonName))
		{
			if (bPlaying)
				audio->Stop();
			else
				audio->Play();
		}

		ImGui::SameLine();

		if (bChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		if (pOpen && (*pOpen == false))
		{
			audio->Stop(); // Stop on exit
		}
	}
}
