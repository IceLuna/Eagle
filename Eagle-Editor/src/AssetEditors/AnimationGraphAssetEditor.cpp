#include "egpch.h"
#include "AnimationGraphAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Input/Input.h"

namespace Eagle
{
	AnimationGraphAssetEditor::AnimationGraphAssetEditor(const Ref<AssetAnimationGraph>& asset)
		: AssetEditor(true, true)
		, m_Asset(asset)
		, m_WindowName(AssetEditor::GetAssetWindowName(m_Asset))
		, m_Graph(MakeScope<AnimationGraphEditor>(m_Asset, m_WindowName))
	{
		m_Graph->SetRenderPreviewPanelCallback([this](float w, float h)
		{
			const auto& currentScene = GetCurrentScene();
			if (!currentScene)
				return;

			auto& renderer = currentScene->GetSceneRenderer();
			auto viewportSize = glm::uvec2(w, h);
			if (renderer->GetViewportSize() != viewportSize)
				currentScene->OnViewportResize(viewportSize.x, viewportSize.y);
			currentScene->OnUpdate(Application::Get().GetTimestep(), true, true);

			const auto& render = renderer->GetOutput();
			UI::Image(render, ImVec2(w, h));

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				ImGui::SetWindowFocus();

			bViewportVisible = ImGui::IsItemVisible();
			// ImGui mouse input is disabled when moving camera, so preserve `bViewportHovered` state if mouse isn't visible
			if (Input::IsMouseVisible())
				bViewportHovered = ImGui::IsItemHovered();
			bViewportFocused = bViewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

			HandleCameraFocus();
			OnViewportEnd();
		});

		m_Graph->SetRenderPreviewVarsCallback([this]()
		{
			UI::BeginPropertyGrid("AnimGraph_Variables");
			EditorResources::DrawGraphVariables(m_Entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph());
			UI::EndPropertyGrid();
		});

		// Setup scene
		{
			const auto& skAsset = m_Asset->GetGraph()->GetSkeletalAsset();
			const auto& scene = GetCurrentScene();
			m_Entity = scene->CreateEntity();
			auto& comp = m_Entity.AddComponent<SkeletalMeshComponent>();
			comp.AnimType = AnimationType::Graph;
			comp.SetMeshAsset(skAsset);
			comp.SetAnimationGraphAsset(m_Asset);

			auto& camera = scene->EditorCamera;
			camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
			camera.LookAt(glm::vec3(0, 0, 0));
			const glm::vec3 cameraDir = camera.GetForwardVector();

			const auto& aabb = skAsset->GetMesh()->GetAABB();
			const glm::vec3 center = aabb.Center();
			camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
			camera.LookAt(center);
		}
	}

	void AnimationGraphAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const bool bRecompiled = m_Graph->OnImGuiRender(pOpen);
		if (bRecompiled)
		{
			// Graphs are reloaded automatically on recompilation.
			// This is done only to reset variables to use new graph default values.
			// Otherwise, we'll get visualization mismatch vars values.

			const bool bMergeVars = false;
			auto& comp = m_Entity.GetComponent<SkeletalMeshComponent>();
			comp.SetAnimationGraphAsset(m_Asset, bMergeVars);
		}
	}

	void AnimationGraphAssetEditor::SetInFocus()
	{
		m_Graph->SetInFocus();
	}

	void AnimationGraphAssetEditor::OnEvent(Event& e)
	{
		AssetEditor::OnEvent(e);
		m_Graph->OnEvent(e);
	}
}
