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
	Ref<Texture2D> EditorResources::s_AnimationBlendSpaceIcon;
	Ref<Texture2D> EditorResources::s_ParticleSystemIcon;
	Ref<Texture2D> EditorResources::s_MaterialIcon;
	Ref<Texture2D> EditorResources::s_BehaviorGraphIcon;
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
		s_AnimationBlendSpaceIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/animationblendspaceicon.png");
		s_ParticleSystemIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/particlesystemicon.png");
		s_MaterialIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/material.png");
		s_BehaviorGraphIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/behaviorgraphicon.png");
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
		s_AnimationBlendSpaceIcon.reset();
		s_ParticleSystemIcon.reset();
		s_MaterialIcon.reset();
		s_BehaviorGraphIcon.reset();
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
			return s_AnimationBlendSpaceIcon;
		case AssetType::BehaviorGraph:
			return s_BehaviorGraphIcon;
		default:
			return s_UnknownIcon;
		}
	}
	
	const Ref<Image> EditorResources::GetAssetPreview(const Ref<Asset>& asset)
	{
		if (Ref<Image> preview = UI::GetAssetPreview(asset))
			return preview;

		return EditorResources::GetAssetIconTexture(asset->GetAssetType())->GetImage();
	}
	
	void EditorResources::OpenAssetEditor(const Ref<Asset>& asset)
	{
		ContentBrowserPanel::Get().OpenAssetEditor(asset);
	}

	bool EditorResources::DrawGraphVariables(const Ref<AnimationGraph>& graph)
	{
		bool bChanged = false;

		for (auto& [name, var] : graph->GetVariables())
		{
			if (!var->bShowInUI)
				continue;

			switch (var->GetType())
			{
			case GraphVariableType::Bool:
			{
				auto boolVar = Cast<GraphVariableBool>(var);
				bChanged |= UI::Property(name, boolVar->Value);
				break;
			}
			case GraphVariableType::Int:
			{
				auto intVar = Cast<GraphVariableInt>(var);
				bChanged |= UI::PropertyDrag(name, intVar->Value);
				break;
			}
			case GraphVariableType::Float:
			{
				auto floatVar = Cast<GraphVariableFloat>(var);
				bChanged |= UI::PropertyDrag(name, floatVar->Value, 0.1f);
				break;
			}
			case GraphVariableType::Animation:
			{
				auto animVar = Cast<GraphVariableAnimation>(var);
				bChanged |= EditorResources::DrawAssetSelection(name, animVar->Value);
				break;
			}
			case GraphVariableType::String:
			{
				auto animVar = Cast<GraphVariableString>(var);
				bChanged |= UI::PropertyText(name, animVar->Value);
				break;
			}
			case GraphVariableType::Vec4:
			{
				auto animVar = Cast<GraphVariableVec4>(var);
				bChanged |= UI::PropertyDrag(name, animVar->Value, 0.05f);
				break;
			}
			default:
				EG_CORE_ASSERT(false);
			}
		}

		return bChanged;
	}
}
