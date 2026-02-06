#include "egpch.h"

#include "UI.h"
#include "Eagle/Classes/Font.h"

#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Renderer/TextureCompressor.h"

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
	static constexpr char* s_HelpMarker = "(?)";

	bool HandlePublicField(std::string_view label, PublicField& field, MonoObject* instance, size_t fieldIndex, bool bRuntime, Entity entity)
	{
		bool bChanged = false;
		switch (field.Type)
		{
			case FieldType::Int:
			case FieldType::UnsignedInt:
			{
				int value = bRuntime ? field.GetRuntimeValue<int>(instance, fieldIndex) : field.GetStoredValue<int>(fieldIndex);
				if (UI::PropertyDrag(label.data(), value, 1, 0, 0, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Float:
			{
				float value = bRuntime ? field.GetRuntimeValue<float>(instance, fieldIndex) : field.GetStoredValue<float>(fieldIndex);
				if (UI::PropertyDrag(label.data(), value, 1, 0, 0, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::String:
			{
				std::string value = bRuntime ? field.GetRuntimeValue<std::string>(instance, fieldIndex) : field.GetStoredValue<std::string>(fieldIndex);
				if (UI::PropertyText(label.data(), value, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue<std::string>(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Vec2:
			{
				glm::vec2 value = bRuntime ? field.GetRuntimeValue<glm::vec2>(instance, fieldIndex) : field.GetStoredValue<glm::vec2>(fieldIndex);
				if (UI::PropertyDrag(label.data(), value, 1, 0, 0, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Vec3:
			{
				glm::vec3 value = bRuntime ? field.GetRuntimeValue<glm::vec3>(instance, fieldIndex) : field.GetStoredValue<glm::vec3>(fieldIndex);
				if (UI::PropertyDrag(label.data(), value, 1, 0, 0, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Vec4:
			{
				glm::vec4 value = bRuntime ? field.GetRuntimeValue<glm::vec4>(instance, fieldIndex) : field.GetStoredValue<glm::vec4>(fieldIndex);
				if (UI::PropertyDrag(label.data(), value, 1, 0, 0, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Bool:
			{
				bool value = bRuntime ? field.GetRuntimeValue<bool>(instance, fieldIndex) : field.GetStoredValue<bool>(fieldIndex);
				if (UI::Property(label.data(), value, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Color3:
			{
				glm::vec3 value = bRuntime ? field.GetRuntimeValue<glm::vec3>(instance, fieldIndex) : field.GetStoredValue<glm::vec3>(fieldIndex);
				if (UI::PropertyColor(label.data(), value, true, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Color4:
			{
				glm::vec4 value = bRuntime ? field.GetRuntimeValue<glm::vec4>(instance, fieldIndex) : field.GetStoredValue<glm::vec4>(fieldIndex);
				if (UI::PropertyColor(label.data(), value, true, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Enum:
			{
				int value = bRuntime ? field.GetRuntimeValue<int>(instance, fieldIndex) : field.GetStoredValue<int>(fieldIndex);
				if (UI::Combo(label.data(), value, field.EnumFields, value, field.Tooltip))
				{
					bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
					bChanged = true;
				}
				break;
			}
			case FieldType::Entity:
			{
				if (entity)
				{
					const auto& scene = entity.GetScene();
					GUID value = bRuntime ? field.GetRuntimeValue<GUID>(instance, fieldIndex) : field.GetStoredValue<GUID>(fieldIndex);
					Entity userEntity = scene->GetEntityByGUID(value);
					const bool bValid = userEntity.IsValid();
					int currentSelection = -1;
					int i = 0;

					const auto entities = scene->GetAllEntitiesWith<IDComponent, EntitySceneNameComponent>();
					std::vector<std::string> names;
					std::vector<GUID> ids;
					names.reserve(entities.size_hint());
					ids.reserve(entities.size_hint());

					for (auto& [sceneEntity, idComp, nameComp] : entities.each())
					{
						if (sceneEntity == entity.GetEnttID())
							continue; // Don't show itself

						const auto& ID = idComp.ID;
						const auto& name = nameComp.Name;
						ids.emplace_back(ID);
						names.emplace_back(name);

						if (bValid && (ID == value))
						{
							currentSelection = i;
						}
						i++;
					}

					const bool bComboChanged = UI::ComboWithNone(label.data(), currentSelection, names, currentSelection, {}, field.Tooltip);
					const bool bInvalidEntity = currentSelection == -1 && value != GUID(0, 0); // Can happen if an entity was removed from the scene
					if (bComboChanged || bInvalidEntity)
					{
						value = currentSelection == -1 ? GUID(0, 0) : ids[currentSelection];
						bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);
						bChanged = true;
					}
				}
				break;
			}

#define AssetField_Case(type) \
				case FieldType::type:\
				{\
					GUID value = bRuntime ? field.GetRuntimeValue<GUID>(instance, fieldIndex) : field.GetStoredValue<GUID>(fieldIndex);\
					Ref<Asset> asset;\
					Ref<type> castedAsset;\
					if (AssetManager::Get(value, &asset))\
						castedAsset = Cast<type>(asset);\
					if (UI::DrawAssetSelection(label.data(), castedAsset, field.Tooltip, -1.f, GetAssetPreview(castedAsset)))\
					{\
						value = castedAsset ? castedAsset->GetGUID() : GUID(0, 0);\
						bRuntime ? field.SetRuntimeValue(instance, value, fieldIndex) : field.SetStoredValue(value, fieldIndex);\
						bChanged = true;\
					}\
					break;\
				}

			AssetField_Case(Asset);
			AssetField_Case(AssetTexture2D);
			AssetField_Case(AssetTextureCube);
			AssetField_Case(AssetStaticMesh);
			AssetField_Case(AssetSkeletalMesh);
			AssetField_Case(AssetAudio);
			AssetField_Case(AssetSoundGroup);
			AssetField_Case(AssetFont);
			AssetField_Case(AssetMaterial);
			AssetField_Case(AssetPhysicsMaterial);
			AssetField_Case(AssetEntity);
			AssetField_Case(AssetScene);
			AssetField_Case(AssetAnimation);
			AssetField_Case(AssetAnimationGraph);
			AssetField_Case(AssetParticleSystem);
			AssetField_Case(AssetAnimationBlendSpace);
			AssetField_Case(AssetBehaviorGraph);
#undef AssetField_Case
		}

		return bChanged;
	}

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

	ButtonType UI::DrawButtons(ButtonType buttons)
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

	const Ref<Eagle::Image> GetAssetPreview(const Ref<Asset>& asset)
	{
		if (!asset)
			return Texture2D::NoneIconTexture->GetImage();

		return ThumbnailCache::Get(asset);
	}

	bool DrawVec3Control(const std::string_view label, glm::vec3& values, const glm::vec3& resetValues /* = glm::vec3{ 0.f }*/, float columnWidth /*= 100.f*/, bool bReturnOnEnter /* = false */)
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

		float lineHeight = (GImGui->FontBaked->Size * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
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
		if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
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
		if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
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
		if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::PopItemWidth();
		UI::Tooltip(std::to_string(values.z));

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return bValueChanged;
	}

	bool DrawQuatControl(const std::string_view label, glm::quat& values, const glm::quat& resetValues, float columnWidth, bool bReturnOnEnter)
	{
		bool bValueChanged = false;
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.data());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.data());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });

		float lineHeight = (GImGui->FontBaked->Size * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
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
		if (ImGui::DragFloat("##X", &values.x, 0.01f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
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
		if (ImGui::DragFloat("##Y", &values.y, 0.01f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
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
		if (ImGui::DragFloat("##Z", &values.z, 0.01f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::PopItemWidth();
		UI::Tooltip(std::to_string(values.z));
		ImGui::SameLine();

		//W
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.8f, 0.8f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.9f, 0.9f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.8f, 0.8f, 1.f });
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("W", buttonSize))
		{
			values.w = resetValues.w;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		if (ImGui::DragFloat("##W", &values.w, 0.01f, 0.f, 0.f, "%.4f"))
		{
			if (!bReturnOnEnter)
				bValueChanged = true;
		}
		if (bReturnOnEnter)
			bValueChanged |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::PopItemWidth();
		UI::Tooltip(std::to_string(values.w));

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		if (bValueChanged)
		{
			if (glm::all(glm::epsilonEqual(values, glm::quat(0, 0, 0, 0), 0.001f)))
				values.w = 1.f;
			values = glm::normalize(values);
		}

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

	bool PropertyText(const std::string_view label, std::string& value, const std::string_view helpMessage, ImGuiInputTextFlags flags)
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

		if (UI::InputText(s_IDBuffer, value, flags))
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

	bool PropertyText(const std::string_view label, std::vector<std::string>& values, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::PushID(s_IDBuffer);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();

		{
			if (ImGui::Button("Remove"))
			{
				values.pop_back();
				bModified = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add"))
			{
				values.emplace_back();
				bModified = true;
			}
		}

		if (!values.empty())
			ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		
		size_t i = 1;
		for (auto& value : values)
		{
			ImGui::Text(("\tElement #" + std::to_string(i++)).c_str());
			ImGui::NextColumn();
			ImGui::PushID(&value);

			if (ImGui::InputText(s_IDBuffer, value.data(), value.length() + 1, ImGuiInputTextFlags_CallbackResize, TextResizeCallback, &value))
				bModified = true;
			if (ImGui::IsItemActive())
				ImGui::SetItemKeyOwner(ImGuiMod_Alt);

			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::PopItemWidth();
		if (values.empty())
			ImGui::NextColumn();

		ImGui::PopID();

		return bModified;
	}

	bool Property(PublicField& field, MonoObject* instance, bool bRuntime, Entity entity)
	{
		if (bRuntime && !instance)
		{
			EG_CORE_ASSERT(false, "Instance must be valid if it's a runtime");
			return false;
		}
		bool bChanged = false;
		ImGui::PushID(field.FullName.c_str());

		if (field.bArray)
		{
			constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

			const size_t arrayLength = bRuntime ? field.GetRuntimeArrayLength(instance) : field.ArrayLength;
			const std::string elementsStr = "Array elements: " + std::to_string(arrayLength);

			// Calculate `collapser arrow width` offset
			// in order to move a tree to the left so that children names are all aligned vertically
			float treeOffsetX = 0.f;
			{
				ImGuiContext& g = *GImGui;
				const ImGuiStyle& style = g.Style;
				const bool display_frame = (treeFlags & ImGuiTreeNodeFlags_Framed) != 0;
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				const ImVec2 padding = (display_frame || (treeFlags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));
				treeOffsetX = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);
			}

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - treeOffsetX * 0.5f + 5.f);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			bool entityTreeOpened = ImGui::TreeNodeEx(field.UIName.data(), treeFlags, field.UIName.data());
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			ImGui::Text(elementsStr.c_str());
			ImGui::PopItemWidth();
			ImGui::NextColumn();

			if (entityTreeOpened)
			{
				for (size_t i = 0; i < arrayLength; ++i)
				{
					ImGui::PushID(int(i));
					bChanged |= HandlePublicField(std::to_string(i), field, instance, i, bRuntime, entity);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}
		else
		{
			bChanged |= HandlePublicField(field.UIName, field, instance, 0, bRuntime, entity);
		}

		ImGui::PopID();
		return bChanged;
	}

	bool Text(const std::string_view label, const std::string_view text, const std::string_view helpMessage)
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
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
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

	bool PropertyDrag(const std::string_view label, glm::ivec3& value, float speed, int min, int max, const std::string_view helpMessage)
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

		bModified = ImGui::DragInt3(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, glm::uvec3& value, float speed, int min, int max, const std::string_view helpMessage)
	{
		glm::ivec3 temp = value;
		const bool bChanged = PropertyDrag(label, temp, speed, min, max, helpMessage);
		if (bChanged)
			value = glm::uvec3(glm::max(temp, 0)); // Clamp negatives to 0 so that we don't overflow
		return bChanged;
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

	bool PropertySlider(const std::string_view label, uint32_t& value, int min, int max, const std::string_view helpMessage)
	{
		int temp = (int)value;
		const bool bChanged = PropertySlider(label, temp, min, max, helpMessage);
		if (bChanged)
			value = uint32_t(glm::max(temp, 0)); // Clamp negatives to 0 so that we don't overflow
		return bChanged;
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

	bool PropertyBitMask(const std::string_view label, uint32_t& value, const std::vector<std::pair<std::string, uint32_t>>& masks, const std::string_view helpMessage)
	{
		bool bModified = false;

		ImGui::PushID(label.data());
		for (const auto& maskInfo : masks)
		{
			const auto& name = maskInfo.first;
			const auto& mask = maskInfo.second;

			bool bChecked = mask & value;
			if (UI::Property(name, bChecked))
			{
				// If it was checked, add the mask, otherwise remove it
				SetFlag(value, mask, bChecked);
				bModified = true;
			}
		}
		ImGui::PopID();

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

	bool InputDouble(const std::string_view label, double& value, double step, double stepFast, const std::string_view helpMessage)
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

		bool result = ImGui::InputDouble(s_IDBuffer, &value, step, stepFast);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return result;
	}

	bool InputText(const std::string_view label, std::string& value, ImGuiInputTextFlags flags, const std::string_view helpMessage)
	{
		const bool bChanged = ImGui::InputText(label.data(), value.data(), value.length() + 1, flags | ImGuiInputTextFlags_CallbackResize, UI::TextResizeCallback, &value);
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		return bChanged;
	}

	bool InputTextWithHint(const std::string_view label, std::string& value, std::string_view hint, ImGuiInputTextFlags flags, const std::string_view helpMessage)
	{
		const bool bChanged = ImGui::InputTextWithHint(label.data(), hint.data(), value.data(), value.length() + 1, flags | ImGuiInputTextFlags_CallbackResize, UI::TextResizeCallback, &value);
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		return bChanged;
	}

	bool InputTextMultiline(const std::string_view label, std::string& value, ImGuiInputTextFlags flags, const std::string_view helpMessage)
	{
		constexpr ImGuiInputTextFlags defaultFlags = ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput;

		const bool bChanged = ImGui::InputTextMultiline(label.data(), value.data(), value.length() + 1, ImVec2(0, 0), defaultFlags | ImGuiInputTextFlags_CallbackResize, UI::TextResizeCallback, &value);
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		return bChanged;
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

	bool ComboWithNone(const std::string_view label, int currentSelectionIndex, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
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

		const bool bNoneSelected = currentSelectionIndex == -1;
		const std::string& currentString = bNoneSelected ? "None" : options[currentSelectionIndex];
		if (ImGui::BeginCombo(s_IDBuffer, currentString.c_str()))
		{
			// None
			{
				ImGui::PushID(-1);
				if (ImGui::Selectable("None", bNoneSelected))
				{
					bModified = true;
					outSelectedIndex = -1;
				}

				if (bNoneSelected)
					ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}

			for (int i = 0; i < options.size(); ++i)
			{
				bool isSelected = currentSelectionIndex == i;
				ImGui::PushID(i);

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
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		if (currentSelectionIndex < tooltipsSize)
			if (!tooltips[currentSelectionIndex].empty())
				Tooltip(tooltips[currentSelectionIndex]);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool ComboWithNone(const std::string_view label, std::string& moduleName, const std::map<std::string, EntityScriptClass>& entityClasses)
	{
		const bool bNoneSelected = moduleName.empty() || !ScriptEngine::ModuleExists(moduleName);
		const char* currentName = bNoneSelected ? "None" : moduleName.c_str();

		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::BeginCombo(s_IDBuffer, currentName))
		{
			// None
			{
				ImGui::PushID(-1);
				if (ImGui::Selectable("None", bNoneSelected))
				{
					moduleName = "";
					bModified = true;
				}

				if (bNoneSelected)
					ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}
			
			int i = 0;
			for (const auto& [name, data] : entityClasses)
			{
				const bool isSelected = name == moduleName;
				ImGui::PushID(i++);

				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					moduleName = name;
					bModified = true;
				}

				if (!data.ClassData.Tooltip.empty())
				{
					ImGui::SameLine();
					UI::HelpMarker(data.ClassData.Tooltip);
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Combo(const std::string_view label, int currentValue, const ScriptEnumFields& fields, int& outSelectedValue, const std::string_view helpMessage)
	{
		std::string_view currentString;
		for (auto& [value, data] : fields)
		{
			if (value == currentValue)
				currentString = data.Name;
		}

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
			for (auto& [value, data] : fields)
			{
				bool isSelected = (currentValue == value);

				if (ImGui::Selectable(data.Name.c_str(), isSelected))
				{
					bModified = true;
					outSelectedValue = value;
				}

				if (!data.Tooltip.empty())
				{
					ImGui::SameLine();
					UI::HelpMarker(data.Tooltip);
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

	void TextWithSeparator(const std::string_view text, float thickness, const std::string_view helpMessage)
	{
		const int columns = ImGui::GetColumnsCount();
		ImGui::Columns(1);

		auto* window = ImGui::GetCurrentWindow();
		const auto& style = ImGui::GetStyle();
		
		ImVec4 textColor = style.Colors[ImGuiCol_Text];
		textColor.x *= 0.75f;
		textColor.y *= 0.75f;
		textColor.z *= 0.75f;

		ImGui::PushStyleColor(ImGuiCol_Text, textColor);
		ImGui::Text(text.data());
		ImGui::PopStyleColor();

		const float paddingX = style.FramePadding.x;
		ImVec2 size = ImGui::CalcTextSize(text.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
			size.x += ImGui::CalcTextSize(s_HelpMarker).x + paddingX;
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(0.0f);
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const ImVec2 start = ImVec2(size.x + (window->DC.TreeDepth * style.IndentSpacing) + (paddingX * 4.0f), size.y * 0.5f) + pos;
		const ImVec2 end = pos + ImVec2(ImGui::GetWindowWidth() - paddingX - window->ScrollbarSizes.x, size.y * 0.5f);
		window->DrawList->AddLine(start, end, ImGui::GetColorU32(ImGuiCol_Separator), thickness);

		ImGui::Dummy(ImVec2(0.0f, size.y));

		ImGui::Columns(columns);
	}

	void PushItemDisabled()
	{
		// If already disabled, don't make it more dimmer
		const bool bDisabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled;

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (bDisabled ? 1.f : 0.5f));
	}

	void PopItemDisabled()
	{
		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
	}

	void PushFrameBGColor(const glm::vec4& color)
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(color.r, color.g, color.b, color.a));
	}

	void PopFrameBGColor()
	{
		ImGui::PopStyleColor();
	}

	void PushButtonSelectedStyleColors()
	{
		const float coef = 1.5f;
		const auto& colors = ImGui::GetStyle().Colors;
		ImVec4 button = colors[ImGuiCol_Button];
		button.x *= coef; button.y *= coef; button.z *= coef;

		ImVec4 buttonHovered = colors[ImGuiCol_ButtonHovered];
		buttonHovered.x *= coef; buttonHovered.y *= coef; buttonHovered.z *= coef;

		ImVec4 buttonActive = colors[ImGuiCol_ButtonActive];
		buttonActive.x *= coef; buttonActive.y *= coef; buttonActive.z *= coef;

		ImGui::PushStyleColor(ImGuiCol_Button, button);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonHovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonActive);
	}

	void PopButtonSelectedStyleColors()
	{
		ImGui::PopStyleColor(3);
	}

	void HelpMarker(const std::string_view text)
	{
		// We don't want help marker to be disabled so we check the current state.
		// If the current state is disabled, we enable it and later restore the state
		size_t disabledCount = 0;
		while ((GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled)
		{
			disabledCount++;
			UI::PopItemDisabled();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
		ImGui::TextDisabled(s_HelpMarker);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.data());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
		ImGui::PopStyleVar();

		// Restore
		for (size_t i = 0; i < disabledCount; ++i)
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
			// Set focus on input text
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
				ImGui::SetKeyboardFocusHere(0);

			if (ImGui::InputTextWithHint("##MyInputPopup", hint.data(), input.data(), input.length() + 1, ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_EnterReturnsTrue, UI::TextResizeCallback, &input))
			{
				if (!input.empty())
					pressedButton = ButtonType::OK;
			}

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
			VkSampler vkSampler = (VkSampler)Sampler::BilinearSampler->GetHandle();
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
			if (!image)
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
			if (!image)
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
		if (!image)
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			ImageView imageView{ mip };

			VkSampler vkSampler = (VkSampler)Sampler::BilinearSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	bool ImageButton(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (!image)
			return false;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			VkSampler vkSampler = (VkSampler)Sampler::BilinearSampler->GetHandle();
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
			if (!image)
				return false;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGuiID id = (ImGuiID)texture->GetGUID().GetHash();
			return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, bg_col, tint_col);
		}
		return false;
	}

	bool ImageButtonRotated(const Ref<Texture2D>& texture, const ImVec2& size, float angleRad, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (!texture || !texture->IsLoaded())
			return false;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image)
				return false;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const ImTextureRef textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGuiID id = (ImGuiID)texture->GetGUID().GetHash();
			return ImGui::ImageButtonRotatedEx(id, textureID.GetTexID(), size, angleRad, uv0, uv1, bg_col, tint_col);
		}
		return false;
	}

	bool ImageButtonRotated(ImGuiID id, ImTextureID textureID, const ImVec2& size, float angleRad, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		return ImGui::ImageButtonRotatedEx(id, textureID, size, angleRad, uv0, uv1, bg_col, tint_col);
	}

	void AddImage(const Ref<Eagle::Image>& image, const ImVec2& min, const ImVec2& max, const ImVec2& uv0, const ImVec2& uv1, uint32_t col)
	{
		if (!image)
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			constexpr uint32_t mip = 0;
			ImageView imageView{ mip };
			VkSampler vkSampler = (VkSampler)Sampler::BilinearSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::GetWindowDrawList()->AddImage(textureID, min, max, uv0, uv1, col);
		}
	}

	void AddImage(const Ref<Texture2D>& texture, const ImVec2& min, const ImVec2& max, const ImVec2& uv0, const ImVec2& uv1, uint32_t col)
	{
		if (!texture || !texture->IsLoaded())
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image)
				return;

			constexpr uint32_t mip = 0;
			ImageView imageView{ mip };
			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::GetWindowDrawList()->AddImage(textureID, min, max, uv0, uv1, col);
		}
	}

	bool ImageButtonWithText(const Ref<Eagle::Image>& image, const std::string_view text, ImVec2 size, bool bFillFrameDefault, float borderSize, float textHeightOffset, ImVec2 framePadding)
	{
		ImGuiContext& g = *GImGui;
		const ImVec2 padding = g.Style.FramePadding;
		const ImVec2 textSize = ImGui::CalcTextSize(text.data(), NULL, true);
		const float itemSpacingHeight = g.Style.ItemSpacing.y;

		ImVec2 frameSize = size + framePadding;
		frameSize.y += textSize.y + 2.f * itemSpacingHeight; // 2 item spacings because we add padding at the top and at the bottom

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + frameSize);

		ImVec2 p = ImGui::GetCursorScreenPos();
		const bool bResult = ImGui::InvisibleButton(text.data(), frameSize);

		// Save the data to restore them at the end
		const ImVec2 prevLine = window->DC.CursorPosPrevLine;
		const ImVec2 curLine = window->DC.CursorPos;
		auto buttonItemData = g.LastItemData;

		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = bHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		auto& colors = ImGui::GetStyle().Colors;
		if (!bFillFrameDefault)
			ImGui::PushStyleColor(ImGuiCol_Button, colors[ImGuiCol_WindowBg]);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, borderSize);
		
		const ImU32 col = ImGui::GetColorU32((bHeld && bHovered) ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));
		
		ImGui::PopStyleVar();
		if (!bFillFrameDefault)
			ImGui::PopStyleColor();

		UI::AddImage(image, p + padding * 0.5f, p + size - padding * 0.5f);

		// Centering text
		ImGui::SetCursorScreenPos(ImVec2(glm::max(p.x, p.x + 0.5f * (size.x - textSize.x)), p.y + size.y + itemSpacingHeight + textHeightOffset));

		ImGui::Text(text.data());

		window->DC.CursorPos = curLine;
		window->DC.CursorPosPrevLine = prevLine;
		g.LastItemData = buttonItemData;

		return bResult;
	}

	bool ImageButtonWithText(const Ref<Texture2D>& texture, const std::string_view text, ImVec2 size, bool bFillFrameDefault, float borderSize, float textHeightOffset, ImVec2 framePadding)
	{
		return ImageButtonWithText(texture->GetImage(), text, size, bFillFrameDefault, borderSize, textHeightOffset, framePadding);
	}

	bool ImageButtonWithTextHorizontal(const Ref<Texture2D>& image, const std::string_view text, ImVec2 size, float frameHeight, bool bFillFrameDefault)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const float prevFontScale = window->FontWindowScale;
		const float scale = size.x / 50.f;
		ImGui::SetWindowFontScale(scale);

		ImGuiContext& g = *GImGui;
		const ImVec2 padding = g.Style.FramePadding;
		const ImVec2 textSize = ImGui::CalcTextSize(text.data(), NULL, true);
		const float itemSpacingWidth = g.Style.ItemSpacing.x;
		const float itemSpacingHeight = g.Style.ItemSpacing.y;

		const float frameWidth = glm::max(1.f, ImGui::GetContentRegionAvail().x);
		ImVec2 frameSize = ImVec2(frameWidth, frameHeight);

		const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + frameSize);

		ImVec2 p = ImGui::GetCursorScreenPos();
		const bool bResult = ImGui::InvisibleButton(text.data(), frameSize);

		// Save the data to restore them at the end
		const ImVec2 prevLine = window->DC.CursorPosPrevLine;
		const ImVec2 curLine = window->DC.CursorPos;
		auto buttonItemData = g.LastItemData;

		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = bHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		auto& colors = ImGui::GetStyle().Colors;
		if (!bFillFrameDefault)
			ImGui::PushStyleColor(ImGuiCol_Button, colors[ImGuiCol_WindowBg]);

		const ImU32 col = ImGui::GetColorU32((bHeld && bHovered) ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));

		if (!bFillFrameDefault)
			ImGui::PopStyleColor();

		const ImVec2 imageEnd = ImVec2(p.x + size.x, p.y + size.y);
		UI::AddImage(image, p, imageEnd);

		// Centering text
		ImGui::SetCursorScreenPos(ImVec2(imageEnd.x + itemSpacingWidth, imageEnd.y - 0.5f * (bb.Max.y - bb.Min.y) - textSize.y * 0.5f));

		ImGui::Text(text.data());
		ImGui::SetWindowFontScale(prevFontScale);

		window->DC.CursorPos = curLine;
		window->DC.CursorPosPrevLine = prevLine;
		g.LastItemData = buttonItemData;

		return bResult;
	}
	
	ImTextureID GetTextureID(const Ref<Texture2D>& texture)
	{
		if (!texture || !texture->IsLoaded())
			return 0;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image)
				return 0;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const ImTextureRef textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			return textureID.GetTexID();
		}
		return 0;
	}
}
