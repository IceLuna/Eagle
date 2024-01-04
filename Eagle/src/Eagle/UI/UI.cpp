#include "egpch.h"

#include "UI.h"
#include "Font.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Audio/AudioEngine.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>

namespace Eagle::UI
{
	static constexpr int s_IDBufferSize = 32;
	static uint64_t s_ID = 0;
	static char s_IDBuffer[s_IDBufferSize];
	static const VkImageLayout s_VulkanImageLayout = ImageLayoutToVulkan(ImageReadAccess::PixelShaderRead);

	int TextResizeCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			// Resize string callback
			// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
			std::string* str = (std::string*)data->UserData;
			assert(data->Buf == str->c_str());
			str->resize(data->BufTextLen);
			data->Buf = str->data();
		}
		return 0;
	}

	static ButtonType DrawButtons(ButtonType buttons)
	{
		ButtonType pressedButton = ButtonType::None;

		if (HasFlags(buttons, ButtonType::OK))
		{
			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::OK;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
		}
		if (HasFlags(buttons, ButtonType::Yes))
		{
			if (ImGui::Button("Yes", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::Yes;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (HasFlags(buttons, ButtonType::No))
		{
			if (ImGui::Button("No", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::No;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (HasFlags(buttons, ButtonType::Cancel))
		{
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::Cancel;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
	
		return pressedButton;
	}

	void UpdateIDBuffer(const std::string_view label)
	{
		s_IDBuffer[0] = '#';
		s_IDBuffer[1] = '#';
		memset(s_IDBuffer + 2, 0, s_IDBufferSize - 2);

		for (int i = 0; i < (s_IDBufferSize - 2) && i < label.size(); ++i)
		{
			s_IDBuffer[i + 2] = label[i];
		}
	}

	const char* GetIDBuffer()
	{
		return s_IDBuffer;
	}

	bool DrawTexture2DSelection(const std::string_view label, Ref<AssetTexture2D>& modifyingAsset, const std::string_view helpMessage)
	{
		bool bResult = false;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		const bool bAssetValid = modifyingAsset.operator bool();

		const std::string textureName = bAssetValid ? modifyingAsset->GetPath().stem().u8string() : "None";
		
		// TODO: test if we can replace it with PushID
		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it

		UI::Image(bAssetValid ? modifyingAsset->GetTexture() : Texture2D::NoneIconTexture, {32, 32});
		ImGui::SameLine();

		// Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetTexture2D::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = asset != modifyingAsset;
				if (bResult)
					modifyingAsset = Cast<AssetTexture2D>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginCombo(comboID.c_str(), textureName.c_str(), 0))
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;
			// Initially find currently selected texture to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& textureAsset = Cast<AssetTexture2D>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			// Drawing all existing texture assets
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto& textureAsset = Cast<AssetTexture2D>(asset);
				if (!textureAsset)
					continue;

				const auto& currentTexture = textureAsset->GetTexture();

				const bool bSelected = currentItemIdx == i;
				ImGui::PushID((int)textureAsset->GetGUID().GetHash());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, 32.f});
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();
				UI::Image(currentTexture, { 32, 32 });

				ImGui::SameLine();
				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i;

					modifyingAsset = textureAsset;
					bResult = true;
				}
				ImGui::PopID();
				++i;
			}

			ImGui::EndCombo();
		}
		
		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawTextureCubeSelection(const std::string_view label, Ref<AssetTextureCube>& modifyingAsset)
	{
		bool bResult = false;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		const bool bAssetValid = modifyingAsset.operator bool();

		const std::string textureName = bAssetValid ? modifyingAsset->GetPath().stem().u8string() : "None";

		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it

		UI::Image(bAssetValid ? modifyingAsset->GetTexture()->GetTexture2D() : Texture2D::NoneIconTexture, {32, 32});
		ImGui::SameLine();

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_CUBE_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetTextureCube::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = modifyingAsset != asset;
				if (bResult)
					modifyingAsset = Cast<AssetTextureCube>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginCombo(comboID.c_str(), textureName.c_str(), 0))
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;

			//Initially find currently selected texture to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& textureAsset = Cast<AssetTextureCube>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			//Drawing all existing textures
			uint32_t i = noneOffset;
			const auto& allAssets = AssetManager::GetAssets();
			for (auto& [path, asset] : allAssets)
			{
				const auto& textureAsset = Cast<AssetTextureCube>(asset);
				if (!textureAsset)
					continue;

				const auto& currentTexture = textureAsset->GetTexture();

				const bool bSelected = currentItemIdx == i;
				ImGui::PushID((int)asset->GetGUID().GetHash());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, { 0.0f, 32.f });
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();
				UI::Image(currentTexture->GetTexture2D(), {32, 32});

				ImGui::SameLine();
				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i;

					modifyingAsset = textureAsset;
					bResult = true;
				}
				ImGui::PopID();
				++i;
			}

			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawMeshSelection(const std::string_view label, Ref<AssetMesh>& modifyingAsset, const std::string_view helpMessage)
	{
		bool bResult = false;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const std::string smName = modifyingAsset ? modifyingAsset->GetPath().stem().u8string() : "None";
		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it
		bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), smName.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MESH_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetMesh::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = asset != modifyingAsset;
				if (bResult)
					modifyingAsset = Cast<AssetMesh>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;
			// Initially find currently selected static mesh to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& meshAsset = Cast<AssetMesh>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			//Drawing all existing meshes
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto& meshAsset = Cast<AssetMesh>(asset);
				if (!meshAsset)
					continue;

				const bool bSelected = currentItemIdx == i;

				const std::string smName = path.stem().u8string();
				if (ImGui::Selectable(smName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvail().x, 32 }))
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i;

					modifyingAsset = meshAsset;
					bResult = true;
				}
				++i;
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawFontSelection(const std::string_view label, Ref<AssetFont>& modifyingAsset, const std::string_view helpMessage)
	{
		bool bResult = false;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const bool bValid = modifyingAsset.operator bool();
		const std::string name = bValid ? modifyingAsset->GetPath().stem().u8string() : "None";
		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it
		bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), name.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FONT_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetFont::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = asset != modifyingAsset;
				if (bResult)
					modifyingAsset = Cast<AssetFont>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;
			// Initially find currently selected static mesh to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& fontAsset = Cast<AssetFont>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			//Drawing all existing meshes
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto& fontAsset = Cast<AssetFont>(asset);
				if (!fontAsset)
					continue;

				const bool bSelected = currentItemIdx == i;

				const std::string smName = path.stem().u8string();
				if (ImGui::Selectable(smName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvail().x, 32 }))
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i;

					modifyingAsset = fontAsset;
					bResult = true;
				}
				++i;
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawAudioSelection(const std::string_view label, Ref<AssetAudio>& modifyingAsset)
	{
		bool bResult = false;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const std::string soundName = modifyingAsset ? modifyingAsset->GetPath().stem().u8string() : "None";
		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it
		bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), soundName.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SOUND_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath = Path(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetMesh::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = asset != modifyingAsset;
				if (bResult)
					modifyingAsset = Cast<AssetAudio>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;
			// Initially find currently selected sound to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& audioAsset = Cast<AssetAudio>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			//Drawing all existing meshes
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto& audioAsset = Cast<AssetAudio>(asset);
				if (!audioAsset)
					continue;

				const bool bSelected = currentItemIdx == i;

				const std::string smName = path.stem().u8string();
				if (ImGui::Selectable(smName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvail().x, 32 }))
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i;

					modifyingAsset = audioAsset;
					bResult = true;
				}
				++i;
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawMaterialSelection(const std::string_view label, Ref<AssetMaterial>& modifyingAsset, const std::string_view helpMessage)
	{
		bool bResult = false;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		const bool bAssetValid = modifyingAsset.operator bool();

		const std::string materialName = bAssetValid ? modifyingAsset->GetPath().stem().u8string() : "None";

		// TODO: test if we can replace it with PushID
		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it

		const bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), materialName.c_str(), 0);

		// Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetMaterial::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = asset != modifyingAsset;
				if (bResult)
					modifyingAsset = Cast<AssetMaterial>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;

			// Initially find currently selected texture to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& materialAsset = Cast<AssetMaterial>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			// Drawing all existing texture assets
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto& materialAsset = Cast<AssetMaterial>(asset);
				if (!materialAsset)
					continue;

				const bool bSelected = currentItemIdx == i;
				ImGui::PushID((int)materialAsset->GetGUID().GetHash());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, { 0.0f, 32.f });
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();

				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i;

					modifyingAsset = materialAsset;
					bResult = true;
				}
				ImGui::PopID();
				++i;
			}

			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawPhysicsMaterialSelection(const std::string_view label, Ref<AssetPhysicsMaterial>& modifyingAsset, const std::string_view helpMessage)
	{
		bool bResult = false;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		const bool bAssetValid = modifyingAsset.operator bool();

		const std::string materialName = bAssetValid ? modifyingAsset->GetPath().stem().u8string() : "None";

		// TODO: test if we can replace it with PushID
		const std::string comboID = std::string("##") + std::string(label);
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it

		const bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), materialName.c_str(), 0);

		// Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PHYSICS_MATERIAL_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Asset> asset;

				if (AssetManager::Get(filepath, &asset) == false)
				{
					asset = AssetPhysicsMaterial::Create(filepath);
					AssetManager::Register(asset);
				}
				bResult = asset != modifyingAsset;
				if (bResult)
					modifyingAsset = Cast<AssetPhysicsMaterial>(asset);
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;

			// Initially find currently selected texture to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& materialAsset = Cast<AssetPhysicsMaterial>(asset))
						i++;
				}
			}

			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				// TODO: Check if it's needed. Maybe we can put it under `ImGui::Selectable()`
				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			// Drawing all existing texture assets
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto& materialAsset = Cast<AssetPhysicsMaterial>(asset);
				if (!materialAsset)
					continue;

				const bool bSelected = currentItemIdx == i;
				ImGui::PushID((int)materialAsset->GetGUID().GetHash());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, { 0.0f, 32.f });
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();

				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i;

					modifyingAsset = materialAsset;
					bResult = true;
				}
				ImGui::PopID();
				++i;
			}

			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawVec3Control(const std::string_view label, glm::vec3& values, const glm::vec3 resetValues /* = glm::vec3{ 0.f }*/, float columnWidth /*= 100.f*/)
	{
		bool bValueChanged = false;
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.data());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.data());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });

		float lineHeight = (GImGui->Font->FontSize * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
		ImVec2 buttonSize = { lineHeight + 3.f, lineHeight };

		//X
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
		{
			values.x = resetValues.x;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##X", &values.x, 0.1f);
		ImGui::PopItemWidth();
		UI::Tooltip(std::to_string(values.x));
		ImGui::SameLine();

		//Y
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
		{
			values.y = resetValues.y;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##Y", &values.y, 0.1f);
		ImGui::PopItemWidth();
		UI::Tooltip(std::to_string(values.y));
		ImGui::SameLine();

		//Z
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValues.z;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##Z", &values.z, 0.1f);
		ImGui::PopItemWidth();
		UI::Tooltip(std::to_string(values.z));

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return bValueChanged;
	}

	void BeginPropertyGrid(const std::string_view gridName)
	{
		ImGui::PushID(gridName.data());
		ImGui::Columns(2);
	}

	void EndPropertyGrid()
	{
		ImGui::Columns(1);
		ImGui::PopID();
	}

	bool Property(const std::string_view label, bool& value, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::SameLine();
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::Checkbox(s_IDBuffer, &value);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool Property(const std::string_view label, const std::vector<std::string>& customLabels, bool* values, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		const size_t count = customLabels.size();

		// It's done that way because for some reason `text` and `checkbox` are misaligned
		if (count)
		{
			size_t i = 0;
			UpdateIDBuffer(std::string(label) + customLabels[i]);
			ImGui::Text(customLabels[i].c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			bModified |= ImGui::Checkbox(s_IDBuffer, &values[i]);
			if (i != (count - 1))
				ImGui::SameLine();
		}

		for (size_t i = 1; i < count; ++i)
		{	
			UpdateIDBuffer(std::string(label) + customLabels[i]);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			ImGui::Text(customLabels[i].c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			bModified |= ImGui::Checkbox(s_IDBuffer, &values[i]);
			if (i != (count - 1))
				ImGui::SameLine();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool PropertyText(const std::string_view label, std::string& value, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::InputText(s_IDBuffer, value.data(), value.length() + 1, ImGuiInputTextFlags_CallbackResize, TextResizeCallback, &value))
			bModified = true;
		ImGui::SetItemKeyOwner(ImGuiMod_Alt);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool PropertyTextMultiline(const std::string_view label, std::string& value, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::InputTextMultiline(s_IDBuffer, value.data(), value.length() + 1, ImVec2{ 0, 0 }, ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput, TextResizeCallback, &value))
			bModified = true;
		ImGui::SetItemKeyOwner(ImGuiMod_Alt);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool Text(const std::string_view label, const std::string_view text)
	{
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::Text(text.data());
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return false;
	}

	bool TextLink(const std::string_view text, const std::string_view url)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.333f, 0.611f, 0.839f, 1.f });
		ImGui::Text(text.data());
		const bool bClicked = ImGui::IsItemClicked();
		const bool bHovered = ImGui::IsItemHovered();
		ImGui::PopStyleColor();

		// Underline
		{
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			min.y = max.y;
			ImGui::GetWindowDrawList()->AddLine(min, max, bHovered ? 0xFFFF5C25 : 0xFFD69C55, 1.0f);
		}

		if (bClicked)
			Utils::OpenLink(url);

		return bClicked;
	}

	bool BulletLink(const std::string_view text, const std::string_view url)
	{
		ImGui::Bullet();
		ImGui::SameLine();

		return TextLink(text, url);
	}

	bool PropertyDrag(const std::string_view label, int& value, float speed, int min, int max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		bModified = ImGui::DragInt(s_IDBuffer, &value, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, uint32_t& value, float speed, int min, int max, const std::string_view helpMessage)
	{
		int temp = (int)value;
		const bool bChanged = PropertyDrag(label, temp, speed, min, max, helpMessage);
		if (bChanged)
			value = uint32_t(glm::max(temp, 0)); // Clamp negatives to 0 so that we don't overflow
		return bChanged;
	}

	bool PropertyDrag(const std::string_view label, float& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat(s_IDBuffer, &value, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, glm::vec2& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat2(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, glm::vec3& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat3(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, glm::vec4& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat4(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string_view label, int& value, int min, int max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderInt(s_IDBuffer, &value, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string_view label, float& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat(s_IDBuffer, &value, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string_view label, glm::vec2& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat2(s_IDBuffer, &value.x, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string_view label, glm::vec3& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat3(s_IDBuffer, &value.x, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string_view label, glm::vec4& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat4(s_IDBuffer, &value.x, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyColor(const std::string_view label, glm::vec3& value, bool bHDR, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		ImGuiColorEditFlags flags = 0;
		if (bHDR)
			flags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

		bModified = ImGui::ColorEdit3(s_IDBuffer, &value.x, flags);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyColor(const std::string_view label, glm::vec4& value, bool bHDR, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		ImGuiColorEditFlags flags = 0;
		if (bHDR)
			flags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

		bModified = ImGui::ColorEdit4(s_IDBuffer, &value.x, flags);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool InputFloat(const std::string_view label, float& value, float step, float stepFast, const std::string_view helpMessage)
	{
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool result = ImGui::InputFloat(s_IDBuffer, &value, step, stepFast);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return result;
	}

	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, size_t optionsSize, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
	{
		currentSelection = glm::clamp(currentSelection, 0u, uint32_t(optionsSize) - 1u);
		const std::string_view currentString = options[currentSelection];
		size_t tooltipsSize = tooltips.size();
		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::BeginCombo(s_IDBuffer, currentString.data()))
		{
			for (int i = 0; i < optionsSize; ++i)
			{
				bool isSelected = (currentString == options[i]);

				if (ImGui::Selectable(options[i].c_str(), isSelected))
				{
					bModified = true;
					outSelectedIndex = i;
				}

				if (i < tooltipsSize)
					if (!tooltips[i].empty())
						Tooltip(tooltips[i]);

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (currentSelection < tooltipsSize)
			if (!tooltips[currentSelection].empty())
				Tooltip(tooltips[currentSelection]);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
	{
		return Combo(label, currentSelection, options, options.size(), outSelectedIndex, tooltips, helpMessage);
	}

	bool ComboWithNone(const std::string_view label, int currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
	{
		size_t tooltipsSize = tooltips.size();
		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const bool bNoneSelected = currentSelection == -1;
		const std::string& currentString = bNoneSelected ? "None" : options[currentSelection];
		if (ImGui::BeginCombo(s_IDBuffer, currentString.c_str()))
		{
			// None
			{
				if (ImGui::Selectable("None", bNoneSelected))
				{
					bModified = true;
					outSelectedIndex = -1;
				}

				if (bNoneSelected)
					ImGui::SetItemDefaultFocus();
			}

			for (int i = 0; i < options.size(); ++i)
			{
				bool isSelected = !bNoneSelected && (currentString == options[i]);

				if (ImGui::Selectable(options[i].c_str(), isSelected))
				{
					bModified = true;
					outSelectedIndex = i;
				}

				if (i < tooltipsSize)
					if (!tooltips[i].empty())
						Tooltip(tooltips[i]);

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (currentSelection < tooltipsSize)
			if (!tooltips[currentSelection].empty())
				Tooltip(tooltips[currentSelection]);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Combo(const std::string_view label, int currentValue, const ScriptEnumFields& fields, int& outSelectedValue)
	{
		std::string_view currentString;
		for (auto& [value, name] : fields)
		{
			if (value == currentValue)
				currentString = name;
		}

		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::BeginCombo(s_IDBuffer, currentString.data()))
		{
			for (auto& [value, name] : fields)
			{
				bool isSelected = (currentValue == value);

				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					bModified = true;
					outSelectedValue = value;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Button(const std::string_view label, const std::string_view buttonText, const ImVec2& size)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::SameLine();
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool result = ImGui::Button(buttonText.data(), size);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return result;
	}

	void Tooltip(const std::string_view tooltip, float treshHold)
	{
		if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > treshHold)
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(tooltip.data());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	void PushItemDisabled()
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	void PopItemDisabled()
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	void PushFrameBGColor(const glm::vec4& color)
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(color.r, color.g, color.b, color.a));
	}

	void PopFrameBGColor()
	{
		ImGui::PopStyleColor();
	}

	void HelpMarker(const std::string_view text)
	{
		// We don't want help marker to be disabled so we check the current state.
		// If the current state is disabled, we enable it and later restore the state
		const bool bDisabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled;
		if (bDisabled)
			UI::PopItemDisabled();

		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.data());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		if (bDisabled)
			UI::PushItemDisabled();
	}

	ButtonType ShowMessage(const std::string_view title, const std::string_view message, ButtonType buttons)
	{
		ButtonType pressedButton = ButtonType::None;
		ImGui::OpenPopup(title.data());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(message.data());
			ImGui::Separator();

			pressedButton = DrawButtons(buttons);

			ImGui::EndPopup();
		}

		return pressedButton;
	}
	
	ButtonType InputPopup(const std::string_view title, const std::string_view hint, std::string& input)
	{
		ButtonType buttons = ButtonType::OKCancel;

		ButtonType pressedButton = ButtonType::None;
		ImGui::OpenPopup(title.data());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			char buf[128] = {0};
			memcpy_s(buf, sizeof(buf), input.c_str(), input.size());
			if (ImGui::InputTextWithHint("##MyInputPopup", hint.data(), buf, sizeof(buf)))
				input = buf;

			ImGui::Separator();

			//Manually drawing OK because it needs to be disabled if input is empty.
			if (HasFlags(buttons, ButtonType::OK))
			{
				const bool bDisable = input.size() == 0;
				if (bDisable)
					UI::PushItemDisabled();
				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					pressedButton = ButtonType::OK;
					ImGui::CloseCurrentPopup();
				}
				if (bDisable)
					UI::PopItemDisabled();
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				buttons = ButtonType(buttons & (~ButtonType::OK)); //Removing OK from mask to draw buttons as usual without OK cuz we already drew it
			}
			if (auto pressed = DrawButtons(buttons); pressed != ButtonType::None)
				pressedButton = pressed;
			
			ImGui::EndPopup();
		}

		return pressedButton;
	}

	void Image(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!image)
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			VkSampler vkSampler = (VkSampler)Sampler::PointSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!texture || !texture->IsLoaded())
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void ImageMip(const Ref<Texture2D>& texture, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!texture || !texture->IsLoaded())
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return;

			ImageView imageView{ mip };
			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void ImageMip(const Ref<Eagle::Image>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			ImageView imageView{ mip };

			VkSampler vkSampler = (VkSampler)Sampler::PointSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	bool ImageButton(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
			return false;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			VkSampler vkSampler = (VkSampler)Sampler::PointSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGuiID id = (ImGuiID)((((uint64_t)vkImageView) >> 32) ^ (uint64_t)vkImageView);

			return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, bg_col, tint_col);
		}
		return false;
	}

	bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (!texture || !texture->IsLoaded())
			return false;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return false;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGuiID id = (ImGuiID)texture->GetGUID().GetHash();
			return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, bg_col, tint_col);
		}
		return false;
	}
	
}

namespace Eagle::UI::Editor
{
	void OpenTextureEditor(const Ref<AssetTexture2D>& asset, bool* outWindowOpened)
	{
		const auto& textureToView = asset->GetTexture();

		bool bHidden = !ImGui::Begin("Texture Editor", outWindowOpened);
		static bool detailsDocked = false;
		static bool bDetailsVisible;
		bDetailsVisible = (!bHidden) || (bHidden && !detailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();
		const uint32_t mipsCount = textureToView->GetMipsCount(); // MipsCount == 1 means no mips, just the original
		static int selectedMip = 0;
		selectedMip = glm::clamp(selectedMip, 0, (int)mipsCount - 1);

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
		: glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::ImageMip(Cast<Texture2D>(textureToView), uint32_t(selectedMip), { visualizeImageSize[0], visualizeImageSize[1] });
		if (bDetailsVisible)
		{
			float anisotropy = textureToView->GetAnisotropy();
			FilterMode filterMode = textureToView->GetFilterMode();
			AddressMode addressMode = textureToView->GetAddressMode();
			const float maxAnisotropy = RenderManager::GetCapabilities().MaxAnisotropy;

			const glm::ivec2 textureSize = glm::ivec2(textureToView->GetSize()) >> selectedMip;
			const std::string textureSizeString = std::to_string(textureSize.x) + "x" + std::to_string(textureSize.y);

			ImGui::Begin("Details");
			detailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureDetails");
			UI::Text("Name", asset->GetPath().stem().u8string());
			UI::Text("Resolution", textureSizeString);
			if (UI::PropertySlider("Anisotropy", anisotropy, 1.f, maxAnisotropy))
				textureToView->SetAnisotropy(anisotropy);

			// Filter mode
			{
				static std::vector<std::string> modesStrings = { "Nearest", "Bilinear", "Trilinear"};
				int selectedIndex = 0;
				if (UI::Combo("Filtering", (uint32_t)filterMode, modesStrings, selectedIndex))
				{
					filterMode = FilterMode(selectedIndex);
					textureToView->SetFilterMode(filterMode);
				}
			}

			// Address Mode
			{
				static std::vector<std::string> modesStrings = { "Wrap", "Mirror", "Clamp", "Clamp to Black", "Clamp to White" };

				int selectedIndex = 0;
				if (UI::Combo("Wrapping", (uint32_t)addressMode, modesStrings, selectedIndex))
				{
					addressMode = AddressMode(selectedIndex);
					textureToView->SetAddressMode(addressMode);
				}
			}

			// Visualize mips
			{
				static std::vector<std::string> modesStrings;
				if (modesStrings.size() != mipsCount)
				{
					modesStrings.resize(mipsCount);
					for (uint32_t i = 0; i < mipsCount; ++i)
						modesStrings[i] = "Mip #" + std::to_string(i);
				}

				UI::Combo("Visualize", (uint32_t)selectedMip, modesStrings, selectedMip, {}, "Mip #0 is the original texture");
			}

			// Generate Mips
			{
				static const Texture2D* s_LastTexture = nullptr;
				constexpr int minMips = 1;
				const int maxMips = (int)CalculateMipCount(textureToView->GetSize());

				static int generateMipsCount = 1;
				if (s_LastTexture != textureToView.get()) // Texture changed, reset mips slider
				{
					s_LastTexture = textureToView.get();
					generateMipsCount = textureToView->GetMipsCount();
				}
				generateMipsCount = glm::clamp(generateMipsCount, minMips, maxMips);

				UpdateIDBuffer("Generate Mips");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Generate Mips");
				ImGui::SameLine();
				UI::HelpMarker("Mips count = 1 means no mips, only the original texture");

				ImGui::NextColumn();

				{
					const float labelwidth = ImGui::CalcTextSize("Generate", NULL, true).x;
					const float buttonWidth = labelwidth + GImGui->Style.FramePadding.x * 8.0f;
					const float width = ImGui::GetColumnWidth();
					ImGui::PushItemWidth(width - buttonWidth);
				}
				ImGui::SliderInt(s_IDBuffer, &generateMipsCount, minMips, maxMips);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
					textureToView->GenerateMips(uint32_t(generateMipsCount));
			}

			UI::EndPropertyGrid();

			ImGui::Separator();

			if (ImGui::Button("Save"))
				Asset::Save(asset);

			ImGui::End();
		}

		ImGui::End();
	}

	void OpenTextureEditor(const Ref<AssetTextureCube>& asset, bool* outWindowOpened)
	{
		const Ref<Texture2D>& textureToView = asset->GetTexture()->GetTexture2D();

		bool bHidden = !ImGui::Begin("Texture Editor", outWindowOpened);
		static bool detailsDocked = false;
		static bool bDetailsVisible;
		bDetailsVisible = (!bHidden) || (bHidden && !detailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
		: glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::Image(Cast<Texture2D>(textureToView), { visualizeImageSize[0], visualizeImageSize[1] });
		if (bDetailsVisible)
		{
			const glm::ivec2 textureSize = glm::ivec2(textureToView->GetSize());
			const std::string textureSizeString = std::to_string(textureSize.x) + "x" + std::to_string(textureSize.y);

			ImGui::Begin("Details");
			detailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureDetails");
			UI::Text("Name", asset->GetPath().stem().u8string());
			UI::Text("Resolution", textureSizeString);

			UI::EndPropertyGrid();
			ImGui::End();
		}

		ImGui::End();
	}
	
	void OpenMaterialEditor(const Ref<AssetMaterial>& asset, bool* outWindowOpened)
	{
		static const char* s_MetalnessHelpMsg = "Controls how 'metal-like' surface looks like.\nDefault is 0";
		static const char* s_RoughnessHelpMsg = "Controls how rough surface looks like.\nRoughness of 0 is a mirror reflection and 1 is completely matte.\nDefault is 0.5";
		static const char* s_AOHelpMsg = "Can be used to affect how ambient lighting is applied to an object. If it's 0, ambient lighting won't affect it. Default is 1.0";
		static const char* s_BlendModeHelpMsg = "Translucent materials do not cast shadows!\nUse translucent materials with caution cause rendering them can be expensive";
		static const char* s_OpacityHelpMsg = "Controls the translucency of the material. 0 - fully transparent, 1 - fully opaque. Default is 0.5";
		static const char* s_OpacityMaskHelpMsg = "When in Masked mode, a material is either completely visible or completely invisible.\nValues below 0.5 are invisible";

		const auto& material = asset->GetMaterial();

		bool bHidden = !ImGui::Begin("Material Editor", outWindowOpened);
		UI::BeginPropertyGrid("MaterialDetails");

		UI::Text("Name", asset->GetPath().stem().u8string());

		Material::BlendMode blendMode = material->GetBlendMode();
		if (UI::ComboEnum("Blend Mode", blendMode, s_BlendModeHelpMsg))
			material->SetBlendMode(blendMode);

		Ref<AssetTexture2D> temp = material->GetAlbedoTexture();
		if (UI::DrawTexture2DSelection("Albedo", temp))
			material->SetAlbedoTexture(temp);

		temp = material->GetMetallnessTexture();
		if (UI::DrawTexture2DSelection("Metalness", temp, s_MetalnessHelpMsg))
			material->SetMetallnessTexture(temp);

		temp = material->GetNormalTexture();
		if (UI::DrawTexture2DSelection("Normal", temp))
			material->SetNormalTexture(temp);

		temp = material->GetRoughnessTexture();
		if (UI::DrawTexture2DSelection("Roughness", temp, s_RoughnessHelpMsg))
			material->SetRoughnessTexture(temp);

		temp = material->GetAOTexture();
		if (UI::DrawTexture2DSelection("Ambient Occlusion", temp, s_AOHelpMsg))
			material->SetAOTexture(temp);

		temp = material->GetEmissiveTexture();
		if (UI::DrawTexture2DSelection("Emissive Color", temp))
			material->SetEmissiveTexture(temp);


		// Disable if not translucent
		{
			const bool bTranslucent = blendMode == Material::BlendMode::Translucent;
			if (!bTranslucent)
				UI::PushItemDisabled();

			temp = material->GetOpacityTexture();
			if (UI::DrawTexture2DSelection("Opacity", temp, s_OpacityHelpMsg))
				material->SetOpacityTexture(temp);

			if (!bTranslucent)
				UI::PopItemDisabled();
		}

		// Disable if not masked
		{
			const bool bMasked = blendMode == Material::BlendMode::Masked;
			if (!bMasked)
				UI::PushItemDisabled();

			temp = material->GetOpacityMaskTexture();
			if (UI::DrawTexture2DSelection("Opacity Mask", temp, s_OpacityMaskHelpMsg))
				material->SetOpacityMaskTexture(temp);

			if (!bMasked)
				UI::PopItemDisabled();
		}

		glm::vec3 emissiveIntensity = material->GetEmissiveIntensity();
		if (UI::PropertyColor("Emissive Intensity", emissiveIntensity, true, "HDR"))
			material->SetEmissiveIntensity(emissiveIntensity);

		glm::vec4 tintColor = material->GetTintColor();
		if (UI::PropertyColor("Tint Color", tintColor, true))
			material->SetTintColor(tintColor);

		float tiling = material->GetTilingFactor();
		if (UI::PropertySlider("Tiling Factor", tiling, 1.f, 128.f))
			material->SetTilingFactor(tiling);

		UI::EndPropertyGrid();

		ImGui::Separator();
		if (ImGui::Button("Save"))
			Asset::Save(asset);

		ImGui::End();
	}

	void OpenPhysicsMaterialEditor(const Ref<AssetPhysicsMaterial>& asset, bool* outWindowOpened)
	{
		static const char* s_StaticFrictionHelpMsg = "Static friction defines the amount of friction that is applied between surfaces that are not moving lateral to each-other";
		static const char* s_DynamicFrictionHelpMsg = "Dynamic friction defines the amount of friction applied between surfaces that are moving relative to each-other";

		const auto& material = asset->GetMaterial();

		bool bHidden = !ImGui::Begin("Physics Material Editor", outWindowOpened);
		UI::BeginPropertyGrid("PhysicsMaterialDetails");

		UI::Text("Name", asset->GetPath().stem().u8string());

		bool bPhysicsMaterialChanged = false;
		bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
		bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
		bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);

		UI::EndPropertyGrid();

		ImGui::Separator();
		if (ImGui::Button("Save"))
			Asset::Save(asset);

		ImGui::End();
	}
}
