#include "egpch.h"
#include "AssetImporterPanel.h"
#include "../EditorResources.h"

#include "Eagle/Core/Application.h"
#include "Eagle/UI/UI.h"

#include <stb_image/stb_image.h>

namespace Eagle
{
	static const char* s_RootMotionHelpMsg = "Animation root motion will be used to drive the transformation of an entity\n"
		"Base Pose: use base pose root bone transform\n"
		"AnimFirstFrame: use root bone transform of the first animation frame";
	static const char* s_2DCommonSettingsHelpMsg = "`Is Normal Map` and `Import Alpha channel` aren't present in common settings. "
		"You can control them via per texture settings";
	static const char* s_CompressionHelpMsg = "If set to true, the engine will try to compress the image. "
		"Most of the time, medium compression is good enough. But if you see some banding/blocks, choose a higher quality, especially for normal maps.";

	static ImVec2 s_DefaultWindowSize = ImVec2(720.f, 450.f);

	template <typename Func>
	static void FancyTreeNode(const char* label, bool bDefaultOpen, Func&& func, const char* helpMsg = nullptr)
	{
		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		const bool treeOpened = ImGui::TreeNodeEx(label, flags | (bDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0));
		ImGui::PopStyleVar();
		if (helpMsg)
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMsg);
		}

		if (treeOpened)
		{
			func();
			ImGui::TreePop();
			ImGui::Separator();
		}
	}

	TextureImporterPanel::TextureImporterPanel(const std::vector<Path>& paths)
	{
		// Not pre-allocating memory for cube textures. Assumption is that there won't be much of them
		m_2DTextures.reserve(paths.size());

		for (const auto& path : paths)
		{
			const AssetType assetType = AssetImporter::GetAssetTypeByExtension(path);
			EG_CORE_ASSERT(assetType == AssetType::Texture2D || assetType == AssetType::TextureCube);
			if (assetType == AssetType::Texture2D)
			{
				auto& data = m_2DTextures.emplace_back();
				data.AssetPath = path;

				auto& settings = data.Settings;
				settings.bNormalMap = Utils::IsNormalMap(path);
				if (settings.bNormalMap)
				{
					settings.Compression = TextureCompressor::Quality::High;
				}

				int comp = 1;
				stbi_info(Utils::AsString(path).c_str(), &data.Size.x, &data.Size.y, &comp);
				settings.ImportFormat = ChannelsToAssetTexture2DFormat(comp);
			}
			else if (assetType == AssetType::TextureCube)
			{
				auto& data = m_CubeTextures.emplace_back();
				data.AssetPath = path;

				auto& settings = data.Settings;

				int comp = 1;
				stbi_info(Utils::AsString(path).c_str(), &data.Size.x, &data.Size.y, &comp);
			}
		}

		if (!m_2DTextures.empty() && !m_CubeTextures.empty())
		{
			m_WindowName = "Import 2D and Cube textures";
		}
		else if (!m_2DTextures.empty())
		{
			m_WindowName = "Importing 2D textures";
		}
		else if (!m_CubeTextures.empty())
		{
			m_WindowName = "Importing Cube textures";
		}
		else
		{
			EG_CORE_ASSERT(false);
		}
	}

	bool TextureImporterPanel::OnImGuiRender(const Path& importTo, bool* pOpen)
	{
		if (m_WindowName.empty())
			return false;

		const char* windowName = m_WindowName.c_str();

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
			if (!m_2DTextures.empty())
			{
				FancyTreeNode("Common 2D settings", true, [this]()
				{
					Render2DSettings("", m_Common2DSettings, {-1, -1}, true);
				}, s_2DCommonSettingsHelpMsg);
			}
			if (!m_CubeTextures.empty())
			{
				FancyTreeNode("Common Cube settings", true, [this]()
				{
					RenderCubeSettings("", m_CommonCubeSettings, {-1, -1});
				});
			}

			if (!m_2DTextures.empty())
			{
				FancyTreeNode("2D settings overrides", false, [this]()
				{
					for (auto& texture : m_2DTextures)
					{
						constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
						if (ImGui::TreeNodeEx(&texture, flags, Utils::AsString(texture.AssetPath).c_str()))
						{
							Render2DSettings(texture.AssetPath, texture.Settings, texture.Size, false, &texture.bOverride);
							ImGui::TreePop();
						}
					}
				});
			}

			if (!m_CubeTextures.empty())
			{
				FancyTreeNode("Cube settings overrides", false, [this]()
				{
					for (auto& texture : m_CubeTextures)
					{
						constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
						if (ImGui::TreeNodeEx(&texture, flags, Utils::AsString(texture.AssetPath).c_str()))
						{
							RenderCubeSettings(texture.AssetPath, texture.Settings, texture.Size, &texture.bOverride);
							ImGui::TreePop();
						}
					}
				});
			}

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();
			if (ImGui::Button("Import"))
			{
				bool bAnyFailed = false;
				for (const auto& texture : m_2DTextures)
				{
					AssetImportSettings settings;
					settings.Texture2DSettings = texture.bOverride ? texture.Settings : m_Common2DSettings;
					settings.Texture2DSettings.bNormalMap = texture.Settings.bNormalMap; // It's always per texture

					// Limit max mips count
					if (!texture.bOverride)
					{
						const int maxMips = (int)CalculateMipCount(uint32_t(texture.Size.x), uint32_t(texture.Size.y));
						settings.Texture2DSettings.MipsCount = glm::min(uint32_t(maxMips), m_Common2DSettings.MipsCount);
					}

					bAnyFailed |= !AssetImporter::Import(texture.AssetPath, importTo, AssetType::Texture2D, settings);
				}
				for (const auto& texture : m_CubeTextures)
				{
					AssetImportSettings settings;
					settings.TextureCubeSettings = texture.bOverride ? texture.Settings : m_CommonCubeSettings;

					bAnyFailed |= !AssetImporter::Import(texture.AssetPath, importTo, AssetType::TextureCube, settings);
				}

				if (bAnyFailed)
					Application::Get().GetImGuiLayer()->AddMessage("At least one texture import failed. See logs for more details");

				*pOpen = false;
				bResult = true;
			}
			ImGui::EndPopup();
		}

		return bResult;
	}

	void TextureImporterPanel::Render2DSettings(const Path& path, AssetImportTexture2DSettings& settings, glm::ivec2 size, bool bDrawingCommon, bool* bOverride)
	{
		UI::BeginPropertyGrid("TextureImporter");

		if (!path.empty())
			UI::Text("Path", Utils::AsString(path));
		if (size.x > 0 && size.y > 0)
			UI::Text("Size", std::to_string(size.x) + 'x' + std::to_string(size.y));
		ImGui::Separator();

		if (bOverride)
		{
			UI::Property("Override", *bOverride, "Enable if you need to override import settings for this texture");

			if ((*bOverride) == false)
			{
				UI::PushItemDisabled();
			}
		}

		constexpr int minMips = 1;
		const int maxMips = (int)CalculateMipCount(uint32_t(size.x), uint32_t(size.y));
		int mips = int(settings.MipsCount);

		UI::ComboEnum("Format", settings.ImportFormat);
		UI::ComboEnum("Filter mode", settings.FilterMode);
		UI::ComboEnum("Address mode", settings.AddressMode);
		UI::PropertySlider("Anisotropy", settings.Anisotropy, 1.f, 16.f, "The final max value will be limited by the hardware capabilities");
		if (!bDrawingCommon)
		{
			if (UI::PropertySlider("Mips", mips, minMips, maxMips))
				settings.MipsCount = uint32_t(glm::clamp(mips, minMips, maxMips));
		}
		else // We're rendering common settings, so display an input field
		{
			if (UI::PropertyDrag("Max Mips", settings.MipsCount))
				settings.MipsCount = glm::max(1u, settings.MipsCount);
		}
		UI::ComboEnum("Compression", settings.Compression, s_CompressionHelpMsg);
		if (!bDrawingCommon)
		{
			UI::Property("Is Normal Map", settings.bNormalMap, "Set to true, if the importing image is a normal map. Currently, it only affects the result if the compression is enabled");
		}

		if (bOverride && ((*bOverride) == false))
		{
			UI::PopItemDisabled();
		}

		UI::EndPropertyGrid();
	}

	void TextureImporterPanel::RenderCubeSettings(const Path& path, AssetImportTextureCubeSettings& settings, glm::ivec2 size, bool* bOverride)
	{
		UI::BeginPropertyGrid("TextureImporter");

		if (!path.empty())
			UI::Text("Path", Utils::AsString(path));
		if (size.x > 0 && size.y > 0)
			UI::Text("Size", std::to_string(size.x) + 'x' + std::to_string(size.y));
		ImGui::Separator();

		if (bOverride)
		{
			UI::Property("Override", *bOverride, "Enable if you need to override import settings for this texture");

			if ((*bOverride) == false)
			{
				UI::PushItemDisabled();
			}
		}

		UI::ComboEnum("Format", settings.ImportFormat);
		if (UI::PropertyDrag("Layer Size", settings.LayerSize, 16.f, 32, 0, "Resolution of a cube side"))
			settings.LayerSize = glm::clamp(settings.LayerSize, 16u, 4096u);
		if (UI::PropertyDrag("Prefilter Size", settings.PrefilterSize, 16.f, 32, 0, "The quality of IBL reflection"))
			settings.PrefilterSize = glm::clamp(settings.PrefilterSize, 16u, 4096u);

		if (bOverride && ((*bOverride) == false))
		{
			UI::PopItemDisabled();
		}

		UI::EndPropertyGrid();
	}

	MeshImporterPanel::MeshImporterPanel(const std::vector<Path>& paths)
	{
		for (const auto& path : paths)
		{
			const AssetType assetType = AssetImporter::GetAssetTypeByExtension(path);
			EG_CORE_ASSERT(assetType == AssetType::StaticMesh || assetType == AssetType::StaticMesh);

			auto& data = m_Meshes.emplace_back();
			data.AssetPath = path;
		}
	}

	bool MeshImporterPanel::OnImGuiRender(const Path& importTo, bool* pOpen)
	{
		if (m_Meshes.empty())
			return false;

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
			FancyTreeNode("Import settings", true, [this]()
			{
				RenderSettings("", m_CommonSettings, m_AsSkeletal);
			});

			FancyTreeNode("Per-mesh import settings overrides", false, [this]()
			{
				for (auto& mesh : m_Meshes)
				{
					constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
						ImGuiTreeNodeFlags_SpanAvailWidth;

					if (ImGui::TreeNodeEx(&mesh, flags, Utils::AsString(mesh.AssetPath).c_str()))
					{
						RenderSettings(mesh.AssetPath, mesh.Settings, mesh.bSkeletal, &mesh.bOverride);
						ImGui::TreePop();
					}
				}
			});

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();

			// Always can create if not a skeletal. Can't create if importing only animations and a skeletal is not selected
			const bool bCanCreate = !m_AsSkeletal || !m_CommonSettings.bOnlyImportAnimations || m_CommonSettings.AnimationSettings.Skeletal;
			if (!bCanCreate)
				UI::PushItemDisabled();

			if (ImGui::Button("Import"))
			{
				bool bAnyFailed = false;
				for (const auto& mesh : m_Meshes)
				{
					const AssetImportSettings settings = mesh.bOverride ? mesh.Settings : m_CommonSettings;
					const bool bSkeletal = mesh.bOverride ? mesh.bSkeletal : m_AsSkeletal;

					const bool bCanCreate = !bSkeletal || !settings.bOnlyImportAnimations || settings.AnimationSettings.Skeletal;
					if (!bCanCreate)
					{
						EG_CORE_ERROR("Failed to import {}: `Import Animations Only` is set but skeletal mesh is not selected for them to use!");
						bAnyFailed = true;
						continue;
					}

					const AssetType type = bSkeletal ?
						settings.bOnlyImportAnimations ? AssetType::Animation : AssetType::SkeletalMesh
						: AssetType::StaticMesh;

					bAnyFailed |= !AssetImporter::Import(mesh.AssetPath, importTo, type, settings);
				}

				if (bAnyFailed)
					Application::Get().GetImGuiLayer()->AddMessage("At least one mesh failed to import. See logs for more details");

				*pOpen = false;
				bResult = true;
			}

			if (!bCanCreate)
				UI::PopItemDisabled();

			ImGui::EndPopup();
		}

		return bResult;
	}

	void MeshImporterPanel::RenderSettings(const Path& path, AssetImportSettings& settings, bool& bSkeletal, bool* bOverride)
	{
		auto& meshSettings = settings.MeshSettings;

		UI::BeginPropertyGrid("MeshImporter");

		if (!path.empty())
			UI::Text("Path", Utils::AsString(path));

		ImGui::Separator();

		if (bOverride)
		{
			UI::Property("Override", *bOverride, "Enable if you need to override import settings for this mesh");

			if ((*bOverride) == false)
			{
				UI::PushItemDisabled();
			}
		}

		UI::Property("As Skeletal", bSkeletal, "Try to import as a skeletal mesh. If a mesh doesn't have bones, it'll be imported as a static mesh");
		ImGui::Separator();

		if (settings.bOnlyImportAnimations && bSkeletal)
			UI::PushItemDisabled();

		if (!bSkeletal)
			UI::PushItemDisabled();
		UI::Property("Import animations", meshSettings.bImportAnimations, "Animation assets will be imported if the mesh contains any");
		if (!bSkeletal)
			UI::PopItemDisabled();

		UI::Property("Import materials", meshSettings.bImportMaterials, "Material assets will be imported if the mesh contains any");

		if (settings.bOnlyImportAnimations && bSkeletal)
			UI::PopItemDisabled();

		ImGui::Separator();

		if (!bSkeletal)
			UI::PushItemDisabled();

		if (!meshSettings.bImportAnimations)
			UI::PushItemDisabled();

		UI::ComboEnum("Root Motion Mode", settings.AnimationSettings.RootMotionType, s_RootMotionHelpMsg);

		if (!meshSettings.bImportAnimations)
			UI::PopItemDisabled();

		UI::Property("Import animations only", settings.bOnlyImportAnimations, "Set this flag if you want only animations to be imported for the reference skeletal mesh asset)");

		{
			if (!bSkeletal || !settings.bOnlyImportAnimations)
				UI::PushItemDisabled();

			EditorResources::DrawAssetSelection("Skeletal", settings.AnimationSettings.Skeletal, "Select skeletal asset to be used for the animation");

			if (!bSkeletal || !settings.bOnlyImportAnimations)
				UI::PopItemDisabled();
		}

		if (!bSkeletal)
			UI::PopItemDisabled();

		if (bOverride && ((*bOverride) == false))
		{
			UI::PopItemDisabled();
		}

		UI::EndPropertyGrid();
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

			EditorResources::DrawAssetSelection("Skeletal", m_Mesh, "Select skeletal asset to be used for the animation graph");

			UI::EndPropertyGrid();

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();

			const bool bCanCreate = m_Mesh.operator bool();
			if (!bCanCreate)
				UI::PushItemDisabled();

			if (ImGui::Button("Create"))
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
	
	bool AnimationBlendSpaceImporterPanel::OnImGuiRender(const Path& importTo, bool* pOpen)
	{
		const char* windowName = "Create an animation blend space";

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
			UI::BeginPropertyGrid("AnimationBlendSpaceImporter");

			EditorResources::DrawAssetSelection("Skeletal", m_Mesh, "Select skeletal asset to be used for the blend space");

			UI::EndPropertyGrid();

			ImGui::Separator();
			if (ImGui::Button("Cancel"))
				*pOpen = false;
			ImGui::SameLine();

			const bool bCanCreate = m_Mesh.operator bool();
			if (!bCanCreate)
				UI::PushItemDisabled();

			if (ImGui::Button("Create"))
			{
				AssetImporter::CreateAnimationBlendSpace(m_Path, m_Mesh);

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
