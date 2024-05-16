#include "egpch.h"
#include "EntityAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "../EditorLayer.h"

namespace Eagle
{
	void EntityAssetEditor::OnImGuiRender(bool* pOpen)
	{
		constexpr bool bRuntime = false;
		constexpr bool bVolumetricsEnabled = true;
		constexpr bool bDrawTransform = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen))
		{
			const bool bEntityChanged = m_EntityProperties.OnImGuiRender(*m_Asset->GetEntity().get(), bRuntime, bVolumetricsEnabled, bDrawTransform);
			if (bEntityChanged)
				m_Asset->SetDirty(true);

			ImGui::Separator();
			ImGui::Separator();

			{
				if (ImGui::Button("Save asset"))
					Asset::Save(m_Asset);

				const bool bDisableReload = m_EditorLayer.GetEditorState() != EditorState::Edit;
				if (bDisableReload)
					UI::PushItemDisabled();

				ImGui::SameLine();

				if (ImGui::Button("Reload entities"))
				{
					auto& scene = Scene::GetCurrentScene();
					scene->ReloadEntitiesCreatedFromAsset(m_Asset);
				}
				ImGui::SameLine();
				UI::HelpMarker("The scene needs to be saved to store reloaded assets");

				if (bDisableReload)
					UI::PopItemDisabled();

				UI::Tooltip("On the opened scene, all entities created from this assets will be reloaded to match this asset");
			}

		}
		ImGui::End(); // Entity Editor
	}
}
