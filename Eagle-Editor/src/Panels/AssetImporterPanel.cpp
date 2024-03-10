#include "egpch.h"
#include "AssetImporterPanel.h"
#include "Eagle/Asset/AssetManager.h"

#include "Eagle/Core/Application.h"
#include "Eagle/UI/UI.h"

#include <stb_image/stb_image.h>

namespace Eagle
{
	static ImVec2 s_DefaultWindowSize = ImVec2(720.f, 256.f);

	TextureImporterPanel::TextureImporterPanel(const Path& path)
		: m_Path(path)
	{
		const AssetType assetType = AssetImporter::GetAssetTypeByExtension(path);
		EG_CORE_ASSERT(assetType == AssetType::Texture2D || assetType == AssetType::TextureCube)
		bCube = assetType == AssetType::TextureCube;
	}

	bool TextureImporterPanel::OnImGuiRender(const Path& importTo, bool* pOpen)
	{
		const char* windowName = bCube ? "Import cube texture" : "Import 2D texture";

		const bool bAnyPopupPresent = ImGui::IsPopupOpen(windowName, ImGuiPopupFlags_AnyPopup);
		const bool bThisOpened = ImGui::IsPopupOpen(windowName);
		if (bAnyPopupPresent && !bThisOpened)
			return false;

		bool bResult = false;

		if (*pOpen)
		{
			ImGui::OpenPopup(windowName);

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(s_DefaultWindowSize, ImGuiCond_FirstUseEver);
		}

		if (ImGui::BeginPopupModal(windowName, pOpen))
		{
			UI::BeginPropertyGrid("TextureImporter");

			int width = 1, height = 1, comp = 1;
			stbi_info(m_Path.u8string().c_str(), &width, &height, &comp);

			UI::Text("Path", m_Path.u8string());
			UI::Text("Size", std::to_string(width) + 'x' + std::to_string(height));

			ImGui::Separator();

			if (bCube)
			{
				UI::Text("Format", Utils::GetEnumName(m_CubeSettings.ImportFormat), "Currently, other formats are not supported");
				if (UI::PropertyDrag("Layer Size", m_CubeSettings.LayerSize, 16.f, 32, 0, "Resolution of a cube side"))
					m_CubeSettings.LayerSize = glm::clamp(m_CubeSettings.LayerSize, 16u, 4096u);
			}
			else
			{
				constexpr int minMips = 1;
				const int maxMips = (int)CalculateMipCount(uint32_t(width), uint32_t(height));
				int mips = int(m_2DSettings.MipsCount);

				UI::ComboEnum("Format", m_2DSettings.ImportFormat, "Compression only supports RGBA8 format!");
				UI::ComboEnum("Filter mode", m_2DSettings.FilterMode);
				UI::ComboEnum("Address mode", m_2DSettings.AddressMode);
				UI::PropertySlider("Anisotropy", m_2DSettings.Anisotropy, 1.f, 16.f, "The final max value will be limited by the hardware capabilities");
				if (UI::PropertySlider("Mips", mips, minMips, maxMips))
					m_2DSettings.MipsCount = uint32_t(glm::clamp(mips, minMips, maxMips));
				UI::Property("Compress", m_2DSettings.bCompress, "If set to true, the engine will try to compress the image. Compression only supports RGBA8 format!");
				UI::Property("Is Normal Map", m_2DSettings.bNormalMap, "Set to true, if the importing image is a normal map. Currently, it only affects the result if the compression is enabled");
				UI::Property("Import alpha-channel", m_2DSettings.bNeedAlpha, "Set to true, if the alpha channel should be imported. Currently, it only affects the result if the compression is enabled");

				if (m_2DSettings.bCompress && m_2DSettings.ImportFormat != AssetTexture2DFormat::RGBA8)
				{
					Application::Get().GetImGuiLayer()->AddMessage("Compressed textures only support RGBA8 format");
					m_2DSettings.ImportFormat = AssetTexture2DFormat::RGBA8;
				}
			}
			
			UI::EndPropertyGrid();

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();
			if (ImGui::Button("Import"))
			{
				AssetImportSettings settings;
				settings.Texture2DSettings = m_2DSettings;
				settings.TextureCubeSettings = m_CubeSettings;
				if (!AssetImporter::Import(m_Path, importTo, bCube ? AssetType::TextureCube : AssetType::Texture2D , settings))
					Application::Get().GetImGuiLayer()->AddMessage("Import failed. See logs for more details");

				*pOpen = false;
				bResult = true;
			}
			ImGui::EndPopup();
		}

		return bResult;
	}

	bool MeshImporterPanel::OnImGuiRender(const Path& importTo, bool* pOpen)
	{
		const char* windowName = "Import a mesh";

		const bool bAnyPopupPresent = ImGui::IsPopupOpen(windowName, ImGuiPopupFlags_AnyPopup);
		const bool bThisOpened = ImGui::IsPopupOpen(windowName);
		if (bAnyPopupPresent && !bThisOpened)
			return false;

		bool bResult = false;

		if (*pOpen)
		{
			ImGui::OpenPopup(windowName);

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(s_DefaultWindowSize, ImGuiCond_FirstUseEver);
		}

		if (ImGui::BeginPopupModal(windowName, pOpen))
		{
			auto& settings = m_Settings.MeshSettings;

			UI::BeginPropertyGrid("MeshImporter");

			UI::Text("Path", m_Path.u8string());

			ImGui::Separator();

			UI::Property("As Skeletal", bSkeletal, "Try to import as a skeletal mesh. If a mesh doesn't have bones, it'll be imported as a static mesh");
			ImGui::Separator();

			if (m_Settings.bOnlyImportAnimations && bSkeletal)
				UI::PushItemDisabled();

			if (!bSkeletal)
				UI::PushItemDisabled();
			UI::Property("Import animations", settings.bImportAnimations, "Animation assets will be imported if the mesh contains any");
			if (!bSkeletal)
				UI::PopItemDisabled();

			UI::Property("Import materials", settings.bImportMaterials, "Material assets will be imported if the mesh contains any");

			if (m_Settings.bOnlyImportAnimations && bSkeletal)
				UI::PopItemDisabled();

			ImGui::Separator();

			if (!bSkeletal)
				UI::PushItemDisabled();

			UI::Property("Import animations only", m_Settings.bOnlyImportAnimations, "Set this flag if you want only animations to be imported for the reference skeletal mesh asset)");

			{
				if (!bSkeletal || !m_Settings.bOnlyImportAnimations)
					UI::PushItemDisabled();

				UI::DrawAssetSelection("Skeletal", m_Settings.AnimationSettings.Skeletal, "Select skeletal asset to be used for the animation");

				if (!bSkeletal || !m_Settings.bOnlyImportAnimations)
					UI::PopItemDisabled();
			}

			if (!bSkeletal)
				UI::PopItemDisabled();

			UI::EndPropertyGrid();

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();

			// Always can create if not a skeletal. Can't create if importing only animations and a skeletal is not selected
			const bool bCanCreate = !bSkeletal || !m_Settings.bOnlyImportAnimations || m_Settings.AnimationSettings.Skeletal;
			if (!bCanCreate)
				UI::PushItemDisabled();

			if (ImGui::Button("Import"))
			{
				const AssetType type = bSkeletal ? 
										m_Settings.bOnlyImportAnimations ? AssetType::Animation : AssetType::SkeletalMesh
									: AssetType::StaticMesh;

				if (!AssetImporter::Import(m_Path, importTo, type, m_Settings))
					Application::Get().GetImGuiLayer()->AddMessage("Import failed. See logs for more details");

				*pOpen = false;
				bResult = true;
			}

			if (!bCanCreate)
				UI::PopItemDisabled();

			ImGui::EndPopup();
		}

		return bResult;
	}
	
	bool AnimationGraphImporterPanel::OnImGuiRender(const Path& importTo, bool* pOpen)
	{
		const char* windowName = "Create an animation graph";

		const bool bAnyPopupPresent = ImGui::IsPopupOpen(windowName, ImGuiPopupFlags_AnyPopup);
		const bool bThisOpened = ImGui::IsPopupOpen(windowName);
		if (bAnyPopupPresent && !bThisOpened)
			return false;

		bool bResult = false;

		if (*pOpen)
		{
			ImGui::OpenPopup(windowName);

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(s_DefaultWindowSize, ImGuiCond_FirstUseEver);
		}

		if (ImGui::BeginPopupModal(windowName, pOpen))
		{
			UI::BeginPropertyGrid("AnimationGraphImporter");

			UI::DrawAssetSelection("Skeletal", m_Mesh, "Select skeletal asset to be used for the animation graph");

			UI::EndPropertyGrid();

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();

			const bool bCanCreate = m_Mesh.operator bool();
			if (!bCanCreate)
				UI::PushItemDisabled();

			if (ImGui::Button("Import"))
			{
				AssetImporter::CreateAnimationGraph(m_Path, m_Mesh);

				*pOpen = false;
				bResult = true;
			}

			if (!bCanCreate)
				UI::PopItemDisabled();

			ImGui::EndPopup();
		}

		return bResult;
	}
}
