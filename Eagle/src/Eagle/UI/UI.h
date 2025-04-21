#pragma once

#include "Eagle/Core/EnumUtils.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/ThumbnailCache.h"
#include "imgui.h"
#include "magic_enum.hpp"
#include "magic_enum_utility.hpp"

namespace Eagle
{
	class Asset;
}

class ScriptEnumFields;

namespace Eagle::UI
{
	enum class ButtonType
	{
		None = 0,
		OK = 0b00000001,
		Cancel = 0b00000010,
		OKCancel = 0b00000011,
		Yes = 0b00000100,
		No = 0b00001000,
		YesNo = 0b00001100,
		YesNoCancel = 0b00001110
	};
	DECLARE_FLAGS(ButtonType);

	// maxItemWidth. Ignored if < 0
	template<class Type>
	bool DrawAssetSelection(const std::string_view label, Ref<Type>& modifyingAsset, const std::string_view helpMessage = "", float maxItemWidth = -1.f, const Ref<Eagle::Image>& preview = nullptr, bool* outPreviewClicked = nullptr)
	{
		const ImVec2 previewSize = ImVec2(32.f, 32.f);
		bool bResult = false;
		constexpr bool bRenderablePreview = ThumbnailCache::IsRenderableAssetType(Type::GetAssetType_Static());

		if constexpr (std::is_same<Type, AssetTexture2D>::value || std::is_same<Type, AssetTextureCube>::value)
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + previewSize.y * 0.5f - ImGui::CalcTextSize(label.data()).y * 0.5f); // Place text in the middle
		else if (preview)
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + previewSize.y * 0.5f - ImGui::CalcTextSize(label.data()).y * 0.5f);
		else
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const std::string assetName = modifyingAsset ? modifyingAsset->GetPath().stem().u8string() : "None";
		const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it
		ImGui::PushID(label.data());

		if constexpr (std::is_same<Type, AssetTexture2D>::value)
		{
			if (modifyingAsset || Texture2D::NoneIconTexture)
			{
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const bool bClicked = ImGui::InvisibleButton("##preview_inv_btn", previewSize);
				const bool bHovered = ImGui::IsItemHovered();
				ImGui::SetCursorScreenPos(p);

				UI::Image(modifyingAsset ? modifyingAsset->GetTexture() : Texture2D::NoneIconTexture, previewSize, { 0, 0 }, { 1, 1 }, bHovered && modifyingAsset ? ImVec4(0.5f, 0.5f, 0.5f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f));

				if (outPreviewClicked)
				{
					*outPreviewClicked = bClicked;
				}

				ImGui::SameLine();
			}
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		}
		else if constexpr (std::is_same<Type, AssetTextureCube>::value)
		{
			if ((modifyingAsset && modifyingAsset->GetTexture()->GetTexture2D()) || Texture2D::NoneIconTexture)
			{
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const bool bClicked = ImGui::InvisibleButton("##preview_inv_btn", previewSize);
				const bool bHovered = ImGui::IsItemHovered();
				ImGui::SetCursorScreenPos(p);

				UI::Image(modifyingAsset ? modifyingAsset->GetTexture()->GetTexture2D() : Texture2D::NoneIconTexture, previewSize, { 0, 0 }, { 1, 1 }, bHovered && modifyingAsset ? ImVec4(0.5f, 0.5f, 0.5f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f));

				if (outPreviewClicked)
				{
					*outPreviewClicked = bClicked;
				}

				ImGui::SameLine();
			}
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		}
		else
		{
			if (preview)
			{
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const bool bClicked = ImGui::InvisibleButton("##preview_inv_btn", previewSize);
				const bool bHovered = ImGui::IsItemHovered();
				ImGui::SetCursorScreenPos(p);

				UI::Image(preview, previewSize, { 0, 0 }, {1, 1}, bHovered && modifyingAsset ? ImVec4(0.5f, 0.5f, 0.5f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f));

				if (outPreviewClicked)
				{
					*outPreviewClicked = bClicked;
				}
				ImGui::SameLine();
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			}
		}

		const bool bApplyMaxWidth = maxItemWidth > 0.f;
		if (bApplyMaxWidth)
			ImGui::PushItemWidth(maxItemWidth);

		bool bBeginCombo = ImGui::BeginCombo("##", assetName.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			auto processAssetDrop = [&](AssetType assetType)
			{
				if (assetType == AssetType::None)
					return;

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GetAssetDragDropCellTag(assetType)))
				{
					const wchar_t* payload_n = (const wchar_t*)payload->Data;
					Path filepath = Path(payload_n);
					Ref<Asset> asset;

					if (AssetManager::Get(filepath, &asset) == false)
					{
						asset = Type::Create(filepath);
						AssetManager::Register(asset);
					}
					bResult = asset != modifyingAsset;
					if (bResult)
						modifyingAsset = Cast<Type>(asset);
				}
			};

			if constexpr (std::is_same<Type, Asset>::value)
			{
				magic_enum::enum_for_each<AssetType>([&](auto val)
				{
					processAssetDrop(val);
				});
			}
			else
			{
				processAssetDrop(Type::GetAssetType_Static());
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;
			// Initially find currently selected asset to scroll to it.
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
					if (const auto& castedAsset = Cast<Type>(asset))
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

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}

			//Drawing all existing asset
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto castedAsset = Cast<Type>(asset);
				if (!castedAsset)
					continue;

				const bool bSelected = currentItemIdx == i;
				ImGui::PushID((void*)asset->GetGUID().GetHash());

				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, previewSize.y});
				bSelectableTriggered |= ImGui::IsItemClicked();

				bool bHasPreview = false;
				if constexpr (std::is_same<Type, Asset>::value)
				{
					if (const auto texture2DAsset = Cast<AssetTexture2D>(asset))
					{
						ImGui::SameLine();
						UI::Image(texture2DAsset->GetTexture(), previewSize);
						bHasPreview = true;
					}
					else if (const auto textureCubeAsset = Cast<AssetTextureCube>(asset))
					{
						if (const auto& texture2D = textureCubeAsset->GetTexture()->GetTexture2D())
						{
							ImGui::SameLine();
							UI::Image(texture2D, previewSize);
							bHasPreview = true;
						}
					}
					else if (bRenderablePreview)
					{
						ImGui::SameLine();
						Ref<Eagle::Image> preview = ThumbnailCache::Get(asset);
						if (!preview)
						{
							if (ThumbnailCache::IsRenderableAssetType(asset->GetAssetType()))
							{
								if (ThumbnailCache::Render(asset, ThumbnailCache::GetThumbnailSize()))
								{
									preview = ThumbnailCache::Get(asset);
								}
							}
						}
						UI::Image(preview ? preview : Texture2D::NoneIconTexture->GetImage(), previewSize);
						bHasPreview = true;
					}
				}
				else if constexpr (std::is_same<Type, AssetTexture2D>::value)
				{
					ImGui::SameLine();
					UI::Image(castedAsset->GetTexture(), previewSize);
					bHasPreview = true;
				}
				else if constexpr (std::is_same<Type, AssetTextureCube>::value)
				{
					if (const auto& texture2D = castedAsset->GetTexture()->GetTexture2D())
					{
						ImGui::SameLine();
						UI::Image(texture2D, previewSize);
						bHasPreview = true;
					}
				}
				else if constexpr (bRenderablePreview)
				{
					ImGui::SameLine();
					Ref<Eagle::Image> preview = ThumbnailCache::Get(asset);
					if (!preview)
					{
						if (ThumbnailCache::IsRenderableAssetType(asset->GetAssetType()))
						{
							if (ThumbnailCache::Render(asset, ThumbnailCache::GetThumbnailSize()))
							{
								preview = ThumbnailCache::Get(asset);
							}
						}
					}
					UI::Image(preview ? preview : Texture2D::NoneIconTexture->GetImage(), previewSize);
					bHasPreview = true;
				}

				ImGui::SameLine();
				if (bHasPreview)
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + previewSize.y * 0.25f);
				ImGui::Text("%s", path.stem().u8string().c_str());

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableTriggered)
				{
					currentItemIdx = i;

					modifyingAsset = castedAsset;
					bResult = true;
				}
				++i;
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		if (bApplyMaxWidth)
			ImGui::PopItemWidth();

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		ImGui::PopID();

		return bResult;
	}

	template<class Type>
	bool DrawAssetSelection(const std::string_view label, Ref<Type>& modifyingAsset, const Ref<Eagle::Image>& preview, bool* outPreviewClicked = nullptr)
	{
		return DrawAssetSelection(label, modifyingAsset, "", -1.f, preview, outPreviewClicked);
	}

	// @bReturnOnEnter. If set to true, the function won't return true while the values is being changed. True will be returned after a user stops editing the value
	bool DrawVec3Control(const std::string_view label, glm::vec3& values, const glm::vec3& resetValues = glm::vec3{ 0.f }, float columnWidth = 100.f, bool bReturnOnEnter = false);
	bool DrawQuatControl(const std::string_view label, glm::quat& values, const glm::quat& resetValues = glm::quat(1.f, 0.f, 0.f, 0.f), float columnWidth = 100.f, bool bReturnOnEnter = false);

	ButtonType DrawButtons(ButtonType buttons);

	//Grid Name needs to be unique
	void BeginPropertyGrid(const std::string_view gridName);
	void EndPropertyGrid();

	bool Property(const std::string_view label, bool& value, const std::string_view helpMessage = "");
	bool Property(const std::string_view label, const std::vector<std::string>& customLabels, bool* values, const std::string_view helpMessage = "");
	bool PropertyText(const std::string_view label, std::string& value, const std::string_view helpMessage = "", ImGuiInputTextFlags flags = 0);
	bool PropertyTextMultiline(const std::string_view label, std::string& value, const std::string_view helpMessage = "");
	bool PropertyText(const std::string_view label, std::vector<std::string>& values, const std::string_view helpMessage = "");

	bool Text(const std::string_view label, const std::string_view text, const std::string_view helpMessage = "");
	bool TextLink(const std::string_view text, const std::string_view url);
	bool BulletLink(const std::string_view text, const std::string_view url);

	bool PropertyDrag(const std::string_view label, int& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, uint32_t& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, float& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::vec2& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::vec3& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::vec4& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::ivec3& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::uvec3& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");

	bool PropertySlider(const std::string_view label, int& value, int min, int max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, uint32_t& value, int min, int max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, float& value, float min, float max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, glm::vec2& value, float min, float max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, glm::vec3& value, float min, float max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, glm::vec4& value, float min, float max, const std::string_view helpMessage = "");

	bool PropertyColor(const std::string_view label, glm::vec3& value, bool bHDR = false, const std::string_view helpMessage = "");
	bool PropertyColor(const std::string_view label, glm::vec4& value, bool bHDR = false, const std::string_view helpMessage = "");

	bool InputFloat(const std::string_view label, float& value, float step = 0.f, float stepFast = 0.f, const std::string_view helpMessage = "");
	bool InputText(const std::string_view label, std::string& value, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None, const std::string_view helpMessage = "");
	
	//Returns true if selection changed.
	//outSelectedIndex - index of the selected option
	// ComboWithNone adds a 'None' option as first. outSelectedIndex is -1 if None is selected
	// OptionsSize can be smaller to cutoff some options
	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, size_t optionsSize, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool ComboWithNone(const std::string_view label, int currentSelectionIndex, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool Combo(const std::string_view label, int currentValue, const ScriptEnumFields& fields, int& outSelectedValue);

	template <typename Enum>
	bool ComboEnum(const std::string_view label, Enum& current, const std::string_view helpMessage = "")
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

		const auto currentName = magic_enum::enum_name(current);
		if (ImGui::BeginCombo(GetIDBuffer(), currentName.data()))
		{
			constexpr auto& entries = magic_enum::enum_entries<Enum>();
			for (size_t i = 0; i < entries.size(); ++i)
			{
				const auto& entry = entries[i];
				bool isSelected = (current == entry.first);

				if (ImGui::Selectable(entry.second.data(), isSelected))
				{
					bModified = true;
					current = entry.first;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	template <typename Enum>
	bool RadioButtonsEnum(const std::string_view label, Enum& current, const std::string_view helpMessage = "")
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
		ImGui::PushID(GetIDBuffer());

		constexpr auto& entries = magic_enum::enum_entries<Enum>();
		constexpr size_t numEntries = entries.size();
		int currentInt = int(current);
		for (size_t i = 0; i < numEntries; ++i)
		{
			const auto& entry = entries[i];

			if (ImGui::RadioButton(entry.second.data(), &currentInt, int(entry.first)))
			{
				current = (Enum)currentInt;
				bModified = true;
			}

			if (i != (numEntries - 1))
				ImGui::SameLine();
		}

		ImGui::PopID();
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Button(const std::string_view label, const std::string_view buttonText, const ImVec2& size = ImVec2(0, 0));

	void Tooltip(const std::string_view tooltip, float treshHold = EG_HOVER_THRESHOLD);

	void TextWithSeparator(const std::string_view text, float thickness = 2.5f);

	void PushItemDisabled();
	void PopItemDisabled();

	void PushFrameBGColor(const glm::vec4& color);
	void PopFrameBGColor();

	void PushButtonSelectedStyleColors();
	void PopButtonSelectedStyleColors();

	void HelpMarker(const std::string_view text);

	ButtonType ShowMessage(const std::string_view title, const std::string_view message, ButtonType buttons);
	ButtonType InputPopup(const std::string_view title, const std::string_view hint, std::string& input);

	void Image(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void ImageMip(const Ref<Eagle::Image>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void ImageMip(const Ref<Texture2D>& texture, uint32_t mip, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	bool ImageButton(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
	bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
	bool ImageButtonRotated(const Ref<Texture2D>& texture, const ImVec2& size, float angleRad, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
	bool ImageButtonRotated(ImGuiID id, ImTextureID textureID, const ImVec2& size, float angleRad, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
	void AddImage(const Ref<Eagle::Image>& image, const ImVec2& min, const ImVec2& max, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), uint32_t col = IM_COL32_WHITE);
	void AddImage(const Ref<Texture2D>& texture, const ImVec2& min, const ImVec2& max, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), uint32_t col = IM_COL32_WHITE);
	bool ImageButtonWithText(const Ref<Eagle::Image>& image, const std::string_view text, ImVec2 size, bool bFillFrameDefault = true, float borderSize = 1.f, float textHeightOffset = 0.f, ImVec2 framePadding = ImVec2{ 0, 0 });
	bool ImageButtonWithText(const Ref<Texture2D>& texture, const std::string_view text, ImVec2 size, bool bFillFrameDefault = true, float borderSize = 1.f, float textHeightOffset = 0.f, ImVec2 framePadding = ImVec2{ 0, 0 });
	bool ImageButtonWithTextHorizontal(const Ref<Texture2D>& image, const std::string_view text, ImVec2 size, float frameHeight, bool bFillFrameDefault = true);
	ImTextureID GetTextureID(const Ref<Texture2D>& texture);

	int TextResizeCallback(ImGuiInputTextCallbackData* data);

	// Internal usage only
	void UpdateIDBuffer(const std::string_view label);
	const char* GetIDBuffer();
}
