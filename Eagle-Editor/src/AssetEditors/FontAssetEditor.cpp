#include "FontAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Utils/PlatformUtils.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	FontAssetEditor::FontAssetEditor(const Ref<AssetFont>& asset)
		: AssetEditor(true, false), m_Asset(asset)
	{
		const auto& scene = GetCurrentScene();
		scene->bDrawMiscellaneous = false;

		Entity entity = scene->CreateEntity("FontAssetEditor");
		m_Component = &entity.AddComponent<Text2DComponent>();
		m_Component->SetFontAsset(asset);
		m_Component->SetText(m_Text);
		m_Component->SetPosition({ -0.9f, -0.9f });
		m_Component->SetScale(glm::vec2(0.1f));
		m_Component->SetMaxWidth(20.f);

		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void FontAssetEditor::OnImGuiRender(bool* pOpen)
	{
		glm::vec2 position = m_Component->GetPosition();
		glm::vec2 scale = m_Component->GetScale();
		float maxWidth = m_Component->GetMaxWidth();
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);

		UI::TextWithSeparator("Data");

		UI::BeginPropertyGrid("FontDetails");
		UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
		UI::Text("Type", "Font");
		UI::EndPropertyGrid();

		ImGui::Separator();
		if (UI::PushTreeNode("Metadata"))
		{
			UI::BeginPropertyGrid("FontDetails");
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
		if (UI::PushTreeNode("Visualization Settings", true))
		{
			UI::BeginPropertyGrid("FontDetails");

			if (UI::PropertyTextMultiline("Text", m_Text))
				m_Component->SetText(m_Text);
			if (UI::PropertyDrag("Position", position, 0.05f, 0.f, 0.f, "In normalized device coordinates"))
				m_Component->SetPosition(position);
			if (UI::PropertyDrag("Scale", scale, 0.05f))
				m_Component->SetScale(scale);
			if (UI::PropertyDrag("Max Width", maxWidth, 0.05f))
				m_Component->SetMaxWidth(maxWidth);

			UI::EndPropertyGrid();
			UI::PopTreeNode();
		}

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

		DrawViewport(false, m_WindowName);
	}
}
