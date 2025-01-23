#include "egpch.h"
#include "StaticMeshAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	StaticMeshAssetEditor::StaticMeshAssetEditor(const Ref<AssetStaticMesh>& asset)
		: AssetEditor(true), m_Asset(asset)
	{
		Entity entity = m_Scene->CreateEntity("StaticMeshAssetEditor");
		m_Component = &entity.AddComponent<StaticMeshComponent>();
		m_Component->SetMeshAsset(m_Asset);

		auto& camera = m_Scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = m_Asset->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
		camera.LookAt(center);
	}

	void StaticMeshAssetEditor::OnImGuiRender(bool* pOpen)
	{
		auto& mesh = m_Asset->GetMesh();
		const size_t verticesCount = mesh->GetVerticesCount();
		size_t indicesCount = 0;
		for (uint32_t i = 0; i < mesh->GetMaterialSlotsCount(); ++i)
			indicesCount += mesh->GetIndicesCount(i);
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("StaticMeshDetails");

		UI::TextWithSeparator("Data");
		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Static Mesh");
		UI::Text("Vertices", std::to_string(verticesCount));
		UI::Text("Indices", std::to_string(indicesCount));
		UI::Text("Vertices Mem Usage (Kb)", std::to_string(verticesCount * sizeof(Vertex) / 1024));
		UI::Text("Indices Mem Usage (Kb)", std::to_string(indicesCount * sizeof(Index) / 1024));

		UI::TextWithSeparator("Materials");

		const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
		for (uint32_t i = 0; i < materialsCount; ++i)
		{
			auto materialAsset = mesh->GetMaterialAsset(i);
			if (EditorResources::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
			{
				mesh->SetMaterialAsset(i, materialAsset);
				m_Component->SetMaterialAsset(i, materialAsset);
				bChanged = true;
			}
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

		DrawViewport();
	}
}
