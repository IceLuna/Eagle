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
		m_Component->SetAnchor(glm::vec2(0.05f));
		m_Component->SetPivot(glm::vec2(0.f));
		m_Component->SetFontSize(100.f);
		m_Component->SetMaxWidth(1080.f);

		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void FontAssetEditor::OnImGuiRender(bool* pOpen)
	{
		glm::vec2 anchor = m_Component->GetAnchor();
		float fontSize = m_Component->GetFontSize();
		float maxWidth = m_Component->GetMaxWidth();
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);

		UI::TextWithSeparator("Data");

		UI::BeginPropertyGrid("FontDetails");
		UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
		UI::Text("Type", "Font");
		AssetEditor::DrawAlwaysIncludeInGameBuilds(m_Asset);
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
			if (UI::PropertyDrag("Anchor", anchor, 0.01f, 0.f, 0.f, "Point of the viewport the text's top left corner is attached to.\n(0; 0) is the top left corner, (1; 1) is the bottom right corner"))
				m_Component->SetAnchor(anchor);
			if (UI::PropertyDrag("Font Size", fontSize, 0.5f, 0.f, 0.f, "In reference pixels"))
				m_Component->SetFontSize(fontSize);
			if (UI::PropertyDrag("Max Width", maxWidth, 1.f, 0.f, 0.f, "In reference pixels. 0 - no wrapping"))
				m_Component->SetMaxWidth(maxWidth);
			UI::Property("Draw Text Bounds", m_bDrawTextBounds, "Outlines the text box and shows the baselines. Lines are wrapped at the box's right edge (Max Width)");

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

		GetCurrentScene()->TextUIDebugSelectedEntity = m_bDrawTextBounds ? m_Component->Parent.GetGUID() : GUID(0, 0);
		DrawViewport(false, m_WindowName);
	}
}
