#include "egpch.h"
#include "PhysicsMaterialAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Physics/PhysicsMaterial.h"

namespace Eagle
{
	void PhysicsMaterialAssetEditor::OnImGuiRender(bool* pOpen)
	{
		static const char* s_StaticFrictionHelpMsg = "Static friction defines the amount of friction that is applied between surfaces that are not moving lateral to each-other";
		static const char* s_DynamicFrictionHelpMsg = "Dynamic friction defines the amount of friction applied between surfaces that are moving relative to each-other";

		const auto& material = m_Asset->GetMaterial();

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("PhysicsMaterialDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());

		bool bPhysicsMaterialChanged = false;
		bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
		bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
		bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);

		UI::EndPropertyGrid();

		if (bPhysicsMaterialChanged)
			m_Asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
	}
}
