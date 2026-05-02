#include "egpch.h"

#include "AssetThumbnailRenderer.h"

#include "Eagle/Core/Scene.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	AssetThumbnailRenderer::AssetThumbnailRenderer(glm::uvec2 size)
	{
		SceneRendererSettings settings = SceneRendererSettings::GetBasicSettings();
		m_Renderer = MakeRef<SceneRenderer>(size, settings);
		m_Scene = MakeRef<Scene>("AssetThumbnail", m_Renderer);
		m_Scene->bDrawMiscellaneous = false;
		m_Scene->SetUseSkyAsBackground(false);
		m_Scene->SetRenderSkybox(false);
	}

	void AssetThumbnailRenderer::Prepare(bool bNeedSkyboxLighting)
	{
		m_Scene->ClearScene();
		m_Scene->SetSkyboxEnabled(bNeedSkyboxLighting);
		if (bNeedSkyboxLighting)
		{
			const auto& skybox = AssetManager::GetPreviewSkybox();
			m_Scene->SetSkybox(skybox);
		}
	}

	void AssetThumbnailRenderer::Render()
	{
		m_Image = Image::Create(m_Renderer->GetOutputImageSpecs(), "Thumbnail_Output");
		m_Renderer->SetOutputImage(m_Image);
		m_Scene->OnUpdate(Application::Get().GetTimestep(), true, true);
		m_TempAsset.reset();
	}

	void AssetThumbnailRenderer::SetupScene(const Ref<AssetStaticMesh>& asset)
	{
		Entity entity = m_Scene->CreateEntity("StaticMeshAssetThumbnail");
		auto& component = entity.AddComponent<StaticMeshComponent>();
		component.SetMeshAsset(asset);

		auto& camera = m_Scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = asset->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
		camera.LookAt(center);
	}

	void AssetThumbnailRenderer::SetupScene(const Ref<AssetSkeletalMesh>& asset)
	{
		Entity entity = m_Scene->CreateEntity("SkeletalMeshAssetThumbnail");
		auto& component = entity.AddComponent<SkeletalMeshComponent>();
		component.SetMeshAsset(asset);

		auto& camera = m_Scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = asset->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
		camera.LookAt(center);
	}

	void AssetThumbnailRenderer::SetupScene(const Ref<AssetMaterial>& asset)
	{
		const auto& sphere = AssetManager::GetPreviewSphere();

		Entity entity = m_Scene->CreateEntity("MaterialAssetThumbnail");
		auto& component = entity.AddComponent<StaticMeshComponent>();
		component.SetMeshAsset(sphere);
		component.SetMaterialAsset(0, asset);

		Transform tr{};
		tr.Rotation = glm::rotate(tr.Rotation.GetQuat(), glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
		component.SetWorldTransform(tr);

		auto& camera = m_Scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = sphere->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
		camera.LookAt(center);
	}

	void AssetThumbnailRenderer::SetupScene(const Ref<AssetEntity>& asset)
	{
		Entity entity = m_Scene->CreateFromEntityAsset(asset);

		auto& camera = m_Scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		AABB aabb;
		if (entity.HasComponent<StaticMeshComponent>())
		{
			const auto& comp = entity.GetComponent<StaticMeshComponent>();
			if (const auto& asset = comp.GetMeshAsset())
				aabb.Grow(AABB::Transformed(asset->GetMesh()->GetAABB(), comp.GetWorldTransform()));
		}
		if (entity.HasComponent<SkeletalMeshComponent>())
		{
			const auto& comp = entity.GetComponent<SkeletalMeshComponent>();
			if (const auto& asset = comp.GetMeshAsset())
				aabb.Grow(AABB::Transformed(asset->GetMesh()->GetAABB(), comp.GetWorldTransform()));
		}
		if (entity.HasComponent<SpriteComponent>())
		{
			const auto& comp = entity.GetComponent<SpriteComponent>();
			const auto& transform = comp.GetWorldTransform();
			const glm::vec3 halfScale = transform.Scale3D * 0.5f;
			aabb.Grow(AABB{transform.Location - halfScale, transform.Location + halfScale});
		}
		
		if (aabb.IsValid())
		{
			const glm::vec3 center = aabb.Center();
			camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
			camera.LookAt(center);
		}
	}

	void AssetThumbnailRenderer::SetupScene(const Ref<AssetAnimation>& asset)
	{
		const auto& skeletal = asset->GetSkeletal();

		Entity entity = m_Scene->CreateEntity("AnimationAssetThumbnail");
		auto& component = entity.AddComponent<SkeletalMeshComponent>();
		component.SetMeshAsset(skeletal);
		component.SetAnimationAsset(asset);

		auto& camera = m_Scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = skeletal->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
		camera.LookAt(center);
	}

	void AssetThumbnailRenderer::SetupScene(const Ref<AssetParticleSystem>& asset)
	{
		Ref<AssetParticleSystem> assetCopy = AssetParticleSystem::Copy(asset);
		m_TempAsset = assetCopy;
		
		// Mark emitters to be destoyed immediately
		{
			auto emitters = assetCopy->GetEmitters();
			for (auto& emitter : emitters)
				emitter.bDestroyImmediately = true;
			assetCopy->SetEmitters(std::move(emitters));
		}

		Entity entity = m_Scene->CreateEntity("ParticleSystemAssetThumbnail");
		auto& component = entity.AddComponent<ParticleSystemComponent>();
		component.SetAsset(assetCopy);

		auto& camera = m_Scene->EditorCamera;
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		AABB aabb;
		for (const auto& emitter : assetCopy->GetEmitters())
		{
			// AABB is relative to emitters center. So we need to convert to world space to correctly account for emitters transformation
			aabb.Grow(AABB::Transformed(emitter.VisibilityAABB, emitter.RelativeTransform));
		}

		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 1.5f); // Move back
		camera.LookAt(center);
	}
}
