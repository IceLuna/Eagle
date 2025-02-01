#pragma once

#include "Eagle/Asset/Asset.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "Eagle/UI/UI.h"

namespace Eagle
{
	class EditorResources
	{
	public:
		static void Init();
		static void Release();

		static const Ref<Texture2D>& GetAssetIconTexture(AssetType assetType);
		static const Ref<Image> GetAssetPreview(const Ref<Asset>& asset);

		template <typename Type>
		static bool DrawAssetSelection(std::string_view label, Ref<Type>& asset, std::string_view helpMessage = "")
		{
			bool bResult = false;
			bool bOpenPreview = false;
			if constexpr (std::is_same<Type, AssetTexture2D>::value || std::is_same<Type, AssetTextureCube>::value)
			{
				bResult = UI::DrawAssetSelection(label, asset, helpMessage, -1.f, nullptr, &bOpenPreview);
			}
			else
			{
				bResult = UI::DrawAssetSelection(label, asset, helpMessage, -1.f, GetAssetPreview(asset), &bOpenPreview);
			}

			if (bOpenPreview && asset)
			{
				OpenAssetEditor(asset);
			}

			return bResult;
		}

	private:
		static void OpenAssetEditor(const Ref<Asset>& asset);

	private:
		// Asset icons
		static Ref<Texture2D> s_TextureIcon;
		static Ref<Texture2D> s_MeshIcon;
		static Ref<Texture2D> s_AudioIcon;
		static Ref<Texture2D> s_SoundGroupIcon;
		static Ref<Texture2D> s_FontIcon;
		static Ref<Texture2D> s_PhysicsMaterialIcon;
		static Ref<Texture2D> s_EntityIcon;
		static Ref<Texture2D> s_SceneIcon;
		static Ref<Texture2D> s_AnimationIcon;
		static Ref<Texture2D> s_AnimationGraphIcon;
		static Ref<Texture2D> s_ParticleSystemIcon;
		static Ref<Texture2D> s_MaterialIcon;
		static Ref<Texture2D> s_UnknownIcon;
	};
}
