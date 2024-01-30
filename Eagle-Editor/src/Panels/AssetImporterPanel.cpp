#include "egpch.h"
#include "AssetImporterPanel.h"

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
				UI::ComboEnum("Format", m_CubeSettings.ImportFormat);
				if (UI::PropertyDrag("Layer Size", m_CubeSettings.LayerSize, 16.f, 32, 0, "Resolution of a cube side"))
					m_CubeSettings.LayerSize = glm::clamp(m_CubeSettings.LayerSize, 16u, 4096u);
			}
			else
			{
				constexpr int minMips = 1;
				const int maxMips = (int)CalculateMipCount(uint32_t(width), uint32_t(height));
				int mips = int(m_2DSettings.MipsCount);

				UI::ComboEnum("Format", m_2DSettings.ImportFormat, "Can't be changed later");
				UI::ComboEnum("Filter mode", m_2DSettings.FilterMode);
				UI::ComboEnum("Address mode", m_2DSettings.AddressMode);
				UI::PropertySlider("Anisotropy", m_2DSettings.Anisotropy, 1.f, 16.f, "The final max value will be limited by the hardware capabilities");
				if (UI::PropertySlider("Mips", mips, minMips, maxMips))
					m_2DSettings.MipsCount = uint32_t(glm::clamp(mips, minMips, maxMips));
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
				AssetImporter::Import(m_Path, importTo, settings);

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

			UI::Text("Path", m_Path.u8string());

			ImGui::Separator();

			UI::Property("Combine meshes", m_Settings.bImportAsSingleMesh, "If a file contains multiple meshes, they're all be combined into a single one");

			UI::EndPropertyGrid();

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();
			if (ImGui::Button("Import"))
			{
				AssetImportSettings settings;
				settings.MeshSettings = m_Settings;
				AssetImporter::Import(m_Path, importTo, settings);

				*pOpen = false;
				bResult = true;
			}
			ImGui::EndPopup();
		}

		return bResult;
	}
}
