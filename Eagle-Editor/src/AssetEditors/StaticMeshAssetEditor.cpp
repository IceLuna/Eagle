#include "egpch.h"
#include "StaticMeshAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	static const char* s_AABBHelpMsg = "If AABB is not visible by the camera, the mesh is not rendered.\n"
		"It makes sense to increase it manually for skeletal meshes if an animation moves the mesh beyond the bounding box. So, to prevent culling it in such cases, increase AABB.\n"
		"But for optimization reasons, keep AABB as small as possible";

	StaticMeshAssetEditor::StaticMeshAssetEditor(const Ref<AssetStaticMesh>& asset)
		: AssetEditor(true), m_Asset(asset)
	{
		const auto& scene = GetCurrentScene();

		Entity entity = scene->CreateEntity("StaticMeshAssetEditor");
		m_Component = &entity.AddComponent<StaticMeshComponent>();
		m_Component->SetMeshAsset(m_Asset);

		auto& camera = scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = m_Asset->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
		camera.LookAt(center);

		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void StaticMeshAssetEditor::OnImGuiRender(bool* pOpen)
	{
		auto& mesh = m_Asset->GetMesh();
		const size_t verticesCount = mesh->GetVerticesCount();
		size_t indicesCount = 0;
		for (uint32_t i = 0; i < mesh->GetMaterialSlotsCount(); ++i)
			indicesCount += mesh->GetIndicesCount(i);
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);

		UI::BeginPropertyGrid("StaticMeshDetails");
		UI::TextWithSeparator("Data");
		UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
		UI::Text("Type", "Static Mesh");
		UI::Text("Vertices", std::to_string(verticesCount));
		UI::Text("Indices", std::to_string(indicesCount));
		UI::Text("Vertices Mem Usage (Kb)", std::to_string(verticesCount * sizeof(Vertex) / 1024));
		UI::Text("Indices Mem Usage (Kb)", std::to_string(indicesCount * sizeof(Index) / 1024));
		UI::EndPropertyGrid();

		if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
		{
			UI::BeginPropertyGrid("StaticMeshDetails");
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

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("AABB", ImGuiTreeNodeFlags_Framed))
		{
			UI::BeginPropertyGrid("StaticMeshDetails");
			bool bAABBChanged = false;
			AABB aabb = mesh->GetAABB();
			bAABBChanged |= UI::PropertyDrag("Min", aabb.Min, 0.1f, 0, 0, s_AABBHelpMsg);
			bAABBChanged |= UI::PropertyDrag("Max", aabb.Max, 0.1f, 0, 0, s_AABBHelpMsg);
			UI::Property("Visualize", bDrawAABB);
			if (bAABBChanged)
			{
				mesh->SetAABB(aabb);
				bChanged = true;
				if (auto& scene = Scene::GetCurrentScene())
					scene->SetStaticMeshesDirty(true);
			}
			UI::EndPropertyGrid();
			ImGui::TreePop();
		}

		if (bDrawAABB)
		{
			const AABB& aabb = mesh->GetAABB();
			GetCurrentScene()->DrawAABB(aabb, Transform{});
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
