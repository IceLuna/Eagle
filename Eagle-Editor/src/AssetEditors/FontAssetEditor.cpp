#include "FontAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

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

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);

		UI::TextWithSeparator("Data");

		UI::BeginPropertyGrid("FontDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Font");

		UI::TextWithSeparator("Visualization Settings");
		
		if (UI::PropertyTextMultiline("Text", m_Text))
			m_Component->SetText(m_Text);
		if (UI::PropertyDrag("Position", position, 0.05f, 0.f, 0.f, "In normalized device coordinates"))
			m_Component->SetPosition(position);
		if (UI::PropertyDrag("Scale", scale, 0.05f))
			m_Component->SetScale(scale);
		if (UI::PropertyDrag("Max Width", maxWidth, 0.05f))
			m_Component->SetMaxWidth(maxWidth);

		UI::EndPropertyGrid();

		ImGui::End();

		DrawViewport(false, m_WindowName);
	}
}
