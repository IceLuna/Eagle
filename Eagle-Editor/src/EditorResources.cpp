#include "EditorResources.h"
#include "Panels/ContentBrowserPanel.h"

#include "Eagle/Utils/ThumbnailCache.h"

namespace Eagle
{
	// Asset icons
	Ref<Texture2D> EditorResources::s_TextureIcon;
	Ref<Texture2D> EditorResources::s_MeshIcon;
	Ref<Texture2D> EditorResources::s_AudioIcon;
	Ref<Texture2D> EditorResources::s_SoundGroupIcon;
	Ref<Texture2D> EditorResources::s_FontIcon;
	Ref<Texture2D> EditorResources::s_PhysicsMaterialIcon;
	Ref<Texture2D> EditorResources::s_EntityIcon;
	Ref<Texture2D> EditorResources::s_SceneIcon;
	Ref<Texture2D> EditorResources::s_AnimationIcon;
	Ref<Texture2D> EditorResources::s_AnimationGraphIcon;
	Ref<Texture2D> EditorResources::s_ParticleSystemIcon;
	Ref<Texture2D> EditorResources::s_MaterialIcon;
	Ref<Texture2D> EditorResources::s_UnknownIcon;

	void EditorResources::Init()
	{
		s_TextureIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/textureicon.png");
		s_MeshIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/meshicon.png");
		s_AudioIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/audioicon.png");
		s_SoundGroupIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/soundgroupicon.png");
		s_FontIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/fonticon.png");
		s_PhysicsMaterialIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/physicsmaterialicon.png");
		s_EntityIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/entityicon.png");
		s_SceneIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/sceneicon.png");
		s_AnimationIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/animationicon.png");
		s_AnimationGraphIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/animationgraphicon.png");
		s_ParticleSystemIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/particlesystemicon.png");
		s_MaterialIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/material.png");
		s_UnknownIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/unknownicon.png");
	}

	void EditorResources::Release()
	{
		s_TextureIcon.reset();
		s_MeshIcon.reset();
		s_AudioIcon.reset();
		s_SoundGroupIcon.reset();
		s_FontIcon.reset();
		s_PhysicsMaterialIcon.reset();
		s_EntityIcon.reset();
		s_SceneIcon.reset();
		s_AnimationIcon.reset();
		s_AnimationGraphIcon.reset();
		s_ParticleSystemIcon.reset();
		s_MaterialIcon.reset();
		s_UnknownIcon.reset();
	}

	const Ref<Texture2D>& EditorResources::GetAssetIconTexture(AssetType assetType)
	{
		switch (assetType)
		{
		case AssetType::Texture2D:
		case AssetType::TextureCube:
			return s_TextureIcon;
		case AssetType::StaticMesh:
		case AssetType::SkeletalMesh:
			return s_MeshIcon;
		case AssetType::Audio:
			return s_AudioIcon;
		case AssetType::SoundGroup:
			return s_SoundGroupIcon;
		case AssetType::Font:
			return s_FontIcon;
		case AssetType::Material:
			return s_MaterialIcon;
		case AssetType::PhysicsMaterial:
			return s_PhysicsMaterialIcon;
		case AssetType::Entity:
			return s_EntityIcon;
		case AssetType::Scene:
			return s_SceneIcon;
		case AssetType::Animation:
			return s_AnimationIcon;
		case AssetType::AnimationGraph:
			return s_AnimationGraphIcon;
		case AssetType::ParticleSystem:
			return s_ParticleSystemIcon;
		case AssetType::AnimationBlendSpace:
			return s_UnknownIcon; // TODO:
		default:
			return s_UnknownIcon;
		}
	}
	
	const Ref<Image> EditorResources::GetAssetPreview(const Ref<Asset>& asset)
	{
		if (!asset)
			return Texture2D::NoneIconTexture->GetImage();

		Ref<Image> preview;
		if (ThumbnailCache::IsRenderableAssetType(asset->GetAssetType()))
		{
			preview = ThumbnailCache::Get(asset);
			if (!preview)
			{
				if (ThumbnailCache::Render(asset, ThumbnailCache::GetThumbnailSize()))
				{
					preview = ThumbnailCache::Get(asset);
				}
			}
		}

		if (preview)
			return preview;

		return EditorResources::GetAssetIconTexture(asset->GetAssetType())->GetImage();
	}
	
	void EditorResources::OpenAssetEditor(const Ref<Asset>& asset)
	{
		ContentBrowserPanel::Get().OpenAssetEditor(asset);
	}
}
