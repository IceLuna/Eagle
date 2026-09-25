#include "egpch.h"
#include "SceneSequenceAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Asset/AssetManager.h"

#include "../EditorResources.h"

#include <implot.h>

namespace Eagle
{
	namespace
	{
		// Two keys closer than this are considered to sit at the same time
		static constexpr float s_TimeTolerance = 1e-4f;
		static constexpr float s_KeyHitRadius = 6.f;
		static constexpr float s_RulerHeight = 26.f;

		static constexpr ImU32 s_SelectedKeyColor = IM_COL32(255, 200, 60, 255);
		static constexpr ImU32 s_KeyOutlineColor = IM_COL32(20, 20, 20, 255);
		static constexpr ImU32 s_HoveredOutlineColor = IM_COL32(255, 255, 255, 255);
		static constexpr ImU32 s_PlayheadColor = IM_COL32(230, 60, 60, 255);
		static constexpr ImU32 s_SummaryKeyColor = IM_COL32(200, 200, 200, 255);
		static constexpr ImU32 s_LiveCameraColor = IM_COL32(90, 220, 110, 255);
		static constexpr ImU32 s_PostProcessRangeColor = IM_COL32(120, 160, 245, 255);

		// Scalar access used by the curve editor. Specialize to expose a new value type as curves.
		template <typename T>
		struct CurveAccess;

		// Step values aren't curves either
		template <>
		struct CurveAccess<bool>
		{
			static constexpr uint32_t Count = 0;
			static float Get(const bool&, uint32_t) { return 0.f; }
			static void Set(bool&, uint32_t, float) {}
			static const char* Name(uint32_t) { return ""; }
		};

		template <>
		struct CurveAccess<int32_t>
		{
			static constexpr uint32_t Count = 0;
			static float Get(const int32_t&, uint32_t) { return 0.f; }
			static void Set(int32_t&, uint32_t, float) {}
			static const char* Name(uint32_t) { return ""; }
		};

		template <>
		struct CurveAccess<uint32_t>
		{
			static constexpr uint32_t Count = 0;
			static float Get(const uint32_t&, uint32_t) { return 0.f; }
			static void Set(uint32_t&, uint32_t, float) {}
			static const char* Name(uint32_t) { return ""; }
		};

		template <>
		struct CurveAccess<float>
		{
			static constexpr uint32_t Count = 1;
			static float Get(const float& value, uint32_t) { return value; }
			static void Set(float& value, uint32_t, float x) { value = x; }
			static const char* Name(uint32_t) { return "Value"; }
		};

		template <>
		struct CurveAccess<glm::vec2>
		{
			static constexpr uint32_t Count = 2;
			static float Get(const glm::vec2& value, uint32_t c) { return value[c]; }
			static void Set(glm::vec2& value, uint32_t c, float x) { value[c] = x; }
			static const char* Name(uint32_t c)
			{
				static const char* names[] = { "X", "Y" };
				return names[c < 2 ? c : 0];
			}
		};

		template <>
		struct CurveAccess<glm::vec3>
		{
			static constexpr uint32_t Count = 3;
			static float Get(const glm::vec3& value, uint32_t c) { return value[c]; }
			static void Set(glm::vec3& value, uint32_t c, float x) { value[c] = x; }
			static const char* Name(uint32_t c)
			{
				static const char* names[] = { "X", "Y", "Z" };
				return names[c < 3 ? c : 0];
			}
		};

		// Rotations aren't shown as curves: per-component quaternion curves are meaningless to
		// edit by hand, and euler curves would lie about what slerp actually does in between
		template <>
		struct CurveAccess<glm::quat>
		{
			static constexpr uint32_t Count = 0;
			static float Get(const glm::quat&, uint32_t) { return 0.f; }
			static void Set(glm::quat&, uint32_t, float) {}
			static const char* Name(uint32_t) { return ""; }
		};

		template <>
		struct CurveAccess<GUID>
		{
			static constexpr uint32_t Count = 0;
			static float Get(const GUID&, uint32_t) { return 0.f; }
			static void Set(GUID&, uint32_t, float) {}
			static const char* Name(uint32_t) { return ""; }
		};

		template <>
		struct CurveAccess<std::string>
		{
			static constexpr uint32_t Count = 0;
			static float Get(const std::string&, uint32_t) { return 0.f; }
			static void Set(std::string&, uint32_t, float) {}
			static const char* Name(uint32_t) { return ""; }
		};

		static ImU32 WithAlpha(ImU32 color, float alpha)
		{
			ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
			c.w = alpha;
			return ImGui::ColorConvertFloat4ToU32(c);
		}

		// Constant = square, Linear = triangle, Smooth = circle, Cubic = diamond
		static void DrawKeyShape(ImDrawList* drawList, const ImVec2& c, float r, SequenceInterpolation interpolation, ImU32 fill, ImU32 outline)
		{
			switch (interpolation)
			{
				case SequenceInterpolation::Constant:
					drawList->AddRectFilled(ImVec2(c.x - r * 0.8f, c.y - r * 0.8f), ImVec2(c.x + r * 0.8f, c.y + r * 0.8f), fill);
					drawList->AddRect(ImVec2(c.x - r * 0.8f, c.y - r * 0.8f), ImVec2(c.x + r * 0.8f, c.y + r * 0.8f), outline);
					break;
				case SequenceInterpolation::Linear:
					drawList->AddTriangleFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y + r * 0.8f), ImVec2(c.x - r, c.y + r * 0.8f), fill);
					drawList->AddTriangle(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y + r * 0.8f), ImVec2(c.x - r, c.y + r * 0.8f), outline);
					break;
				case SequenceInterpolation::Smooth:
					drawList->AddCircleFilled(c, r * 0.85f, fill);
					drawList->AddCircle(c, r * 0.85f, outline);
					break;
				case SequenceInterpolation::Cubic:
				default:
					drawList->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), fill);
					drawList->AddQuad(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), outline);
					break;
			}
		}

		// ---- Value widgets used by the details panel (must be inside a property grid) ----
		static bool DrawValue(const GUID&, const char* label, bool& value)
		{
			return UI::Property(label, value);
		}

		static bool DrawValue(const GUID&, const char* label, int32_t& value)
		{
			return UI::PropertyDrag(label, value, 1.f);
		}

		static bool DrawValue(const GUID&, const char* label, uint32_t& value)
		{
			return UI::PropertyDrag(label, value, 1.f);
		}

		static bool DrawValue(const GUID&, const char* label, float& value)
		{
			return UI::PropertyDrag(label, value, 0.05f);
		}

		static bool DrawValue(const GUID&, const char* label, glm::vec2& value)
		{
			return UI::PropertyDrag(label, value, 0.05f);
		}

		static bool DrawValue(const GUID&, const char* label, glm::vec3& value)
		{
			return UI::PropertyDrag(label, value, 0.05f);
		}

		static bool DrawValue(const GUID&, const char* label, GUID& value)
		{
			UI::Text(label, std::to_string(value.GetHash()));
			return false;
		}

		static bool DrawValue(const GUID&, const char* label, std::string& value)
		{
			return UI::PropertyText(label, value);
		}

		static bool DrawValue(const GUID&, const char* label, glm::quat& value)
		{
			ImGui::Columns(1);
			const bool bChanged = UI::DrawQuatControl(std::string(label) + " (Quat)", value);
			ImGui::Columns(2);
			return bChanged;
		}

		// Draws text inside a slot of a fixed width, so that whatever follows on the line doesn't move
		// as the text changes (a frame counter gaining a digit, a camera name, ...).
		// Text that doesn't fit is cut short and the full version goes into the tooltip
		static void TextInSlot(const std::string& text, float slotWidth, const std::string_view tooltip = "")
		{
			const float startX = ImGui::GetCursorPosX();

			std::string shown = text;
			bool bTruncated = false;
			if (ImGui::CalcTextSize(shown.c_str()).x > slotWidth)
			{
				while (!shown.empty() && ImGui::CalcTextSize((shown + "...").c_str()).x > slotWidth)
					shown.pop_back();
				shown += "...";
				bTruncated = true;
			}

			ImGui::TextUnformatted(shown.c_str());
			if (bTruncated)
				UI::Tooltip(text);
			else if (!tooltip.empty())
				UI::Tooltip(tooltip);

			ImGui::SameLine(startX + slotWidth);
		}

		// Width of the widest text this slot will ever hold
		static float GetSlotWidth(const std::string& widestText, float padding = 10.f)
		{
			return ImGui::CalcTextSize(widestText.c_str()).x + padding;
		}

		// "Bloom" + "Intensity" -> "Bloom Intensity", but "Exposure" stays "Exposure"
		static std::string GetPostProcessPropertyLabel(const PostProcessPropertyInfo* info)
		{
			if (!info)
				return "Property";
			if (std::string_view(info->Category) == std::string_view(info->Name))
				return info->Name;
			return std::string(info->Category) + " " + info->Name;
		}

		// Properties of the same category share a color, so a track reads as groups
		static ImU32 GetPostProcessCategoryColor(const PostProcessPropertyInfo* info)
		{
			static constexpr ImU32 palette[] =
			{
				IM_COL32(235, 170, 80, 255),  IM_COL32(120, 205, 120, 255), IM_COL32(120, 165, 245, 255),
				IM_COL32(215, 120, 200, 255), IM_COL32(110, 210, 215, 255), IM_COL32(230, 120, 110, 255),
				IM_COL32(180, 190, 110, 255), IM_COL32(160, 140, 235, 255),
			};
			if (!info)
				return palette[0];

			const size_t hash = std::hash<std::string_view>{}(std::string_view(info->Category));
			return palette[hash % std::size(palette)];
		}
	}

	// Concrete channel view. One template covers every channel value type; the per-type bits live in
	// `SequenceValueTraits`, `CurveAccess` and the `DrawValue` overloads (editor).
	template <typename T>
	class TypedSequenceChannelView : public SequenceChannelView
	{
	public:
		using Access = CurveAccess<T>;
		using Traits = SequenceValueTraits<T>;

		TypedSequenceChannelView(const std::string& name, ImU32 color, SequenceChannel<T>* channel, const T& defaultValue)
			: SequenceChannelView(name, color), m_Channel(channel), m_DefaultValue(defaultValue) {}

		size_t GetKeysCount() const override { return m_Channel->GetKeysCount(); }
		GUID GetKeyID(size_t index) const override { return m_Channel->GetKey(index).ID; }
		float GetKeyTime(size_t index) const override { return m_Channel->GetKey(index).Time; }
		SequenceInterpolation GetKeyInterpolation(size_t index) const override { return m_Channel->GetKey(index).Interpolation; }
		bool FindKeyIndex(const GUID& id, size_t* outIndex) const override { return m_Channel->GetKeyIndex(id, outIndex); }

		void SetKeyTime(const GUID& id, float time) override
		{
			if (auto* key = m_Channel->FindKey(id))
				key->Time = glm::max(0.f, time);
		}

		void Sort() override { m_Channel->SortKeys(); }

		void SetKeyInterpolation(const GUID& id, SequenceInterpolation interpolation) override
		{
			size_t index = 0;
			if (!m_Channel->GetKeyIndex(id, &index))
				return;

			auto& key = m_Channel->GetKey(index);
			if (key.Interpolation == interpolation)
				return;

			if (interpolation == SequenceInterpolation::Cubic)
			{
				// Seed the tangents from the curve as it is right now, so converting a key to
				// Cubic doesn't visibly change anything until the user starts editing the handles
				const T inTangent = m_Channel->GetInTangent(index);
				const T outTangent = m_Channel->GetOutTangent(index);
				key.InTangent = inTangent;
				key.OutTangent = outTangent;
			}
			key.Interpolation = interpolation;
		}

		void FlattenTangents(const GUID& id) override
		{
			auto* key = m_Channel->FindKey(id);
			if (key && key->Interpolation == SequenceInterpolation::Cubic)
			{
				key->InTangent = Traits::Zero();
				key->OutTangent = Traits::Zero();
			}
		}

		bool RemoveKey(const GUID& id) override { return m_Channel->RemoveKey(id); }

		GUID AddKeyAtTime(float time) override
		{
			time = glm::max(0.f, time);
			const T value = m_Channel->Evaluate(time, m_DefaultValue);

			// Inherit the mode of the key to the left, so inserting a key mid-curve doesn't
			// change the character of the segment it lands in
			SequenceInterpolation interpolation = SequenceInterpolation::Smooth;
			for (const auto& key : m_Channel->GetKeys())
			{
				if (key.Time <= time)
					interpolation = key.Interpolation;
				else
					break;
			}

			const size_t index = m_Channel->AddKey(time, value, interpolation);
			return m_Channel->GetKey(index).ID;
		}

		std::any CopyKey(const GUID& id) const override
		{
			if (const auto* key = m_Channel->FindKey(id))
				return std::any(*key);
			return {};
		}

		GUID PasteKey(const std::any& data, float time) override
		{
			const SequenceKey<T>* source = std::any_cast<SequenceKey<T>>(&data);
			if (!source)
				return GUID(0, 0);

			SequenceKey<T> pasted = *source;
			pasted.ID = GUID();
			pasted.Time = glm::max(0.f, time);

			// Pasting onto an existing key replaces it rather than stacking two keys at one time
			if (auto* existing = m_Channel->FindKeyAtTime(pasted.Time, s_TimeTolerance))
			{
				const GUID existingID = existing->ID;
				*existing = pasted;
				existing->ID = existingID;
				return existingID;
			}

			const size_t index = m_Channel->InsertKey(pasted);
			return m_Channel->GetKey(index).ID;
		}

		bool DrawKeyValue(const GUID& id) override
		{
			auto* key = m_Channel->FindKey(id);
			if (!key)
				return false;

			bool bChanged = DrawValue(id, "Value", key->Value);

			if constexpr (Access::Count > 0)
			{
				if (key->Interpolation == SequenceInterpolation::Cubic)
				{
					bChanged |= DrawValue(id, "In Tangent", key->InTangent);
					bChanged |= DrawValue(id, "Out Tangent", key->OutTangent);
				}
			}
			return bChanged;
		}

		uint32_t GetCurveComponentsCount() const override { return Access::Count; }
		const char* GetCurveComponentName(uint32_t component) const override { return Access::Name(component); }

		float EvaluateComponent(float time, uint32_t component) const override
		{
			return Access::Get(m_Channel->Evaluate(time, m_DefaultValue), component);
		}

		float GetKeyComponent(size_t index, uint32_t component) const override
		{
			return Access::Get(m_Channel->GetKey(index).Value, component);
		}

		void SetKeyComponent(const GUID& id, uint32_t component, float value) override
		{
			if (auto* key = m_Channel->FindKey(id))
				Access::Set(key->Value, component, value);
		}

		float GetKeyTangent(size_t index, uint32_t component, bool bOut) const override
		{
			return Access::Get(bOut ? m_Channel->GetOutTangent(index) : m_Channel->GetInTangent(index), component);
		}

		void SetKeyTangent(const GUID& id, uint32_t component, bool bOut, float value) override
		{
			size_t index = 0;
			if (!m_Channel->GetKeyIndex(id, &index))
				return;

			SetKeyInterpolation(id, SequenceInterpolation::Cubic);
			auto& key = m_Channel->GetKey(index);
			Access::Set(bOut ? key.OutTangent : key.InTangent, component, value);
		}

	protected:
		SequenceChannel<T>* m_Channel; // Owned by the track
		T m_DefaultValue;
	};

	// Keys of a Post Process track. One view per animated rendering property; the registry entry
	// supplies the label, the drag speed and the limits
	template <typename T>
	class PostProcessChannelView : public TypedSequenceChannelView<T>
	{
	public:
		PostProcessChannelView(const std::string& name, ImU32 color, SequenceChannel<T>* channel, const T& defaultValue,
			const PostProcessPropertyInfo* info, bool* enabledFlag)
			: TypedSequenceChannelView<T>(name, color, channel, defaultValue), m_Info(info), m_EnabledFlag(enabledFlag) {}

		bool CanBeDisabled() const override { return true; }
		bool IsEnabled() const override { return !m_EnabledFlag || *m_EnabledFlag; }
		void SetEnabled(bool bValue) override { if (m_EnabledFlag) *m_EnabledFlag = bValue; }

		// Integer types and asset references switch at the key rather than blending into it
		bool IsInterpolationEditable() const override
		{
			return !std::is_same_v<T, bool> && !std::is_same_v<T, int32_t> && !std::is_same_v<T, uint32_t> && !std::is_same_v<T, GUID>;
		}

		void SetKeyInterpolation(const GUID& id, SequenceInterpolation interpolation) override
		{
			if (IsInterpolationEditable())
				TypedSequenceChannelView<T>::SetKeyInterpolation(id, interpolation);
		}

		// Label displayed next to a key on the graph
		std::string GetKeyLabel(size_t index) const override
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				return this->m_Channel->GetKey(index).Value ? "On" : "Off";
			}
			else if constexpr (std::is_same_v<T, int32_t>)
			{
				return IsEnum() ? GetEnumValueName(this->m_Channel->GetKey(index).Value) : "";
			}
			else if constexpr (std::is_same_v<T, uint32_t>)
			{
				return "";
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return "";
			}
			else if constexpr (std::is_same_v<T, glm::vec2>)
			{
				return "";
			}
			else if constexpr (std::is_same_v<T, glm::vec3>)
			{
				return "";
			}
			else if constexpr (std::is_same_v<T, GUID>)
			{
				return GetAssetName(this->m_Channel->GetKey(index).Value);
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				return this->m_Channel->GetKey(index).Value;
			}
			else
			{
				static_assert(false);
				return {};
			}
		}

		bool DrawKeyValue(const GUID& id) override
		{
			auto* key = this->m_Channel->FindKey(id);
			if (!key || !m_Info)
				return TypedSequenceChannelView<T>::DrawKeyValue(id);

			bool bChanged = false;
			if constexpr (std::is_same_v<T, bool>)
			{
				bChanged = UI::Property(m_Info->Name, key->Value, m_Info->Tooltip);
			}
			else if constexpr (std::is_same_v<T, int32_t>)
			{
				if (m_Info->Type == PostProcessValueType::Enum)
				{
					// Enum property (currently only the fog equation)
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
					ImGui::TextUnformatted(m_Info->Name.c_str());
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo("##PostProcessEnum", GetEnumValueName(key->Value).c_str()))
					{
						if (m_Info->EnumType == PostProcessEnumType::FogEquation)
						{
							for (const auto& [value, name] : magic_enum::enum_entries<FogEquation>())
							{
								if (ImGui::Selectable(std::string(name).c_str(), int32_t(value) == key->Value))
								{
									key->Value = int32_t(value);
									bChanged = true;
								}
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
					ImGui::NextColumn();
				}
				else
				{
					bChanged = UI::PropertyDrag(m_Info->Name, key->Value, m_Info->DragSpeed, int32_t(m_Info->Min), int32_t(m_Info->Max), m_Info->Tooltip);
				}
			}
			else if constexpr (std::is_same_v<T, uint32_t>)
			{
				bChanged = UI::PropertyDrag(m_Info->Name, key->Value, m_Info->DragSpeed, uint32_t(m_Info->Min), uint32_t(m_Info->Max), m_Info->Tooltip);
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				bChanged = UI::PropertyDrag(m_Info->Name, key->Value, m_Info->DragSpeed, m_Info->Min, m_Info->Max, m_Info->Tooltip);
			}
			else if constexpr (std::is_same_v<T, glm::vec2>)
			{
				glm::vec3 asVec3 = glm::vec3(key->Value, 0.f);
				if (UI::PropertyDrag(m_Info->Name, asVec3, m_Info->DragSpeed, m_Info->Min, m_Info->Max, m_Info->Tooltip))
				{
					key->Value = glm::vec2(asVec3);
					bChanged = true;
				}
			}
			else if constexpr (std::is_same_v<T, glm::vec3>)
			{
				const bool bColor = m_Info->Type == PostProcessValueType::Color3;
				bChanged = bColor ? UI::PropertyColor(m_Info->Name, key->Value)
					: UI::PropertyDrag(m_Info->Name, key->Value, m_Info->DragSpeed, m_Info->Min, m_Info->Max, m_Info->Tooltip);
			}
			else if constexpr (std::is_same_v<T, GUID>)
			{
				Ref<AssetTexture2D> texture;
				if (!key->Value.IsNull())
				{
					Ref<Asset> asset;
					if (AssetManager::Get(key->Value, &asset))
						texture = Cast<AssetTexture2D>(asset);
				}

				if (EditorResources::DrawAssetSelection(m_Info->Name, texture, m_Info->Tooltip))
				{
					key->Value = texture ? texture->GetGUID() : GUID(0, 0);
					bChanged = true;
				}
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				bChanged = UI::PropertyText(m_Info->Name, key->Value, m_Info->Tooltip);
			}
			else
			{
				static_assert(false, "Unknown property type");
			}

			if constexpr (CurveAccess<T>::Count > 0)
			{
				const bool bDisable = key->Interpolation != SequenceInterpolation::Cubic;
				if (bDisable)
					UI::PushItemDisabled();
				
				UI::TextWithSeparator("Tangents", 2.5f, "Used only for Cubic interpolation");
				bChanged |= DrawValue(id, "In Tangent", key->InTangent);
				bChanged |= DrawValue(id, "Out Tangent", key->OutTangent);

				if (bDisable)
					UI::PopItemDisabled();
			}

			return bChanged;
		}

	private:
		// File name of the asset a key points at, for the label next to the key
		static std::string GetAssetName(const GUID& assetID)
		{
			if (assetID.IsNull())
				return "None";

			Ref<Asset> asset;
			if (AssetManager::Get(assetID, &asset))
				return Utils::AsString(asset->GetPath().stem());
			return "(missing asset)";
		}

		bool IsEnum() const
		{
			return m_Info && m_Info->EnumType != PostProcessEnumType::None;
		}

		std::string GetEnumValueName(int32_t value) const
		{
			if (IsEnum())
			{
				switch (m_Info->EnumType)
				{
					case PostProcessEnumType::FogEquation:
						return Utils::GetEnumName(FogEquation(value));
					default:
						EG_CORE_ASSERT(false);
						return "<Unknown>";
				}
			}
			return std::to_string(value);
		}

	private:
		const PostProcessPropertyInfo* m_Info = nullptr;
		bool* m_EnabledFlag = nullptr; // Points at the track's channel, which outlives the view
	};

	// Keys of a Camera Cuts track: each one names the camera that goes live at its time
	class CameraCutChannelView : public TypedSequenceChannelView<GUID>
	{
	public:
		CameraCutChannelView(SequenceGUIDChannel* channel, const AssetSceneSequence* asset)
			: TypedSequenceChannelView<GUID>("Cuts", IM_COL32(235, 200, 80, 255), channel, GUID(0, 0)), m_Asset(asset) {}

		// Cuts are always instant
		void SetKeyInterpolation(const GUID&, SequenceInterpolation) override {}
		void SetKeyTangent(const GUID&, uint32_t, bool, float) override {}
		bool IsInterpolationEditable() const override { return false; }

		GUID AddKeyAtTime(float time) override
		{
			// A new cut is almost always there to switch cameras, so point it at the camera after the live one.
			// It can be changed in the Details panel
			const size_t index = m_Channel->AddKey(glm::max(0.f, time), PickNextCamera(time), SequenceInterpolation::Constant);
			return m_Channel->GetKey(index).ID;
		}

		GUID PasteKey(const std::any& data, float time) override
		{
			const GUID id = TypedSequenceChannelView<GUID>::PasteKey(data, time);
			if (auto* key = m_Channel->FindKey(id))
				key->Interpolation = SequenceInterpolation::Constant;
			return id;
		}

		std::string GetKeyLabel(size_t index) const override
		{
			return GetCameraName(m_Channel->GetKey(index).Value);
		}

		bool DrawKeyValue(const GUID& id) override
		{
			auto* key = m_Channel->FindKey(id);
			if (!key)
				return false;

			bool bChanged = false;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::Text("Camera");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			if (ImGui::BeginCombo("##CutCamera", GetCameraName(key->Value).c_str()))
			{
				for (const auto& track : m_Asset->GetCameraTracks())
				{
					if (!track)
						continue;

					EG_CORE_ASSERT(track->GetType() == SequenceTrackType::Camera);

					ImGui::PushID((void*)track->GetID().GetHash());
					if (ImGui::Selectable(track->GetName().c_str(), track->GetID() == key->Value))
					{
						key->Value = track->GetID();
						bChanged = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
			ImGui::NextColumn();
			return bChanged;
		}

	private:
		std::string GetCameraName(const GUID& cameraID) const
		{
			for (const auto& track : m_Asset->GetCameraTracks())
				if (track && track->GetID() == cameraID)
					return track->GetName();
			return "(missing camera)";
		}

		GUID PickNextCamera(float time) const
		{
			std::vector<GUID> cameras;
			for (const auto& track : m_Asset->GetCameraTracks())
				if (track && track->IsEnabled())
					cameras.push_back(track->GetID());

			if (cameras.empty())
				return GUID(0, 0);

			if (const SequenceCameraTrack* live = m_Asset->ResolveActiveCamera(time))
			{
				for (size_t i = 0; i < cameras.size(); ++i)
					if (cameras[i] == live->GetID())
						return cameras[(i + 1) % cameras.size()];
			}
			return cameras[0];
		}

	private:
		const AssetSceneSequence* m_Asset;
	};

	// Keys of a Event track: each one represents a name of an event to trigger
	class EventChannelView : public TypedSequenceChannelView<std::string>
	{
	public:
		EventChannelView(SequenceStringChannel* channel)
			: TypedSequenceChannelView<std::string>("Events", IM_COL32(235, 200, 80, 255), channel, "") {}

		// Cuts are always instant
		void SetKeyInterpolation(const GUID&, SequenceInterpolation) override {}
		void SetKeyTangent(const GUID&, uint32_t, bool, float) override {}
		bool IsInterpolationEditable() const override { return false; }

		GUID PasteKey(const std::any& data, float time) override
		{
			const GUID id = TypedSequenceChannelView<std::string>::PasteKey(data, time);
			if (auto* key = m_Channel->FindKey(id))
				key->Interpolation = SequenceInterpolation::Constant;
			return id;
		}

		bool DrawKeyValue(const GUID& id) override
		{
			auto* key = m_Channel->FindKey(id);
			if (!key)
				return false;

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::Text("Event Name");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);

			ImGui::PushID(key);
			const bool bChanged = UI::InputText("", key->Value);
			ImGui::PopID();

			ImGui::PopItemWidth();
			ImGui::NextColumn();
			return bChanged;
		}
	};

	// ------------------------------------------------------------------------------------------------
	SceneSequenceAssetEditor::SceneSequenceAssetEditor(const Ref<AssetSceneSequence>& asset)
		: m_Asset(asset)
	{
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
		m_Player.SetAsset(m_Asset);

		// New sequences open with every track expanded
		for (const auto& track : m_Asset->GetTracks())
		{
			if (track)
				m_ExpandedTracks.push_back(track->GetID());
		}

		if (!m_Asset->GetTracks().empty() && m_Asset->GetTracks()[0])
			m_SelectedTrackID = m_Asset->GetTracks()[0]->GetID();
	}

	SceneSequenceAssetEditor::~SceneSequenceAssetEditor()
	{
		if (auto scene = m_PreviewScene.lock())
			SceneSequencePlayer::ReleaseScene(scene.get(), m_PreviewOwnerID);
	}

	void SceneSequenceAssetEditor::BuildChannelViews()
	{
		m_TrackViews.clear();
		m_TrackViews.reserve(m_Asset->GetTracks().size());

		for (const auto& track : m_Asset->GetTracks())
		{
			if (!track)
				continue;

			TrackViews& views = m_TrackViews.emplace_back();
			views.Track = track;

			// Every new track type adds its channels here.
			// The channel index (position in this list) is what `KeyRef::ChannelIndex` refers to,
			// so keep the order stable for a given track type.
			switch (track->GetType())
			{
				case SequenceTrackType::Camera:
				{
					SequenceCameraTrack* camera = (SequenceCameraTrack*)track.get();
					views.Channels.push_back(MakeScope<TypedSequenceChannelView<glm::vec3>>("Location", IM_COL32(235, 120, 90, 255),
						&camera->GetLocationChannel(), glm::vec3(0.f)));
					views.Channels.push_back(MakeScope<TypedSequenceChannelView<glm::quat>>("Rotation", IM_COL32(120, 205, 120, 255),
						&camera->GetRotationChannel(), glm::quat(1.f, 0.f, 0.f, 0.f)));
					views.Channels.push_back(MakeScope<TypedSequenceChannelView<float>>("Field Of View", IM_COL32(120, 165, 245, 255),
						&camera->GetFOVChannel(), camera->GetDefaultFOVDegrees()));
					break;
				}
				case SequenceTrackType::CameraCuts:
				{
					SequenceCameraCutTrack* cuts = (SequenceCameraCutTrack*)track.get();
					views.Channels.push_back(MakeScope<CameraCutChannelView>(&cuts->GetCutsChannel(), m_Asset.get()));
					break;
				}
				case SequenceTrackType::PostProcess:
				{
					SequencePostProcessTrack* postProcess = (SequencePostProcessTrack*)track.get();
					for (auto& channel : postProcess->GetChannels())
					{
						const PostProcessPropertyInfo* info = FindPostProcessProperty(channel.Property);
						const std::string name = GetPostProcessPropertyLabel(info);
						const ImU32 color = GetPostProcessCategoryColor(info);

						// One view per property. Which channel type it is was decided when the property was added
						bool* enabledFlag = &channel.bEnabled;
						std::visit([&views, &name, color, info, enabledFlag](auto& data)
						{
							using ValueType = std::decay_t<decltype(data.GetKey(0).Value)>;
							views.Channels.push_back(MakeScope<PostProcessChannelView<ValueType>>(name, color, &data, ValueType{}, info, enabledFlag));
						}, channel.Data);
					}
					break;
				}
				case SequenceTrackType::Event:
				{
					SequenceEventTrack* events = (SequenceEventTrack*)track.get();
					views.Channels.push_back(MakeScope<EventChannelView>(&events->GetEventsChannel()));
					break;
				}
			}
		}
	}

	void SceneSequenceAssetEditor::PruneSelection()
	{
		m_Selection.erase(std::remove_if(m_Selection.begin(), m_Selection.end(), [this](const KeyRef& ref)
		{
			SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
			return !view || !view->FindKeyIndex(ref.KeyID, nullptr);
		}), m_Selection.end());

		if (!m_SelectedTrackID.IsNull() && FindTrackIndex(m_SelectedTrackID) < 0)
			m_SelectedTrackID = GUID(0, 0);

		if (!m_RenamingTrackID.IsNull() && FindTrackIndex(m_RenamingTrackID) < 0)
			m_RenamingTrackID = GUID(0, 0);
	}

	// ---- Lookups / selection ----
	SequenceChannelView* SceneSequenceAssetEditor::FindView(const GUID& trackID, uint32_t channelIndex) const
	{
		for (const auto& views : m_TrackViews)
		{
			if (views.Track->GetID() == trackID)
				return channelIndex < views.Channels.size() ? views.Channels[channelIndex].get() : nullptr;
		}
		return nullptr;
	}

	int32_t SceneSequenceAssetEditor::FindTrackIndex(const GUID& trackID) const
	{
		for (size_t i = 0; i < m_TrackViews.size(); ++i)
			if (m_TrackViews[i].Track->GetID() == trackID)
				return int32_t(i);
		return -1;
	}

	Ref<SequenceCameraTrack> SceneSequenceAssetEditor::GetTargetCameraTrack() const
	{
		// Prefer the selected track, then fall back to the first camera track
		if (const int32_t index = FindTrackIndex(m_SelectedTrackID); index >= 0)
		{
			const auto& track = m_TrackViews[index].Track;
			if (track->GetType() == SequenceTrackType::Camera)
				return Cast<SequenceCameraTrack>(track);
		}

		for (const auto& views : m_TrackViews)
			if (views.Track->GetType() == SequenceTrackType::Camera)
				return Cast<SequenceCameraTrack>(views.Track);

		return {};
	}

	float SceneSequenceAssetEditor::GetLastKeyTime() const
	{
		const auto& tracks = m_Asset->GetTracks();
		float result = 0.f;
		for (const auto& track : tracks)
			result = glm::max(result, track->GetLastKeyTime());
		return result;
	}

	bool SceneSequenceAssetEditor::IsSelected(const KeyRef& ref) const
	{
		return std::find(m_Selection.begin(), m_Selection.end(), ref) != m_Selection.end();
	}

	void SceneSequenceAssetEditor::Select(const KeyRef& ref, bool bAdditive)
	{
		if (!bAdditive)
			m_Selection.clear();

		if (!IsSelected(ref))
			m_Selection.push_back(ref);
	}

	void SceneSequenceAssetEditor::Deselect(const KeyRef& ref)
	{
		std::erase(m_Selection, ref);
	}

	void SceneSequenceAssetEditor::GatherTrackKeysAtTime(uint32_t trackIndex, float time, std::vector<KeyRef>& outRefs) const
	{
		if (trackIndex >= m_TrackViews.size())
			return;

		const auto& views = m_TrackViews[trackIndex];
		for (uint32_t ci = 0; ci < uint32_t(views.Channels.size()); ++ci)
		{
			const auto& view = views.Channels[ci];
			for (size_t i = 0; i < view->GetKeysCount(); ++i)
			{
				if (glm::abs(view->GetKeyTime(i) - time) <= s_TimeTolerance)
					outRefs.push_back({ views.Track->GetID(), ci, view->GetKeyID(i) });
			}
		}
	}

	bool SceneSequenceAssetEditor::IsTrackExpanded(const GUID& trackID) const
	{
		return std::find(m_ExpandedTracks.begin(), m_ExpandedTracks.end(), trackID) != m_ExpandedTracks.end();
	}

	void SceneSequenceAssetEditor::SetTrackExpanded(const GUID& trackID, bool bExpanded)
	{
		const bool bCurrently = IsTrackExpanded(trackID);
		if (bExpanded && !bCurrently)
			m_ExpandedTracks.push_back(trackID);
		else if (!bExpanded && bCurrently)
			std::erase(m_ExpandedTracks, trackID);
	}

	// ---- Helpers ----
	float SceneSequenceAssetEditor::GetFrameDuration() const
	{
		return 1.f / glm::max(1.f, m_Asset->GetFrameRate());
	}

	float SceneSequenceAssetEditor::SnapTime(float time) const
	{
		// Holding Alt temporarily disables snapping
		if (!bSnapToFrames || ImGui::GetIO().KeyAlt)
			return time;

		const float frame = GetFrameDuration();
		return glm::round(time / frame) * frame;
	}

	std::string SceneSequenceAssetEditor::FormatTime(float time) const
	{
		char buffer[32];
		if (bShowFrames)
			snprintf(buffer, sizeof(buffer), "%d", int(glm::round(time * m_Asset->GetFrameRate())));
		else
			snprintf(buffer, sizeof(buffer), "%.2fs", time);
		return buffer;
	}

	void SceneSequenceAssetEditor::SetPlayheadTime(float time)
	{
		m_Player.SetTime(glm::clamp(time, 0.f, m_Asset->GetDuration()));

		// While piloting, the viewport camera rides along with the scrubbed playhead
		if (bPiloting && !m_Player.IsPlaying())
			SnapPilotCameraToPlayhead();
	}

	void SceneSequenceAssetEditor::MarkDirty()
	{
		m_Asset->SetDirty(true);
	}

	Ref<Scene> SceneSequenceAssetEditor::GetEditorScene() const
	{
		return Scene::GetCurrentScene();
	}

	Transform SceneSequenceAssetEditor::GetPreviewBase() const
	{
		if (m_PreviewContextEntity.IsNull())
			return Transform{};

		Ref<Scene> scene = GetEditorScene();
		if (!scene)
			return Transform{};

		Entity entity = scene->GetEntityByGUID(m_PreviewContextEntity);
		if (!entity || !entity.HasComponent<SceneSequenceComponent>())
			return Transform{};

		const auto& component = entity.GetComponent<SceneSequenceComponent>();
		return component.bPlayInWorldSpace ? Transform{} : component.GetWorldTransform();
	}

	void SceneSequenceAssetEditor::ComputeTickSteps(float* outMajor, float* outMinor) const
	{
		constexpr float minLabelSpacing = 70.f; // px between labelled ticks

		float major = 1.f;
		if (bShowFrames)
		{
			static constexpr float frameSteps[] = { 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600 };
			const float frame = GetFrameDuration();
			major = frameSteps[std::size(frameSteps) - 1] * frame;
			for (float steps : frameSteps)
			{
				if (steps * frame * m_PixelsPerSecond >= minLabelSpacing)
				{
					major = steps * frame;
					break;
				}
			}
		}
		else
		{
			static constexpr float timeSteps[] = { 0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.25f, 0.5f, 1.f, 2.f, 5.f, 10.f, 15.f, 30.f, 60.f, 120.f, 300.f };
			major = timeSteps[std::size(timeSteps) - 1];
			for (float step : timeSteps)
			{
				if (step * m_PixelsPerSecond >= minLabelSpacing)
				{
					major = step;
					break;
				}
			}
		}

		*outMajor = major;
		*outMinor = major / 5.f;
	}

	void SceneSequenceAssetEditor::FitViewToContent()
	{
		const float end = glm::max(0.1f, glm::max(m_Asset->GetDuration(), GetLastKeyTime()));
		constexpr float padding = 30.f;
		m_PixelsPerSecond = glm::clamp((m_TimelineWidth - padding * 2.f) / end, 5.f, 5000.f);
		m_ViewStart = -padding / m_PixelsPerSecond;
	}

	// ---- Frame ----
	void SceneSequenceAssetEditor::OnImGuiRender(bool* pOpen)
	{
		BuildChannelViews();
		PruneSelection();

		m_Player.SetLoopOverride(bLoopPlayback);
		m_Player.Update(Application::Get().GetTimestep());

		// Previewing doesn't run scripts, so events are logged instead
		for (const auto& event : m_Player.GetFiredEvents())
			EG_CORE_TRACE("[Scene Sequence] Event '{}' at {:.3f}s", event.Name, event.Time);

		// Piloting pauses while the sequence plays (you watch the result), and picks back up
		// from wherever the playhead stopped
		const bool bPlaying = m_Player.IsPlaying();
		if (bPiloting && bWasPlaying && !bPlaying)
			SnapPilotCameraToPlayhead();
		bWasPlaying = bPlaying;

		// Auto-key: while piloting, write the camera into the key at the playhead whenever it moves
		if (bPiloting && bAutoKey && !bPlaying)
		{
			if (Ref<Scene> scene = GetEditorScene())
			{
				const Transform& current = scene->EditorCamera.GetTransform();
				const bool bMoved = glm::distance(current.Location, m_LastPilotTransform.Location) > 1e-4f
					|| glm::abs(glm::dot(current.Rotation.GetQuat(), m_LastPilotTransform.Rotation.GetQuat())) < 0.999999f;
				if (bMoved)
				{
					KeyCameraFromViewport(GetTargetCameraTrack(), SnapTime(m_Player.GetTime()));
					m_LastPilotTransform = current;
				}
			}
		}

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		const bool bVisible = ImGui::Begin(m_WindowName.c_str(), pOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		if (bVisible)
		{
			DrawToolbar();
			ImGui::Separator();

			// Layout: Tracks/Curves on the left, Details on the right, with a draggable splitter between.
			// Curves share the left region with the tracks because both want the full width and height;
			// Details is a narrow column of properties and stays put whichever of the two is shown
			const ImVec2 available = ImGui::GetContentRegionAvail();
			constexpr float splitterWidth = 6.f;
			constexpr float minSidePanelWidth = 220.f;
			constexpr float minTimelineWidth = 300.f;
			const float maxSidePanelWidth = glm::max(minSidePanelWidth, available.x - minTimelineWidth - splitterWidth);
			m_SidePanelWidth = glm::clamp(m_SidePanelWidth, minSidePanelWidth, maxSidePanelWidth);
			const float timelineWidth = glm::max(1.f, available.x - m_SidePanelWidth - splitterWidth);
			const float regionHeight = glm::max(1.f, available.y);

			if (ImGui::BeginChild("##TimelineRegion", ImVec2(timelineWidth, regionHeight), false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				DrawTracksAndCurves();
			}
			ImGui::EndChild();

			ImGui::SameLine(0.f, 0.f);
			ImGui::InvisibleButton("##SidePanelSplitter", ImVec2(splitterWidth, regionHeight));
			{
				const bool bActive = ImGui::IsItemActive();
				const bool bHovered = ImGui::IsItemHovered();
				if (bActive || bHovered)
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
				if (bActive)
					m_SidePanelWidth -= ImGui::GetIO().MouseDelta.x; // Dragging left widens the panel

				const ImVec2 min = ImGui::GetItemRectMin();
				const ImVec2 max = ImGui::GetItemRectMax();
				const float x = (min.x + max.x) * 0.5f;
				ImGui::GetWindowDrawList()->AddLine(ImVec2(x, min.y), ImVec2(x, max.y),
					ImGui::GetColorU32((bActive || bHovered) ? ImGuiCol_SeparatorActive : ImGuiCol_Separator), 2.f);
			}

			ImGui::SameLine(0.f, 0.f);
			if (ImGui::BeginChild("##SidePanel", ImVec2(0.f, regionHeight)))
			{
				UI::TextWithSeparator("Details");
				if (ImGui::BeginChild("##SequenceDetails"))
					DrawDetailsTab();
				ImGui::EndChild();
			}
			ImGui::EndChild();

			HandleShortcuts();
		}
		ImGui::End();

		// Structural changes to a track's properties are applied here, where nothing holds
		// a pointer into the channel vector any more
		ApplyPendingPropertyOps();

		const bool bClosing = pOpen && !(*pOpen);
		UpdatePreview(bVisible && !bClosing);
	}

	void SceneSequenceAssetEditor::UpdatePreview(bool bActive)
	{
		Ref<Scene> scene = GetEditorScene();

		// The level changed (another scene opened, simulation started/stopped...): hand the old one back
		if (auto previous = m_PreviewScene.lock(); previous && previous != scene)
		{
			SceneSequencePlayer::ReleaseScene(previous.get(), m_PreviewOwnerID);
			m_PreviewScene.reset();
		}

		// Don't fight gameplay: a running game owns its camera (and may be playing this very sequence)
		const bool bSceneIsPlaying = scene && scene->IsPlaying();
		const bool bShouldPreview = bActive && bPreviewInViewport && scene && !bSceneIsPlaying;

		// While piloting, the viewport camera belongs to the user, but the grade still previews
		const bool bShouldDriveCamera = bShouldPreview && (!bPiloting || m_Player.IsPlaying());

		if (bShouldPreview)
		{
			m_Player.ApplyToScene(scene.get(), m_PreviewOwnerID, GetPreviewBase(), bShouldDriveCamera);
			m_PreviewScene = scene;
		}
		else if (auto previous = m_PreviewScene.lock())
		{
			SceneSequencePlayer::ReleaseScene(previous.get(), m_PreviewOwnerID);
			m_PreviewScene.reset();
		}

		// A sequence without a camera track still previews its rendering settings, so this is
		// about what the viewport looks through, not about whether anything is being previewed
		const bool bDrivingCamera = bShouldPreview && scene->IsCameraOverridden()
			&& scene->GetCameraOverrideOwner() == m_PreviewOwnerID;

		// Path visualization. Skipped while looking through the sequence camera, otherwise the
		// path would be drawn straight across the screen
		if (bActive && scene && !bSceneIsPlaying && !bDrivingCamera)
		{
			GUID highlight = GUID(0, 0);
			for (const auto& ref : m_Selection)
			{
				const int32_t trackIndex = FindTrackIndex(ref.TrackID);
				if (trackIndex >= 0 && m_TrackViews[trackIndex].Track->GetType() == SequenceTrackType::Camera && ref.ChannelIndex == 0)
				{
					highlight = ref.KeyID;
					break;
				}
			}
			scene->DrawSequencePath(m_Asset, GetPreviewBase(), m_Player.GetTime(), highlight);
		}
	}

	void SceneSequenceAssetEditor::DrawToolbar()
	{
		const float frame = GetFrameDuration();
		const float duration = m_Asset->GetDuration();
		const float time = m_Player.GetTime();

		if (ImGui::Button("|<"))
			SetPlayheadTime(0.f);
		UI::Tooltip("Go to start (Home)");

		ImGui::SameLine();
		if (ImGui::Button("<"))
			SetPlayheadTime(time - frame);
		UI::Tooltip("Previous frame (Left arrow)");

		ImGui::SameLine();
		if (ImGui::Button(m_Player.IsPlaying() ? "Pause" : "Play", ImVec2(52.f, 0.f)))
		{
			if (m_Player.IsPlaying())
				m_Player.Pause();
			else
				m_Player.Play();
		}
		UI::Tooltip("Play/Pause (Space)");

		ImGui::SameLine();
		if (ImGui::Button(">"))
			SetPlayheadTime(time + frame);
		UI::Tooltip("Next frame (Right arrow)");

		ImGui::SameLine();
		if (ImGui::Button(">|"))
			SetPlayheadTime(duration);
		UI::Tooltip("Go to end (End)");

		ImGui::SameLine();
		if (ImGui::Button("Stop"))
			m_Player.Stop();

		ImGui::SameLine();
		ImGui::Checkbox("Loop Preview", &bLoopPlayback);
		UI::Tooltip("Loop playback in the editor only. Doesn't change the asset; see 'Looping' for that");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.f);
		float playRate = m_Player.GetPlayRate();
		if (ImGui::DragFloat("##PlayRate", &playRate, 0.05f, -10.f, 10.f, "%.2fx"))
			m_Player.SetPlayRate(glm::clamp(playRate, -10.f, 10.f));
		UI::Tooltip("Play rate. 1 is normal speed, negative plays the sequence backwards.\n"
			"Editor preview only; components have their own play rate");

		ImGui::SameLine();
		if (ImGui::Button("1x"))
			m_Player.SetPlayRate(1.f);
		UI::Tooltip("Reset the play rate");

		// Current time / duration
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.f);
		float editedTime = time;
		if (ImGui::DragFloat("##PlayheadTime", &editedTime, 0.01f, 0.f, duration, "%.3f s"))
			SetPlayheadTime(editedTime);
		UI::Tooltip("Current time");

		ImGui::SameLine();
		ImGui::TextUnformatted("/");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.f);
		float editedDuration = duration;
		if (ImGui::DragFloat("##Duration", &editedDuration, 0.05f, frame, 3600.f, "%.3f s"))
			m_Asset->SetDuration(glm::clamp(editedDuration, frame, 3600.f));
		UI::Tooltip("Duration: length of the sequence in seconds. Can also be changed by dragging the red marker in the ruler");

		// Fixed-width slot: the counter must not push everything right as the frame number grows
		ImGui::SameLine();
		{
			const int totalFrames = int(glm::round(m_Asset->GetDuration() / frame));
			const int currentFrame = int(glm::round(time / frame));
			const std::string widest = "Frame " + std::to_string(totalFrames) + " / " + std::to_string(totalFrames);
			TextInSlot("Frame " + std::to_string(currentFrame) + " / " + std::to_string(totalFrames), GetSlotWidth(widest));
		}

		// No SameLine here: `TextInSlot` already placed the cursor at the end of its slot
		if (ImGui::Button("Fit To Keys"))
		{
			const float lastKey = GetLastKeyTime();
			if (lastKey > 0.f)
				m_Asset->SetDuration(lastKey);
		}
		UI::Tooltip("Set the duration to the time of the last key");

		// Sequence settings
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f);
		float frameRate = m_Asset->GetFrameRate();
		if (ImGui::DragFloat("##FrameRate", &frameRate, 1.f, 1.f, 240.f, "%.0f fps"))
			m_Asset->SetFrameRate(glm::clamp(frameRate, 1.f, 240.f));
		UI::Tooltip("Frame rate. Used for snapping and the frame counter only; evaluation is continuous");

		ImGui::SameLine();
		bool bLooping = m_Asset->IsLooping();
		if (ImGui::Checkbox("Looping", &bLooping))
			m_Asset->SetLooping(bLooping);
		UI::Tooltip("Whether components playing this sequence loop it (a component can override this)");

		// Second row: editing options
		ImGui::Checkbox("Snap", &bSnapToFrames);
		UI::Tooltip("Snap keys and the playhead to frames. Hold Alt to temporarily disable");

		ImGui::SameLine();
		ImGui::Checkbox("Show Frames", &bShowFrames);

		ImGui::SameLine();
		ImGui::Checkbox("Preview In Viewport", &bPreviewInViewport);
		UI::Tooltip("Render the level viewport through the sequence camera at the playhead");

		ImGui::SameLine();
		const bool bWasPiloting = bPiloting;
		if (bWasPiloting)
			UI::PushButtonSelectedStyleColors();
		// Fixed size: the label changes, and everything after it would shift otherwise
		if (ImGui::Button(bPiloting ? "Stop Piloting" : "Pilot Camera", ImVec2(110.f, 0.f)))
		{
			if (bPiloting)
				StopPiloting();
			else
				StartPiloting();
		}
		if (bWasPiloting)
			UI::PopButtonSelectedStyleColors();
		UI::Tooltip("Fly the level viewport camera (hold RMB + WASD) to position shots.\n"
			"The viewport jumps to the sequence camera whenever you scrub, and 'Key' records the viewport camera at the playhead");

		ImGui::SameLine();
		if (ImGui::Button("Key Camera"))
			KeyCameraFromViewport(GetTargetCameraTrack(), SnapTime(time));
		UI::Tooltip("Key location + rotation of the camera you're looking through at the playhead (K) into the selected camera track.\n"
			"Creates a camera track if there isn't one yet");
		// Name of the track that gets keyed, in a fixed-width slot so a long name doesn't
		// push the rest of the row around
		{
			Ref<SequenceCameraTrack> target = GetTargetCameraTrack();
			ImGui::SameLine();
			TextInSlot(target ? "-> " + target->GetName() : std::string("-> none"), 100.f,
				"Camera track that 'Key Camera' writes into. Select a camera track to change it");
		}

		// Always drawn, so that toggling piloting doesn't move the buttons after it
		if (!bPiloting)
			UI::PushItemDisabled();
		ImGui::Checkbox("Auto Key", &bAutoKey);
		if (!bPiloting)
			UI::PopItemDisabled();
		UI::Tooltip("While piloting, every camera move rewrites the key at the playhead");

		ImGui::SameLine();
		if (ImGui::Button(m_Asset->IsDirty() ? "Save*" : "Save", ImVec2(60.f, 0.f)))
			Asset::Save(m_Asset);

		// Preview context: which component's transform the sequence is played relative to
		if (Ref<Scene> scene = GetEditorScene())
		{
			std::vector<Entity> candidates;
			for (Entity entity : scene->GetAllEntitiesWith_Vector<SceneSequenceComponent>())
			{
				if (entity.GetComponent<SceneSequenceComponent>().GetAsset() == m_Asset)
					candidates.push_back(entity);
			}

			// Default to the first component using this asset, so the preview matches the level right away
			if (!bPreviewContextInitialized)
			{
				bPreviewContextInitialized = true;
				if (!candidates.empty())
					m_PreviewContextEntity = candidates[0].GetGUID();
			}

			std::string currentLabel = "World Origin";
			if (!m_PreviewContextEntity.IsNull())
			{
				Entity current = scene->GetEntityByGUID(m_PreviewContextEntity);
				currentLabel = current ? current.GetName() : "World Origin (entity missing)";
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(200.f);
			if (ImGui::BeginCombo("##PreviewContext", currentLabel.c_str()))
			{
				if (ImGui::Selectable("World Origin", m_PreviewContextEntity.IsNull()))
					m_PreviewContextEntity = GUID(0, 0);

				for (Entity& entity : candidates)
				{
					ImGui::PushID((void*)entity.GetGUID().GetHash());
					if (ImGui::Selectable(entity.GetName().c_str(), entity.GetGUID() == m_PreviewContextEntity))
						m_PreviewContextEntity = entity.GetGUID();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			UI::HelpMarker("Preview Context. Keys are offsets from the transform of the chosen Scene Sequence component "
				"(unless it plays in world space), exactly as they will be at runtime. 'World Origin' treats keys as world positions.");
		}
	}

	void SceneSequenceAssetEditor::HandleShortcuts()
	{
		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			return;

		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput)
			return;

		const float frame = GetFrameDuration();

		if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
		{
			if (m_Player.IsPlaying())
				m_Player.Pause();
			else
				m_Player.Play();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			DeleteSelectedKeys();

		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
			CopySelectedKeys();

		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
			PasteKeys(SnapTime(m_Player.GetTime()));

		if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false))
			KeyCameraFromViewport(GetTargetCameraTrack(), SnapTime(m_Player.GetTime()));

		if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false))
			FitViewToContent();

		if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && !m_SelectedTrackID.IsNull())
			BeginRenameTrack(m_SelectedTrackID);

		if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
			SetPlayheadTime(0.f);

		if (ImGui::IsKeyPressed(ImGuiKey_End, false))
			SetPlayheadTime(m_Asset->GetDuration());

		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
			SetPlayheadTime(m_Player.GetTime() - frame);

		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
			SetPlayheadTime(m_Player.GetTime() + frame);
	}

	// ---- Piloting ----
	void SceneSequenceAssetEditor::StartPiloting()
	{
		bPiloting = true;
		m_Player.Pause();
		SnapPilotCameraToPlayhead();

		if (Ref<Scene> scene = GetEditorScene())
			m_LastPilotTransform = scene->EditorCamera.GetTransform();
	}

	void SceneSequenceAssetEditor::StopPiloting()
	{
		bPiloting = false;
	}

	void SceneSequenceAssetEditor::SnapPilotCameraToPlayhead()
	{
		Ref<Scene> scene = GetEditorScene();
		if (!scene)
			return;

		// Follow the camera being edited (the selected camera track), so flying shows its own keys even
		// while another camera is live at this time. Falls back to the live camera if it has no keys yet
		SequenceEvalContext context;
		context.Base = GetPreviewBase();
		context.Time = m_Player.GetTime();
		Ref<SequenceCameraTrack> target = GetTargetCameraTrack();
		if (target && target->IsEnabled() && target->HasTransformKeys())
			target->Evaluate(context);
		else
			m_Player.Evaluate(scene.get(), context.Base, &context);

		if (!context.Camera.bValid)
			return; // Nothing keyed yet: leave the viewport where the user put it

		Transform pose = context.Camera.WorldTransform;
		pose.Scale3D = glm::vec3(1.f);
		scene->EditorCamera.SetTransform(pose);
		m_LastPilotTransform = pose;
	}

	// ---- Editing ----
	void SceneSequenceAssetEditor::AddTrack(SequenceTrackType type)
	{
		// Remembered before the new track takes the selection over
		const GUID previouslySelectedTrack = m_SelectedTrackID;

		// Only one Camera Cuts track is meaningful; asking for another just selects the existing one
		if (type == SequenceTrackType::CameraCuts)
		{
			if (Ref<SequenceCameraCutTrack> existing = FindCameraCutTrack())
			{
				m_SelectedTrackID = existing->GetID();
				SetTrackExpanded(existing->GetID(), true);
				return;
			}
		}

		// Adding a cuts track shouldn't change what's on screen, so bake the cuts the automatic rule
		// currently produces. The automatic choice can only change where a shot starts or ends
		std::vector<std::pair<float, GUID>> bakedCuts;
		if (type == SequenceTrackType::CameraCuts)
		{
			std::vector<float> changeTimes = { 0.f };
			for (const auto& views : m_TrackViews)
			{
				if (views.Track->GetType() != SequenceTrackType::Camera)
					continue;

				float start = 0.f, end = 0.f;
				if (((const SequenceCameraTrack*)views.Track.get())->GetShotRange(&start, &end))
				{
					changeTimes.push_back(start);
					changeTimes.push_back(end + s_TimeTolerance * 2.f);
				}
			}
			std::sort(changeTimes.begin(), changeTimes.end());

			const SequenceCameraTrack* previous = nullptr;
			for (float t : changeTimes)
			{
				const SequenceCameraTrack* live = m_Asset->ResolveActiveCamera(t);
				if (live && live != previous)
					bakedCuts.emplace_back(t, live->GetID());
				previous = live;
			}
		}

		Ref<SequenceTrack> track = SequenceTrack::Create(type);
		if (!track)
			return;

		const std::string baseName = Utils::GetEnumName(type);
		uint32_t sameTypeCount = 0;
		for (const auto& views : m_TrackViews)
			if (views.Track->GetType() == type)
				++sameTypeCount;
		track->SetName(sameTypeCount == 0 ? baseName : baseName + " " + std::to_string(sameTypeCount + 1));

		m_Asset->AddTrack(track);
		m_SelectedTrackID = track->GetID();
		SetTrackExpanded(track->GetID(), true);

		if (type == SequenceTrackType::CameraCuts)
		{
			SequenceCameraCutTrack* cuts = (SequenceCameraCutTrack*)track.get();
			for (const auto& [time, cameraID] : bakedCuts)
				cuts->AddCut(time, cameraID);
		}

#if 0
		if (type == SequenceTrackType::PostProcess)
		{
			// If a camera track was selected, assume the new track is meant to grade that shot
			if (const int32_t selectedIndex = FindTrackIndex(previouslySelectedTrack); selectedIndex >= 0)
			{
				const auto& selected = m_TrackViews[selectedIndex].Track;
				if (selected->GetType() == SequenceTrackType::Camera)
					((SequencePostProcessTrack*)track.get())->SetCameraTrackID(selected->GetID());
			}
		}
#endif

		if (type == SequenceTrackType::Camera)
		{
			Ref<SequenceCameraTrack> camera = Cast<SequenceCameraTrack>(track);
			if (Ref<Scene> scene = GetEditorScene())
				camera->SetDefaultFOVDegrees(glm::degrees(scene->EditorCamera.GetPerspectiveVerticalFOV()));

			// Start the track from where the user is looking, so it's immediately usable
			KeyCameraFromViewport(camera, SnapTime(m_Player.GetTime()));
		}

		MarkDirty();
	}

	void SceneSequenceAssetEditor::DuplicateTrack(const GUID& trackID)
	{
		const auto& tracks = m_Asset->GetTracks();
		for (auto it = tracks.begin(); it != tracks.end(); ++it)
		{
			if ((*it)->GetID() != trackID)
				continue;

			Ref<SequenceTrack> clone = (*it)->Clone();
			clone->SetID(GUID()); // Clone copies the ID; a duplicate needs its own
			clone->SetName((*it)->GetName() + " Copy");
			m_Asset->AddTrack(clone);

			m_SelectedTrackID = clone->GetID();
			SetTrackExpanded(clone->GetID(), true);
			MarkDirty();
			return;
		}
	}

	Ref<SequenceCameraCutTrack> SceneSequenceAssetEditor::FindCameraCutTrack() const
	{
		for (const auto& track : m_Asset->GetTracks())
			if (track && track->GetType() == SequenceTrackType::CameraCuts)
				return Cast<SequenceCameraCutTrack>(track);
		return {};
	}

	void SceneSequenceAssetEditor::AddCameraCut(float time, const GUID& cameraTrackID)
	{
		Ref<SequenceCameraCutTrack> cuts = FindCameraCutTrack();
		if (!cuts)
		{
			AddTrack(SequenceTrackType::CameraCuts);
			cuts = FindCameraCutTrack();
			if (!cuts)
				return;
		}

		time = glm::max(0.f, time);
		cuts->AddCut(time, cameraTrackID);

		ClearSelection();
		if (auto* key = cuts->GetCutsChannel().FindKeyAtTime(time, s_TimeTolerance))
			m_Selection.push_back({ cuts->GetID(), 0u, key->ID });
		SetTrackExpanded(cuts->GetID(), true);

		if (time > m_Asset->GetDuration())
			m_Asset->SetDuration(time);

		MarkDirty();
	}

	void SceneSequenceAssetEditor::RequestAddPostProcessProperty(const GUID& trackID, PostProcessProperty property, float time)
	{
		m_PendingPropertyOps.push_back({ trackID, property, time, true });
	}

	void SceneSequenceAssetEditor::RequestRemovePostProcessProperty(const GUID& trackID, PostProcessProperty property)
	{
		m_PendingPropertyOps.push_back({ trackID, property, 0.f, false });
	}

	// Runs after the frame's UI is done, so nothing is holding a pointer into the channel vector
	void SceneSequenceAssetEditor::ApplyPendingPropertyOps()
	{
		if (m_PendingPropertyOps.empty())
			return;

		for (const auto& op : m_PendingPropertyOps)
		{
			const int32_t trackIndex = FindTrackIndex(op.TrackID);
			if (trackIndex < 0 || m_TrackViews[trackIndex].Track->GetType() != SequenceTrackType::PostProcess)
				continue;

			SequencePostProcessTrack* track = (SequencePostProcessTrack*)m_TrackViews[trackIndex].Track.get();
			if (op.bAdd)
				AddPostProcessProperty(op.TrackID, op.Property, op.Time);
			else if (track->RemoveProperty(op.Property))
				MarkDirty();
		}
		m_PendingPropertyOps.clear();

		// The views point into the vector that just moved
		BuildChannelViews();
		PruneSelection();
	}

	void SceneSequenceAssetEditor::AddPostProcessProperty(const GUID& trackID, PostProcessProperty property, float time)
	{
		const int32_t trackIndex = FindTrackIndex(trackID);
		if (trackIndex < 0 || m_TrackViews[trackIndex].Track->GetType() != SequenceTrackType::PostProcess)
			return;

		const PostProcessPropertyInfo* info = FindPostProcessProperty(property);
		if (!info)
			return;

		SequencePostProcessTrack* track = (SequencePostProcessTrack*)m_TrackViews[trackIndex].Track.get();
		const bool bIsNew = !track->HasProperty(property);
		auto& channel = track->AddProperty(property);

		// Seed the first key from the scene's current value, so adding a property doesn't change
		// the look until the user actually animates it
		PostProcessValue value;
		if (Ref<Scene> scene = GetEditorScene(); scene && scene->GetSceneRenderer())
			value = info->Get(scene->GetSceneRenderer()->GetOptions());
		else
			value = info->Get(SceneRendererSettings{});

		time = glm::max(0.f, time);
		std::visit([&value, time](auto& data)
		{
			using ValueType = std::decay_t<decltype(data.GetKey(0).Value)>;
			if constexpr (Utils::IsVariantType<ValueType, PostProcessValue>::value)
			{
				if (const ValueType* typed = std::get_if<ValueType>(&value))
				{
					// Step properties never blend, so their keys are always Constant
					constexpr bool bStep = std::is_same_v<ValueType, bool> || std::is_same_v<ValueType, int32_t> || std::is_same_v<ValueType, uint32_t> || std::is_same_v<ValueType, std::string>;
					data.AddKey(time, *typed, bStep ? SequenceInterpolation::Constant : SequenceInterpolation::Smooth);
				}
			}
			else
			{
				EG_CORE_ERROR("Unsupported variant type: {}", typeid(ValueType).name());
				EG_CORE_ASSERT(false);
			}
		}, channel.Data);

		m_SelectedTrackID = trackID;
		SetTrackExpanded(trackID, true);
		if (bIsNew)
			ClearSelection();

		if (time > m_Asset->GetDuration())
			m_Asset->SetDuration(time);

		MarkDirty();
	}

	void SceneSequenceAssetEditor::DrawAddPostProcessPropertyMenu(const GUID& trackID, float time)
	{
		const int32_t trackIndex = FindTrackIndex(trackID);
		if (trackIndex < 0 || m_TrackViews[trackIndex].Track->GetType() != SequenceTrackType::PostProcess)
			return;

		const SequencePostProcessTrack* track = (const SequencePostProcessTrack*)m_TrackViews[trackIndex].Track.get();

		// Grouped by category, in registry order. Properties already on the track are greyed out
		const char* currentCategory = nullptr;
		bool bCategoryOpen = false;
		for (const auto& info : GetPostProcessProperties())
		{
			if (!currentCategory || std::string_view(currentCategory) != std::string_view(info.Category))
			{
				if (bCategoryOpen)
					ImGui::EndMenu();

				currentCategory = info.Category.c_str();
				bCategoryOpen = ImGui::BeginMenu(currentCategory);
			}

			if (!bCategoryOpen)
				continue;

			const bool bAlreadyThere = track->HasProperty(info.Property);
			if (ImGui::MenuItem(info.Name.c_str(), nullptr, false, !bAlreadyThere))
				RequestAddPostProcessProperty(trackID, info.Property, time);

			if (!info.Tooltip.empty())
				UI::Tooltip(info.Tooltip);
		}

		if (bCategoryOpen)
			ImGui::EndMenu();
	}

	void SceneSequenceAssetEditor::BeginRenameTrack(const GUID& trackID)
	{
		const int32_t index = FindTrackIndex(trackID);
		if (index < 0)
			return;

		m_RenamingTrackID = trackID;
		m_RenameBuffer = m_TrackViews[index].Track->GetName();
		m_SelectedTrackID = trackID;
		bRenameFocusPending = true;
	}

	void SceneSequenceAssetEditor::DeleteTrack(const GUID& trackID)
	{
		if (!m_Asset->RemoveTrack(trackID))
			return;

		m_Selection.erase(std::remove_if(m_Selection.begin(), m_Selection.end(),
			[&trackID](const KeyRef& ref) { return ref.TrackID == trackID; }), m_Selection.end());
		SetTrackExpanded(trackID, false);

		if (m_SelectedTrackID == trackID)
			m_SelectedTrackID = GUID(0, 0);

		MarkDirty();
	}

	void SceneSequenceAssetEditor::KeyCameraFromViewport(const Ref<SequenceCameraTrack>& track, float time)
	{
		Ref<Scene> scene = GetEditorScene();
		if (!scene)
			return;

		if (!track)
		{
			AddTrack(SequenceTrackType::Camera); // Keys itself
			return;
		}

		time = glm::max(0.f, time);

		// Key what the user is actually looking at. While the preview drives the viewport that's the
		// sequence camera (so K just "locks in" the current pose), otherwise it's the editor camera.
		Transform world;
		const bool bViewingSequence = scene->IsCameraOverridden() && scene->GetCameraOverrideOwner() == m_PreviewOwnerID;
		if (bViewingSequence)
		{
			// What's on screen is the live camera at the playhead, whichever track that is.
			// (Evaluating only `track` would fail for a camera that doesn't have keys yet.)
			SequenceEvalContext context;
			m_Player.Evaluate(scene.get(), GetPreviewBase(), &context);
			if (!context.Camera.bValid)
				return;
			world = context.Camera.WorldTransform;
		}
		else
		{
			world = scene->EditorCamera.GetTransform();
		}

		// World -> sequence space (inverse of what `SequenceCameraTrack::Evaluate` does)
		const Transform base = GetPreviewBase();
		const glm::quat invBaseRotation = glm::inverse(base.Rotation.GetQuat());
		const glm::vec3 safeScale = glm::vec3(
			glm::abs(base.Scale3D.x) > 1e-6f ? base.Scale3D.x : 1.f,
			glm::abs(base.Scale3D.y) > 1e-6f ? base.Scale3D.y : 1.f,
			glm::abs(base.Scale3D.z) > 1e-6f ? base.Scale3D.z : 1.f);

		Transform local;
		local.Location = (invBaseRotation * (world.Location - base.Location)) / safeScale;
		local.Rotation = glm::normalize(invBaseRotation * world.Rotation.GetQuat());

		// Match the interpolation of the key to the left so keying mid-sequence keeps the feel
		SequenceInterpolation interpolation = SequenceInterpolation::Smooth;
		for (const auto& key : track->GetLocationChannel().GetKeys())
		{
			if (key.Time <= time)
				interpolation = key.Interpolation;
			else
				break;
		}

		track->AddTransformKey(time, local, interpolation);

		// Select what was just keyed, which also highlights it in the viewport
		ClearSelection();
		if (auto* key = track->GetLocationChannel().FindKeyAtTime(time, s_TimeTolerance))
			m_Selection.push_back({ track->GetID(), 0u, key->ID });
		if (auto* key = track->GetRotationChannel().FindKeyAtTime(time, s_TimeTolerance))
			m_Selection.push_back({ track->GetID(), 1u, key->ID });

		m_SelectedTrackID = track->GetID();

		// Grow the sequence if the user keyed past its end
		if (time > m_Asset->GetDuration())
			m_Asset->SetDuration(time);

		MarkDirty();
	}

	void SceneSequenceAssetEditor::AddKeysAtTime(uint32_t trackIndex, int32_t channelIndex, float time)
	{
		if (trackIndex >= m_TrackViews.size())
			return;

		auto& views = m_TrackViews[trackIndex];
		const GUID trackID = views.Track->GetID();
		time = glm::max(0.f, time);

		if (channelIndex >= 0)
		{
			if (channelIndex < int32_t(views.Channels.size()))
			{
				const GUID keyID = views.Channels[channelIndex]->AddKeyAtTime(time);
				Select({ trackID, uint32_t(channelIndex), keyID }, false);
				MarkDirty();
			}
			return;
		}

		// Header row: key every channel that is already animated, at its current value
		ClearSelection();
		bool bAnyKeyed = false;
		for (uint32_t ci = 0; ci < uint32_t(views.Channels.size()); ++ci)
		{
			auto& view = views.Channels[ci];
			if (view->GetKeysCount() == 0)
				continue;

			m_Selection.push_back({ trackID, ci, view->AddKeyAtTime(time) });
			bAnyKeyed = true;
		}

		// Nothing to evaluate yet: for a camera, that means "start from the viewport"
		if (!bAnyKeyed && views.Track->GetType() == SequenceTrackType::Camera)
		{
			KeyCameraFromViewport(Cast<SequenceCameraTrack>(views.Track), time);
			return;
		}

		if (!bAnyKeyed && views.Track->GetType() == SequenceTrackType::CameraCuts && !views.Channels.empty())
		{
			m_Selection.push_back({ trackID, 0u, views.Channels[0]->AddKeyAtTime(time) });
			bAnyKeyed = true;
		}

		if (bAnyKeyed)
			MarkDirty();
	}

	void SceneSequenceAssetEditor::DeleteSelectedKeys()
	{
		if (m_Selection.empty())
			return;

		for (const auto& ref : m_Selection)
		{
			if (SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex))
				view->RemoveKey(ref.KeyID);
		}
		ClearSelection();
		MarkDirty();
	}

	void SceneSequenceAssetEditor::CopySelectedKeys()
	{
		if (m_Selection.empty())
			return;

		// Times are stored relative to the earliest key, so pasting keeps the spacing
		float earliest = std::numeric_limits<float>::max();
		for (const auto& ref : m_Selection)
		{
			SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
			size_t index = 0;
			if (view && view->FindKeyIndex(ref.KeyID, &index))
				earliest = glm::min(earliest, view->GetKeyTime(index));
		}

		m_Clipboard.clear();
		for (const auto& ref : m_Selection)
		{
			SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
			size_t index = 0;
			if (!view || !view->FindKeyIndex(ref.KeyID, &index))
				continue;

			ClipboardEntry& entry = m_Clipboard.emplace_back();
			entry.TrackID = ref.TrackID;
			entry.ChannelIndex = ref.ChannelIndex;
			entry.RelativeTime = view->GetKeyTime(index) - earliest;
			entry.Key = view->CopyKey(ref.KeyID);
		}
	}

	void SceneSequenceAssetEditor::PasteKeys(float atTime)
	{
		if (m_Clipboard.empty())
			return;

		// If a different track is selected, paste into it. Handy for copying a move between cameras.
		// Channels whose value type doesn't match are silently skipped by `PasteKey`
		const bool bRetarget = !m_SelectedTrackID.IsNull() && FindTrackIndex(m_SelectedTrackID) >= 0;

		ClearSelection();
		for (const auto& entry : m_Clipboard)
		{
			const GUID targetTrack = bRetarget ? m_SelectedTrackID : entry.TrackID;
			SequenceChannelView* view = FindView(targetTrack, entry.ChannelIndex);
			if (!view)
				continue;

			const GUID pastedID = view->PasteKey(entry.Key, atTime + entry.RelativeTime);
			if (!pastedID.IsNull())
				m_Selection.push_back({ targetTrack, entry.ChannelIndex, pastedID });
		}

		if (!m_Selection.empty())
			MarkDirty();
	}

	void SceneSequenceAssetEditor::SetSelectedInterpolation(SequenceInterpolation interpolation)
	{
		for (const auto& ref : m_Selection)
		{
			if (SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex))
				view->SetKeyInterpolation(ref.KeyID, interpolation);
		}
		MarkDirty();
	}

	void SceneSequenceAssetEditor::ResolveOverlapsAfterMove()
	{
		// A dragged key dropped onto an unselected key replaces it. Two keys at one time would
		// make evaluation depend on sort stability, which nobody wants to debug
		for (const auto& views : m_TrackViews)
		{
			for (uint32_t ci = 0; ci < uint32_t(views.Channels.size()); ++ci)
			{
				auto& view = views.Channels[ci];

				std::vector<GUID> toRemove;
				for (size_t i = 0; i < view->GetKeysCount(); ++i)
				{
					const KeyRef ref{ views.Track->GetID(), ci, view->GetKeyID(i) };
					if (IsSelected(ref))
						continue;

					for (size_t j = 0; j < view->GetKeysCount(); ++j)
					{
						if (i == j)
							continue;

						const KeyRef other{ views.Track->GetID(), ci, view->GetKeyID(j) };
						if (IsSelected(other) && glm::abs(view->GetKeyTime(i) - view->GetKeyTime(j)) <= s_TimeTolerance)
						{
							toRemove.push_back(ref.KeyID);
							break;
						}
					}
				}

				for (const GUID& id : toRemove)
					view->RemoveKey(id);
			}
		}
	}

	// ---- Timeline ----
	void SceneSequenceAssetEditor::DrawTimeline()
	{
		// Fills the whole timeline region; the Details/Curves panel lives to the right
		const ImVec2 available = ImGui::GetContentRegionAvail();
		DrawRuler(available.x);
		DrawRows(available.x, glm::max(1.f, available.y - s_RulerHeight));
	}

	void SceneSequenceAssetEditor::DrawTracksAndCurves()
	{
		if (!ImGui::BeginTabBar("##SceneSequenceTabs"))
			return;

		// Tracks and curves are two views of the same thing, so switching between them keeps the
		// selection and the playhead. Both get the whole left region
		if (ImGui::BeginTabItem("Tracks"))
		{
			DrawTimeline();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Curves"))
		{
			if (ImGui::BeginChild("##SequenceCurvesRegion"))
				DrawCurvesTab();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	void SceneSequenceAssetEditor::HandleZoomPan(bool bHovered)
	{
		if (!bHovered)
			return;

		const ImGuiIO& io = ImGui::GetIO();
		if (io.MouseWheel != 0.f && io.KeyCtrl)
		{
			// Zoom around the mouse, so the time under the cursor stays put
			const float mouseTime = XToTime(io.MousePos.x);
			m_PixelsPerSecond = glm::clamp(m_PixelsPerSecond * glm::pow(1.2f, io.MouseWheel), 5.f, 5000.f);
			m_ViewStart = mouseTime - (io.MousePos.x - m_TimelineOriginX) / m_PixelsPerSecond;
		}
		else if (io.MouseWheel != 0.f && io.KeyShift)
		{
			m_ViewStart -= io.MouseWheel * 80.f / m_PixelsPerSecond;
		}

		// Horizontal wheel
		if (io.MouseWheelH != 0.f)
			m_ViewStart -= io.MouseWheelH * 80.f / m_PixelsPerSecond;
	}

	void SceneSequenceAssetEditor::DrawAddTrackPopup()
	{
		if (ImGui::BeginPopup("##AddTrackPopup"))
		{
			for (const auto& [type, name] : magic_enum::enum_entries<SequenceTrackType>())
			{
				if (ImGui::MenuItem(std::string(name).c_str()))
					AddTrack(type);
			}
			ImGui::EndPopup();
		}
	}

	void SceneSequenceAssetEditor::DrawRuler(float width)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const float height = s_RulerHeight;

		m_TimelineOriginX = pos.x + m_TrackListWidth;
		m_TimelineWidth = glm::max(1.f, width - m_TrackListWidth);

		// Left side: track list tools
		ImGui::SetCursorScreenPos(ImVec2(pos.x + 4.f, pos.y + 3.f));
		if (ImGui::SmallButton("+ Track"))
			ImGui::OpenPopup("##AddTrackPopup");
		DrawAddTrackPopup();

		ImGui::SameLine();
		if (ImGui::SmallButton("Fit"))
			FitViewToContent();
		UI::Tooltip("Fit the whole sequence into view (F)");

		ImGui::SameLine();
		UI::HelpMarker(
			"Timeline controls\n"
			"- Click a key to select it, Ctrl/Shift+click to add to the selection\n"
			"- Drag keys to retime them. Hold Alt to disable frame snapping\n"
			"- Drag on empty space to box-select\n"
			"- Double-click empty space to add a key; on a track row, keys every animated channel\n"
			"- Right-click keys to change interpolation, copy or delete them\n"
			"- Ctrl+Wheel zooms, Shift+Wheel or Middle-drag pans\n"
			"- Drag in the ruler to scrub; drag the red marker to change the duration\n"
			"- Double-click a track name (or F2) to rename it\n"
			"\n"
			"Several cameras: the green bar on a camera track shows when it's live, and the green dot marks the\n"
			"live camera at the playhead. By default the live camera is the one whose keys cover the current time.\n"
			"For exact control add a 'CameraCuts' track, or right-click a camera track and pick 'Cut To This Camera'\n"
			"\n"
			"Shortcuts: Space play/pause, K key camera, Del delete, Ctrl+C/V copy/paste,\n"
			"Left/Right step a frame, Home/End jump, F fit view\n"
			"\n"
			"Key shapes: square = Constant, triangle = Linear, circle = Smooth, diamond = Cubic");

		// Ruler
		const ImVec2 rulerMin(m_TimelineOriginX, pos.y);
		const ImVec2 rulerMax(pos.x + width, pos.y + height);
		ImGui::SetCursorScreenPos(rulerMin);
		ImGui::InvisibleButton("##Ruler", ImVec2(m_TimelineWidth, height));
		const bool bHovered = ImGui::IsItemHovered();

		const float duration = m_Asset->GetDuration();
		const float durationX = TimeToX(duration);

		if (ImGui::IsItemActivated())
			m_DragMode = glm::abs(io.MousePos.x - durationX) <= 6.f ? DragMode::Duration : DragMode::Playhead;

		if (ImGui::IsItemActive())
		{
			const float time = SnapTime(XToTime(io.MousePos.x));
			if (m_DragMode == DragMode::Playhead)
			{
				SetPlayheadTime(time);
			}
			else if (m_DragMode == DragMode::Duration)
			{
				m_Asset->SetDuration(glm::max(GetFrameDuration(), time));
				ImGui::SetTooltip("Duration: %s", FormatTime(m_Asset->GetDuration()).c_str());
			}
		}
		else if (m_DragMode == DragMode::Playhead || m_DragMode == DragMode::Duration)
		{
			m_DragMode = DragMode::None;
		}

		HandleZoomPan(bHovered);

		if (bHovered && m_DragMode == DragMode::None && glm::abs(io.MousePos.x - durationX) <= 6.f)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			ImGui::SetTooltip("Sequence end: %s (drag to change)", FormatTime(duration).c_str());
		}

		// Draw
		drawList->AddRectFilled(rulerMin, rulerMax, IM_COL32(32, 32, 36, 255));
		drawList->PushClipRect(rulerMin, rulerMax, true);

		float major = 1.f, minor = 0.2f;
		ComputeTickSteps(&major, &minor);
		const float viewEnd = XToTime(rulerMax.x);
		const int32_t firstTick = int32_t(glm::floor(m_ViewStart / minor));
		const int32_t lastTick = int32_t(glm::ceil(viewEnd / minor));
		for (int32_t i = firstTick; i <= lastTick; ++i)
		{
			const float t = float(i) * minor;
			const float x = TimeToX(t);
			const bool bMajor = ((i % 5) + 5) % 5 == 0;

			drawList->AddLine(ImVec2(x, rulerMax.y - (bMajor ? 10.f : 5.f)), ImVec2(x, rulerMax.y), IM_COL32(150, 150, 150, 255));
			if (bMajor)
			{
				const std::string label = FormatTime(t);
				drawList->AddText(ImVec2(x + 3.f, rulerMin.y + 2.f), t < 0.f ? IM_COL32(100, 100, 100, 255) : IM_COL32(200, 200, 200, 255), label.c_str());
			}
		}

		// Past the end of the sequence
		drawList->AddRectFilled(ImVec2(durationX, rulerMin.y), rulerMax, IM_COL32(0, 0, 0, 80));
		drawList->AddLine(ImVec2(durationX, rulerMin.y), ImVec2(durationX, rulerMax.y), IM_COL32(230, 80, 80, 220), 2.f);
		drawList->AddTriangleFilled(ImVec2(durationX - 6.f, rulerMin.y), ImVec2(durationX + 6.f, rulerMin.y), ImVec2(durationX, rulerMin.y + 8.f), IM_COL32(230, 80, 80, 255));

		// Playhead handle with the current time on it
		{
			const float playheadX = TimeToX(m_Player.GetTime());
			const std::string label = FormatTime(m_Player.GetTime());
			const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
			const float halfWidth = textSize.x * 0.5f + 5.f;

			drawList->AddRectFilled(ImVec2(playheadX - halfWidth, rulerMin.y + 2.f), ImVec2(playheadX + halfWidth, rulerMin.y + 4.f + textSize.y), s_PlayheadColor, 3.f);
			drawList->AddText(ImVec2(playheadX - textSize.x * 0.5f, rulerMin.y + 3.f), IM_COL32(255, 255, 255, 255), label.c_str());
			drawList->AddLine(ImVec2(playheadX, rulerMin.y + 4.f + textSize.y), ImVec2(playheadX, rulerMax.y), s_PlayheadColor, 2.f);
		}

		drawList->PopClipRect();

		ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height));
	}

	bool SceneSequenceAssetEditor::HitTestKeys(const RowLayout& row, float mouseX, float radius, std::vector<KeyRef>& outRefs) const
	{
		if (row.TrackIndex >= m_TrackViews.size())
			return false;

		const auto& views = m_TrackViews[row.TrackIndex];

		if (row.ChannelIndex < 0)
		{
			// Summary row: find the nearest key time across every channel, then take all keys at that time
			float bestDistance = radius;
			float bestTime = 0.f;
			bool bFound = false;
			for (const auto& view : views.Channels)
			{
				for (size_t i = 0; i < view->GetKeysCount(); ++i)
				{
					const float distance = glm::abs(TimeToX(view->GetKeyTime(i)) - mouseX);
					if (distance <= bestDistance)
					{
						bestDistance = distance;
						bestTime = view->GetKeyTime(i);
						bFound = true;
					}
				}
			}

			if (bFound)
				GatherTrackKeysAtTime(row.TrackIndex, bestTime, outRefs);
			return !outRefs.empty();
		}

		if (row.ChannelIndex >= int32_t(views.Channels.size()))
			return false;

		const auto& view = views.Channels[row.ChannelIndex];
		float bestDistance = radius;
		int64_t bestIndex = -1;
		for (size_t i = 0; i < view->GetKeysCount(); ++i)
		{
			const float distance = glm::abs(TimeToX(view->GetKeyTime(i)) - mouseX);
			if (distance <= bestDistance)
			{
				bestDistance = distance;
				bestIndex = int64_t(i);
			}
		}

		if (bestIndex < 0)
			return false;

		outRefs.push_back({ views.Track->GetID(), uint32_t(row.ChannelIndex), view->GetKeyID(size_t(bestIndex)) });
		return true;
	}

	void SceneSequenceAssetEditor::DrawRows(float width, float height)
	{
		const ImGuiIO& io = ImGui::GetIO();

		// Ctrl/Shift+wheel belong to the timeline (zoom/pan), not to vertical scrolling
		ImGuiWindowFlags flags = ImGuiWindowFlags_None;
		if (io.KeyCtrl || io.KeyShift)
			flags |= ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		const bool bChildVisible = ImGui::BeginChild("##SequenceRows", ImVec2(width, glm::max(1.f, height)), false, flags);
		ImGui::PopStyleVar();

		if (!bChildVisible)
		{
			ImGui::EndChild();
			return;
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 windowMin = ImGui::GetWindowPos();
		const ImVec2 windowMax(windowMin.x + ImGui::GetWindowSize().x, windowMin.y + ImGui::GetWindowSize().y);
		const float rightX = origin.x + ImGui::GetContentRegionAvail().x;
		const float rowHeight = ImGui::GetFrameHeight() + 4.f;
		const float time = m_Player.GetTime();

		auto drawRowBackground = [&](float top, bool bHeader, bool bSelected)
		{
			ImU32 color = bHeader ? IM_COL32(48, 48, 53, 255) : IM_COL32(38, 38, 42, 255);
			if (bHeader && bSelected)
				color = IM_COL32(58, 62, 80, 255);
			drawList->AddRectFilled(ImVec2(origin.x, top), ImVec2(rightX, top + rowHeight), color);
			drawList->AddLine(ImVec2(origin.x, top + rowHeight - 1.f), ImVec2(rightX, top + rowHeight - 1.f), IM_COL32(24, 24, 27, 255));
		};

		// Track mutations requested from menus are applied after the loop, while nothing iterates tracks
		GUID pendingDuplicate = GUID(0, 0);
		GUID pendingDelete = GUID(0, 0);

		m_Rows.clear();
		float y = origin.y;
		const float labelRight = origin.x + m_TrackListWidth - 8.f;

		// Which camera is live, at the playhead and across the visible part of the timeline.
		// The spans are drawn on camera header rows, so it's visible at a glance which shot plays when
		const SequenceTrack* liveCameraAtPlayhead = m_Asset->ResolveActiveCamera(time);
		struct LiveSpan
		{
			float X0 = 0.f;
			float X1 = 0.f;
			const SequenceTrack* Camera = nullptr;
		};
		std::vector<LiveSpan> liveSpans;
		{
			constexpr float sampleStep = 3.f; // px
			const float duration = m_Asset->GetDuration();
			const SequenceTrack* current = nullptr;
			float spanStart = m_TimelineOriginX;
			for (float x = m_TimelineOriginX; x <= rightX + sampleStep; x += sampleStep)
			{
				const float t = XToTime(x);
				const SequenceTrack* live = (t < 0.f || t > duration) ? nullptr : m_Asset->ResolveActiveCamera(t);
				if (live != current)
				{
					if (current)
						liveSpans.push_back({ spanStart, x, current });
					current = live;
					spanStart = x;
				}
			}
			if (current)
				liveSpans.push_back({ spanStart, rightX, current });
		}

		for (uint32_t ti = 0; ti < uint32_t(m_TrackViews.size()); ++ti)
		{
			auto& views = m_TrackViews[ti];
			const Ref<SequenceTrack>& track = views.Track;
			const GUID trackID = track->GetID();
			const bool bExpanded = IsTrackExpanded(trackID);
			const bool bTrackSelected = trackID == m_SelectedTrackID;

			// ---- Track header row ----
			drawRowBackground(y, true, bTrackSelected);
			m_Rows.push_back({ y, y + rowHeight, ti, -1 });

			ImGui::PushID((void*)trackID.GetHash());
			ImGui::PushClipRect(ImVec2(origin.x, y), ImVec2(labelRight, y + rowHeight), true);
			ImGui::SetCursorScreenPos(ImVec2(origin.x + 4.f, y + 2.f));

			if (ImGui::ArrowButton("##Expand", bExpanded ? ImGuiDir_Down : ImGuiDir_Right))
				SetTrackExpanded(trackID, !bExpanded);

			ImGui::SameLine();
			bool bEnabled = track->IsEnabled();
			if (ImGui::Checkbox("##Enabled", &bEnabled))
			{
				track->SetEnabled(bEnabled);
				MarkDirty();
			}
			UI::Tooltip("Enable/disable this track");

			ImGui::SameLine();

			// Live camera indicator: a dot at the end of the name area
			const bool bIsLiveCamera = track.get() == liveCameraAtPlayhead;
			constexpr float liveDotSpace = 14.f;
			const float nameWidth = glm::max(10.f, labelRight - ImGui::GetCursorScreenPos().x - (bIsLiveCamera ? liveDotSpace : 0.f));

			if (m_RenamingTrackID == trackID)
			{
				// Inline rename. Enter or clicking away commits, Escape cancels
				if (bRenameFocusPending)
				{
					ImGui::SetKeyboardFocusHere();
					bRenameFocusPending = false;
				}
				ImGui::SetNextItemWidth(nameWidth);
				UI::InputText("##RenameTrack", m_RenameBuffer, ImGuiInputTextFlags_AutoSelectAll);

				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
				{
					m_RenamingTrackID = GUID(0, 0);
				}
				else if (ImGui::IsItemDeactivated())
				{
					if (!m_RenameBuffer.empty() && m_RenameBuffer != track->GetName())
					{
						track->SetName(m_RenameBuffer);
						MarkDirty();
					}
					m_RenamingTrackID = GUID(0, 0);
				}
			}
			else
			{
				if (ImGui::Selectable(track->GetName().c_str(), bTrackSelected, ImGuiSelectableFlags_None, ImVec2(nameWidth, ImGui::GetFrameHeight())))
					m_SelectedTrackID = trackID;
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
					m_SelectedTrackID = trackID;
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					BeginRenameTrack(trackID);
			}

			if (bIsLiveCamera)
			{
				const ImVec2 dotCenter(labelRight - liveDotSpace * 0.5f, y + rowHeight * 0.5f);
				drawList->AddCircleFilled(dotCenter, 4.f, s_LiveCameraColor);
				if (ImGui::IsMouseHoveringRect(ImVec2(dotCenter.x - 6.f, dotCenter.y - 6.f), ImVec2(dotCenter.x + 6.f, dotCenter.y + 6.f)))
					ImGui::SetTooltip("Live camera at the playhead");
			}

			if (ImGui::BeginPopupContextItem("##TrackContext"))
			{
				if (ImGui::MenuItem("Add Key At Playhead"))
					AddKeysAtTime(ti, -1, SnapTime(time));

				if (track->GetType() == SequenceTrackType::PostProcess)
				{
					if (ImGui::BeginMenu("Add Property"))
					{
						DrawAddPostProcessPropertyMenu(trackID, SnapTime(time));
						ImGui::EndMenu();
					}
				}

				if (track->GetType() == SequenceTrackType::Camera)
				{
					if (ImGui::MenuItem("Key Camera At Playhead", "K"))
						KeyCameraFromViewport(Cast<SequenceCameraTrack>(track), SnapTime(time));
					if (ImGui::MenuItem("Cut To This Camera At Playhead"))
						AddCameraCut(SnapTime(time), trackID);
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Rename", "F2"))
					BeginRenameTrack(trackID);
				// A second cuts track would be ignored, so don't offer to make one
				if (ImGui::MenuItem("Duplicate", nullptr, false, track->GetType() != SequenceTrackType::CameraCuts))
					pendingDuplicate = trackID;
				if (ImGui::MenuItem("Delete"))
					pendingDelete = trackID;

				ImGui::EndPopup();
			}

			ImGui::PopClipRect();
			ImGui::PopID();
			y += rowHeight;

			// ---- Channel rows ----
			if (!bExpanded)
				continue;

			for (uint32_t ci = 0; ci < uint32_t(views.Channels.size()); ++ci)
			{
				auto& view = views.Channels[ci];

				drawRowBackground(y, false, false);
				m_Rows.push_back({ y, y + rowHeight, ti, int32_t(ci) });

				ImGui::PushID((void*)trackID.GetHash());
				ImGui::PushID(int(ci));
				ImGui::PushClipRect(ImVec2(origin.x, y), ImVec2(labelRight, y + rowHeight), true);

				// Color swatch. For channels that can be switched off it doubles as the toggle:
				// filled means enabled, hollow means disabled
				const bool bChannelEnabled = view->IsEnabled();
				const float swatchY = y + rowHeight * 0.5f;
				const ImVec2 swatchMin(origin.x + 26.f, swatchY - 6.f);
				const ImVec2 swatchMax(origin.x + 38.f, swatchY + 6.f);

				if (view->CanBeDisabled())
				{
					ImGui::SetCursorScreenPos(swatchMin);
					ImGui::InvisibleButton("##ToggleChannel", ImVec2(swatchMax.x - swatchMin.x, swatchMax.y - swatchMin.y));
					if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					{
						view->SetEnabled(!bChannelEnabled);
						MarkDirty();
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(bChannelEnabled ? "Click to stop overriding this property" : "Click to override this property again");
				}

				if (bChannelEnabled)
					drawList->AddRectFilled(ImVec2(swatchMin.x + 2.f, swatchMin.y + 2.f), ImVec2(swatchMax.x - 2.f, swatchMax.y - 2.f), view->GetColor(), 2.f);
				else
					drawList->AddRect(ImVec2(swatchMin.x + 2.f, swatchMin.y + 2.f), ImVec2(swatchMax.x - 2.f, swatchMax.y - 2.f), view->GetColor(), 2.f, 0, 1.5f);

				ImGui::SetCursorScreenPos(ImVec2(origin.x + 42.f, y + 2.f));
				const float labelWidth = glm::max(10.f, labelRight - ImGui::GetCursorScreenPos().x);

				// A disabled property is dimmed, like the rest of the editor dims inactive things
				if (!bChannelEnabled)
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				if (ImGui::Selectable(view->GetName().c_str(), false, ImGuiSelectableFlags_None, ImVec2(labelWidth, ImGui::GetFrameHeight())))
					m_SelectedTrackID = trackID;
				if (!bChannelEnabled)
					ImGui::PopStyleColor();

				if (ImGui::BeginPopupContextItem("##ChannelContext"))
				{
					if (ImGui::MenuItem("Add Key At Playhead"))
						AddKeysAtTime(ti, int32_t(ci), SnapTime(time));

					if (ImGui::MenuItem("Select All Keys", nullptr, false, view->GetKeysCount() > 0))
					{
						ClearSelection();
						for (size_t i = 0; i < view->GetKeysCount(); ++i)
							m_Selection.push_back({ trackID, ci, view->GetKeyID(i) });
					}

					if (ImGui::MenuItem("Delete All Keys", nullptr, false, view->GetKeysCount() > 0))
					{
						while (view->GetKeysCount() > 0)
							view->RemoveKey(view->GetKeyID(0));
						MarkDirty();
					}

					// Removing the property stops overriding it entirely, unlike deleting its keys
					if (track->GetType() == SequenceTrackType::PostProcess)
					{
						ImGui::Separator();

						bool bEnabled = view->IsEnabled();
						if (ImGui::MenuItem(bEnabled ? "Disable Property" : "Enable Property"))
						{
							view->SetEnabled(!bEnabled);
							MarkDirty();
						}

						if (ImGui::MenuItem("Remove Property"))
						{
							SequencePostProcessTrack* postProcess = (SequencePostProcessTrack*)track.get();
							if (ci < uint32_t(postProcess->GetChannels().size()))
								RequestRemovePostProcessProperty(trackID, postProcess->GetChannels()[ci].Property);
						}
					}
					ImGui::EndPopup();
				}

				ImGui::PopClipRect();
				ImGui::PopID();
				ImGui::PopID();
				y += rowHeight;
			}
		}

		if (m_TrackViews.empty())
		{
			drawList->AddText(ImVec2(origin.x + 8.f, origin.y + 8.f), IM_COL32(150, 150, 150, 255),
				"No tracks yet. Use '+ Track', or position the level camera and press 'Key Camera' (K).");
		}

		const float contentHeight = glm::max(y - origin.y, windowMax.y - origin.y);

		// Track list / timeline splitter
		{
			ImGui::SetCursorScreenPos(ImVec2(origin.x + m_TrackListWidth - 6.f, origin.y));
			ImGui::InvisibleButton("##TrackListSplitter", ImVec2(6.f, glm::max(1.f, contentHeight)));
			const bool bActive = ImGui::IsItemActive();
			if (bActive || ImGui::IsItemHovered())
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if (bActive)
				m_TrackListWidth = glm::clamp(m_TrackListWidth + io.MouseDelta.x, 120.f, 600.f);

			const float splitterX = origin.x + m_TrackListWidth - 3.f;
			drawList->AddLine(ImVec2(splitterX, windowMin.y), ImVec2(splitterX, windowMax.y),
				ImGui::GetColorU32(bActive ? ImGuiCol_SeparatorActive : ImGuiCol_Separator));
		}

		// One invisible button captures all timeline mouse input. Keys are hit-tested manually,
		// which is far cheaper than an ImGui item per key and makes box selection straightforward
		const ImVec2 areaMin(m_TimelineOriginX, origin.y);
		const ImVec2 areaMax(rightX, origin.y + contentHeight);
		ImGui::SetCursorScreenPos(areaMin);
		ImGui::InvisibleButton("##TimelineArea", ImVec2(glm::max(1.f, areaMax.x - areaMin.x), glm::max(1.f, contentHeight)),
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
		const bool bAreaHovered = ImGui::IsItemHovered();
		HandleRowsMouse(areaMin, areaMax);

		// ---- Draw keys ----
		const float visibleTop = windowMin.y;
		const float visibleBottom = windowMax.y;
		drawList->PushClipRect(ImVec2(areaMin.x, visibleTop), ImVec2(areaMax.x, visibleBottom), true);

		// Grid
		{
			float major = 1.f, minor = 0.2f;
			ComputeTickSteps(&major, &minor);
			const float viewEnd = XToTime(areaMax.x);
			for (int32_t i = int32_t(glm::floor(m_ViewStart / major)); i <= int32_t(glm::ceil(viewEnd / major)); ++i)
			{
				const float x = TimeToX(float(i) * major);
				drawList->AddLine(ImVec2(x, visibleTop), ImVec2(x, visibleBottom), IM_COL32(255, 255, 255, 14));
			}
		}

		// Past the end of the sequence
		const float durationX = TimeToX(m_Asset->GetDuration());
		drawList->AddRectFilled(ImVec2(durationX, visibleTop), ImVec2(areaMax.x, visibleBottom), IM_COL32(0, 0, 0, 60));
		drawList->AddLine(ImVec2(durationX, visibleTop), ImVec2(durationX, visibleBottom), IM_COL32(230, 80, 80, 120), 1.f);

		const ImVec2 mouse = io.MousePos;
		const bool bCanHover = bAreaHovered && m_DragMode == DragMode::None;
		std::string tooltip;

		std::vector<float> summaryTimes;
		std::vector<KeyRef> refsAtTime;
		for (const RowLayout& row : m_Rows)
		{
			if (row.MaxY < visibleTop || row.MinY > visibleBottom)
				continue;

			const float centerY = (row.MinY + row.MaxY) * 0.5f;
			const bool bMouseInRow = mouse.y >= row.MinY && mouse.y < row.MaxY;
			auto& views = m_TrackViews[row.TrackIndex];

			if (row.ChannelIndex < 0 && views.Track->GetType() == SequenceTrackType::PostProcess)
			{
				// Shows exactly when this track overrides rendering settings. Outside the bar,
				// the scene's own settings are back in charge
				const SequencePostProcessTrack* postProcess = (const SequencePostProcessTrack*)views.Track.get();
				float rangeStart = 0.f, rangeEnd = 0.f;
				if (postProcess->GetActiveRange(&rangeStart, &rangeEnd))
				{
					if (postProcess->DoesApplyWholeSequence())
					{
						rangeStart = 0.f;
						rangeEnd = m_Asset->GetDuration();
					}

					const GUID& linkedCamera = postProcess->GetCameraTrackID();
					if (linkedCamera.IsNull())
					{
						drawList->AddRectFilled(ImVec2(TimeToX(rangeStart), row.MinY + 3.f), ImVec2(TimeToX(rangeEnd), row.MaxY - 4.f),
							WithAlpha(s_PostProcessRangeColor, 0.22f), 2.f);
					}
					else
					{
						// Linked to a camera: only the parts where that camera is live actually apply
						for (const LiveSpan& span : liveSpans)
						{
							if (!span.Camera || span.Camera->GetID() != linkedCamera)
								continue;

							const float x0 = glm::max(span.X0, TimeToX(rangeStart));
							const float x1 = glm::min(span.X1, TimeToX(rangeEnd));
							if (x1 > x0)
								drawList->AddRectFilled(ImVec2(x0, row.MinY + 3.f), ImVec2(x1, row.MaxY - 4.f),
									WithAlpha(s_PostProcessRangeColor, 0.22f), 2.f);
						}
					}
				}
			}

			if (row.ChannelIndex < 0)
			{
				if (views.Track->GetType() == SequenceTrackType::Camera)
				{
					for (const LiveSpan& span : liveSpans)
					{
						if (span.Camera == views.Track.get())
							drawList->AddRectFilled(ImVec2(span.X0, row.MinY + 3.f), ImVec2(span.X1, row.MaxY - 4.f), WithAlpha(s_LiveCameraColor, 0.22f), 2.f);
					}
				}

				// Summary keys: one marker per distinct time across the track's channels
				summaryTimes.clear();
				for (const auto& view : views.Channels)
					for (size_t i = 0; i < view->GetKeysCount(); ++i)
						summaryTimes.push_back(view->GetKeyTime(i));

				std::sort(summaryTimes.begin(), summaryTimes.end());
				summaryTimes.erase(std::unique(summaryTimes.begin(), summaryTimes.end(),
					[](float a, float b) { return glm::abs(a - b) <= s_TimeTolerance; }), summaryTimes.end());

				for (float t : summaryTimes)
				{
					const float x = TimeToX(t);
					if (x < areaMin.x - 10.f || x > areaMax.x + 10.f)
						continue;

					refsAtTime.clear();
					GatherTrackKeysAtTime(row.TrackIndex, t, refsAtTime);
					size_t selectedCount = 0;
					for (const auto& ref : refsAtTime)
						if (IsSelected(ref))
							++selectedCount;

					const bool bHovered = bCanHover && bMouseInRow && glm::abs(mouse.x - x) <= s_KeyHitRadius;
					const ImU32 fill = selectedCount == refsAtTime.size() && selectedCount > 0 ? s_SelectedKeyColor
						: (selectedCount > 0 ? WithAlpha(s_SelectedKeyColor, 0.55f) : s_SummaryKeyColor);

					DrawKeyShape(drawList, ImVec2(x, centerY), 5.f, SequenceInterpolation::Cubic, fill, bHovered ? s_HoveredOutlineColor : s_KeyOutlineColor);

					if (bHovered)
						tooltip = std::to_string(refsAtTime.size()) + " key(s) at " + FormatTime(t) + "\nDrag to retime them together";
				}
				continue;
			}

			if (row.ChannelIndex >= int32_t(views.Channels.size()))
				continue;

			const auto& view = views.Channels[row.ChannelIndex];
			const size_t count = view->GetKeysCount();
			const bool bChannelEnabled = view->IsEnabled();

			// Segments between keys, so gaps and holds are visible at a glance
			for (size_t i = 0; i + 1 < count; ++i)
			{
				const float x0 = TimeToX(view->GetKeyTime(i));
				const float x1 = TimeToX(view->GetKeyTime(i + 1));
				const bool bHold = view->GetKeyInterpolation(i) == SequenceInterpolation::Constant;
				drawList->AddLine(ImVec2(x0, centerY), ImVec2(x1, centerY), WithAlpha(view->GetColor(), bHold ? 0.2f : 0.4f), bHold ? 1.f : 2.f);
			}

			for (size_t i = 0; i < count; ++i)
			{
				const float x = TimeToX(view->GetKeyTime(i));
				if (x < areaMin.x - 10.f || x > areaMax.x + 10.f)
					continue;

				const KeyRef ref{ views.Track->GetID(), uint32_t(row.ChannelIndex), view->GetKeyID(i) };
				const bool bSelected = IsSelected(ref);
				const bool bHovered = bCanHover && bMouseInRow && glm::abs(mouse.x - x) <= s_KeyHitRadius;
				const SequenceInterpolation interpolation = view->GetKeyInterpolation(i);

				const ImU32 keyColor = bSelected ? s_SelectedKeyColor : view->GetColor();
				DrawKeyShape(drawList, ImVec2(x, centerY), 6.f, interpolation,
					bChannelEnabled ? keyColor : WithAlpha(keyColor, 0.35f), bHovered ? s_HoveredOutlineColor : s_KeyOutlineColor);

				// Label, clipped so it never runs into the next key
				const std::string label = view->GetKeyLabel(i);
				if (!label.empty())
				{
					const float labelMaxX = i + 1 < count ? TimeToX(view->GetKeyTime(i + 1)) - 8.f : areaMax.x;
					if (labelMaxX > x + 12.f)
					{
						const float textY = centerY - ImGui::GetTextLineHeight() * 0.5f;
						drawList->PushClipRect(ImVec2(x + 9.f, row.MinY), ImVec2(labelMaxX, row.MaxY), true);
						drawList->AddText(ImVec2(x + 9.f, textY), IM_COL32(220, 220, 220, 255), label.c_str());
						drawList->PopClipRect();
					}
				}

				if (bHovered)
				{
					tooltip = view->GetName() + "\nTime: " + FormatTime(view->GetKeyTime(i));
					if (!label.empty())
						tooltip += "\n" + label;
					if (view->IsInterpolationEditable())
						tooltip += std::string("\nInterpolation: ") + Utils::GetEnumName(interpolation);
				}
			}
		}

		// Playhead
		{
			const float playheadX = TimeToX(time);
			drawList->AddLine(ImVec2(playheadX, visibleTop), ImVec2(playheadX, visibleBottom), s_PlayheadColor, 2.f);
		}

		// Box selection
		if (m_DragMode == DragMode::BoxSelect)
		{
			const ImVec2 boxMin(glm::min(m_DragStartMouse.x, mouse.x), glm::min(m_DragStartMouse.y, mouse.y));
			const ImVec2 boxMax(glm::max(m_DragStartMouse.x, mouse.x), glm::max(m_DragStartMouse.y, mouse.y));
			drawList->AddRectFilled(boxMin, boxMax, IM_COL32(80, 140, 255, 40));
			drawList->AddRect(boxMin, boxMax, IM_COL32(80, 140, 255, 200));
		}

		drawList->PopClipRect();

		if (!tooltip.empty())
			ImGui::SetTooltip("%s", tooltip.c_str());

		// ---- Timeline context menu (opened in `HandleRowsMouse`) ----
		if (ImGui::BeginPopup("##TimelineContext"))
		{
			const bool bValidRow = m_ContextRow.TrackIndex < m_TrackViews.size();

			if (!m_ContextKeys.empty() && !m_Selection.empty())
			{
				ImGui::TextDisabled("%d key(s)", int(m_Selection.size()));
				ImGui::Separator();

				bool bAnyInterpolationEditable = false;
				for (const auto& ref : m_Selection)
					if (SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex); view && view->IsInterpolationEditable())
						bAnyInterpolationEditable = true;

				for (const auto& [interpolation, name] : magic_enum::enum_entries<SequenceInterpolation>())
				{
					if (!bAnyInterpolationEditable)
						break;

					bool bAllMatch = true;
					for (const auto& ref : m_Selection)
					{
						SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
						size_t index = 0;
						if (view && view->FindKeyIndex(ref.KeyID, &index) && view->GetKeyInterpolation(index) != interpolation)
						{
							bAllMatch = false;
							break;
						}
					}

					if (ImGui::MenuItem(std::string(name).c_str(), nullptr, bAllMatch))
						SetSelectedInterpolation(interpolation);
				}

				if (ImGui::MenuItem("Flatten Tangents"))
				{
					for (const auto& ref : m_Selection)
						if (SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex))
							view->FlattenTangents(ref.KeyID);
					MarkDirty();
				}
				UI::Tooltip("Only affects Cubic keys. Flat tangents ease in and out of the key");

				ImGui::Separator();
				if (ImGui::MenuItem("Copy", "Ctrl+C"))
					CopySelectedKeys();
				if (ImGui::MenuItem("Delete", "Del"))
					DeleteSelectedKeys();
			}
			else if (bValidRow)
			{
				const auto& track = m_TrackViews[m_ContextRow.TrackIndex].Track;

				if (ImGui::MenuItem("Add Key Here"))
					AddKeysAtTime(m_ContextRow.TrackIndex, m_ContextRow.ChannelIndex, m_ContextTime);

				if (track->GetType() == SequenceTrackType::Camera)
				{
					if (ImGui::MenuItem("Key Camera Here"))
						KeyCameraFromViewport(Cast<SequenceCameraTrack>(track), m_ContextTime);
					if (ImGui::MenuItem("Cut To This Camera Here"))
						AddCameraCut(m_ContextTime, track->GetID());
				}

				if (ImGui::MenuItem("Paste Here", "Ctrl+V", false, !m_Clipboard.empty()))
				{
					m_SelectedTrackID = track->GetID();
					PasteKeys(m_ContextTime);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Move Playhead Here"))
				SetPlayheadTime(m_ContextTime);

			ImGui::EndPopup();
		}

		ImGui::EndChild();

		if (!pendingDuplicate.IsNull())
			DuplicateTrack(pendingDuplicate);
		if (!pendingDelete.IsNull())
			DeleteTrack(pendingDelete);
	}

	void SceneSequenceAssetEditor::HandleRowsMouse(const ImVec2& areaMin, const ImVec2& areaMax)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 mouse = io.MousePos;
		const bool bHovered = ImGui::IsItemHovered();

		HandleZoomPan(bHovered);

		const RowLayout* hoveredRow = nullptr;
		for (const RowLayout& row : m_Rows)
		{
			if (mouse.y >= row.MinY && mouse.y < row.MaxY)
			{
				hoveredRow = &row;
				break;
			}
		}

		// Double-click on empty space adds keys
		if (bHovered && hoveredRow && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			std::vector<KeyRef> hit;
			if (!HitTestKeys(*hoveredRow, mouse.x, s_KeyHitRadius, hit))
			{
				AddKeysAtTime(hoveredRow->TrackIndex, hoveredRow->ChannelIndex, SnapTime(XToTime(mouse.x)));
				m_DragMode = DragMode::None;
				return;
			}
		}

		// ---- Press ----
		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			m_DragStartMouse = mouse;
			bDragMoved = false;
			bReduceSelectionOnRelease = false;
			m_ClickedKeys.clear();

			if (hoveredRow)
				m_SelectedTrackID = m_TrackViews[hoveredRow->TrackIndex].Track->GetID();

			if (hoveredRow && HitTestKeys(*hoveredRow, mouse.x, s_KeyHitRadius, m_ClickedKeys))
			{
				bool bAllSelected = true;
				for (const auto& ref : m_ClickedKeys)
					bAllSelected &= IsSelected(ref);

				if (io.KeyCtrl && bAllSelected)
				{
					// Ctrl+click on a selected key toggles it off
					for (const auto& ref : m_ClickedKeys)
						Deselect(ref);
					m_DragMode = DragMode::None;
				}
				else
				{
					if (!bAllSelected)
					{
						if (!io.KeyCtrl && !io.KeyShift)
							ClearSelection();
						for (const auto& ref : m_ClickedKeys)
							Select(ref, true);
					}
					else
					{
						// Clicking an already-selected key might be the start of a multi-key drag.
						// Only if the mouse is released without dragging do we reduce the selection to it.
						bReduceSelectionOnRelease = !io.KeyCtrl && !io.KeyShift;
					}

					// Every drag frame is computed from these, never from the previous frame,
					// so snapping can't accumulate rounding drift
					m_DragOriginalTimes.clear();
					for (const auto& ref : m_Selection)
					{
						SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
						size_t index = 0;
						if (view && view->FindKeyIndex(ref.KeyID, &index))
							m_DragOriginalTimes.emplace_back(ref, view->GetKeyTime(index));
					}

					m_DragAnchorTime = 0.f;
					const KeyRef& anchor = m_ClickedKeys[0];
					if (SequenceChannelView* view = FindView(anchor.TrackID, anchor.ChannelIndex))
					{
						size_t index = 0;
						if (view->FindKeyIndex(anchor.KeyID, &index))
							m_DragAnchorTime = view->GetKeyTime(index);
					}

					m_DragMode = DragMode::MoveKeys;
				}
			}
			else
			{
				if (!io.KeyCtrl && !io.KeyShift)
					ClearSelection();
				m_DragMode = DragMode::BoxSelect;
			}
		}

		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
			m_DragMode = DragMode::Pan;

		// ---- Drag ----
		if (m_DragMode == DragMode::MoveKeys && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const float dx = mouse.x - m_DragStartMouse.x;
			if (bDragMoved || glm::abs(dx) > 3.f)
			{
				bDragMoved = true;

				// Snap the key under the mouse, and move everything else by the same amount,
				// so the relative spacing of the selection is preserved exactly
				const float newAnchor = SnapTime(m_DragAnchorTime + dx / m_PixelsPerSecond);
				float delta = newAnchor - m_DragAnchorTime;

				float earliest = std::numeric_limits<float>::max();
				for (const auto& [ref, originalTime] : m_DragOriginalTimes)
					earliest = glm::min(earliest, originalTime);
				if (earliest + delta < 0.f)
					delta = -earliest;

				std::vector<SequenceChannelView*> touched;
				for (const auto& [ref, originalTime] : m_DragOriginalTimes)
				{
					SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
					if (!view)
						continue;

					view->SetKeyTime(ref.KeyID, originalTime + delta);
					if (std::find(touched.begin(), touched.end(), view) == touched.end())
						touched.push_back(view);
				}
				for (SequenceChannelView* view : touched)
					view->Sort();

				MarkDirty();
				ImGui::SetTooltip("%s  (%+.3f s)", FormatTime(m_DragAnchorTime + delta).c_str(), delta);
			}
		}

		if (m_DragMode == DragMode::Pan)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
				m_ViewStart -= io.MouseDelta.x / m_PixelsPerSecond;
			else
				m_DragMode = DragMode::None;
		}

		// ---- Release ----
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (m_DragMode == DragMode::MoveKeys)
			{
				if (!bDragMoved && bReduceSelectionOnRelease)
				{
					ClearSelection();
					for (const auto& ref : m_ClickedKeys)
						Select(ref, true);
				}

				if (bDragMoved)
					ResolveOverlapsAfterMove();

				m_DragMode = DragMode::None;
			}
			else if (m_DragMode == DragMode::BoxSelect)
			{
				const ImVec2 boxMin(glm::min(m_DragStartMouse.x, mouse.x), glm::min(m_DragStartMouse.y, mouse.y));
				const ImVec2 boxMax(glm::max(m_DragStartMouse.x, mouse.x), glm::max(m_DragStartMouse.y, mouse.y));

				if (boxMax.x - boxMin.x > 2.f || boxMax.y - boxMin.y > 2.f)
				{
					for (const RowLayout& row : m_Rows)
					{
						if (row.MaxY < boxMin.y || row.MinY > boxMax.y)
							continue;

						auto& views = m_TrackViews[row.TrackIndex];
						const GUID trackID = views.Track->GetID();

						// A summary row stands for every channel of its track
						const uint32_t firstChannel = row.ChannelIndex < 0 ? 0u : uint32_t(row.ChannelIndex);
						const uint32_t lastChannel = row.ChannelIndex < 0 ? uint32_t(views.Channels.size()) : uint32_t(row.ChannelIndex + 1);

						for (uint32_t ci = firstChannel; ci < lastChannel && ci < views.Channels.size(); ++ci)
						{
							const auto& view = views.Channels[ci];
							for (size_t i = 0; i < view->GetKeysCount(); ++i)
							{
								const float x = TimeToX(view->GetKeyTime(i));
								if (x >= boxMin.x && x <= boxMax.x)
									Select({ trackID, ci, view->GetKeyID(i) }, true);
							}
						}
					}
				}

				m_DragMode = DragMode::None;
			}
		}

		// ---- Context menu ----
		if (bHovered && hoveredRow && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
			&& io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 16.f)
		{
			m_ContextRow = *hoveredRow;
			m_ContextTime = glm::max(0.f, SnapTime(XToTime(mouse.x)));
			m_ContextKeys.clear();

			if (HitTestKeys(*hoveredRow, mouse.x, s_KeyHitRadius, m_ContextKeys))
			{
				// Right-clicking an unselected key makes it the selection, so the menu acts on it
				bool bAllSelected = true;
				for (const auto& ref : m_ContextKeys)
					bAllSelected &= IsSelected(ref);

				if (!bAllSelected)
				{
					ClearSelection();
					for (const auto& ref : m_ContextKeys)
						Select(ref, true);
				}
			}

			ImGui::OpenPopup("##TimelineContext");
		}
	}

	// ---- Details ----
	void SceneSequenceAssetEditor::DrawDetailsTab()
	{
		// Sequence-wide settings live in the toolbar; this panel is only about the selected track and keys
		DrawTrackDetails();
		DrawKeyDetails();
	}

	void SceneSequenceAssetEditor::DrawTrackDetails()
	{
		const int32_t trackIndex = FindTrackIndex(m_SelectedTrackID);
		if (trackIndex < 0)
		{
			UI::TextWithSeparator("Track");
			ImGui::TextDisabled("Select a track to edit it");
			return;
		}

		const Ref<SequenceTrack>& track = m_TrackViews[trackIndex].Track;

		if (UI::PushTreeNode("Track", true))
		{
			UI::BeginPropertyGrid("SceneSequenceTrackSettings");

			UI::Text("Type", Utils::GetEnumName(track->GetType()));

			std::string name = track->GetName();
			if (UI::PropertyText("Name", name))
			{
				track->SetName(name);
				MarkDirty();
			}

			bool bEnabled = track->IsEnabled();
			if (UI::Property("Enabled", bEnabled))
			{
				track->SetEnabled(bEnabled);
				MarkDirty();
			}
			UI::EndPropertyGrid();

			UI::PopTreeNode();
		}

		switch (track->GetType())
		{
			case SequenceTrackType::Camera:
			{
				SequenceCameraTrack* camera = (SequenceCameraTrack*)track.get();

				UI::BeginPropertyGrid("SceneSequenceTrackSettings");

				float fov = camera->GetDefaultFOVDegrees();
				if (UI::PropertyDrag("Default FOV", fov, 0.1f, 1.f, 170.f, "Vertical field of view in degrees, used while the 'Field Of View' channel has no keys"))
				{
					camera->SetDefaultFOVDegrees(glm::clamp(fov, 1.f, 170.f));
					MarkDirty();
				}

				float nearClip = camera->GetNearClip();
				if (UI::PropertyDrag("Near Clip", nearClip, 0.01f, 0.001f, 1000.f))
				{
					camera->SetNearClip(nearClip);
					MarkDirty();
				}

				float farClip = camera->GetFarClip();
				if (UI::PropertyDrag("Far Clip", farClip, 1.f, 0.01f, 100000.f))
				{
					camera->SetFarClip(farClip);
					MarkDirty();
				}
				UI::EndPropertyGrid();
				break;
			}
			case SequenceTrackType::PostProcess:
			{
				SequencePostProcessTrack* postProcess = (SequencePostProcessTrack*)track.get();

				// Which camera this grade belongs to. "Any Camera" makes it a sequence-wide grade
				const GUID& linkedCamera = postProcess->GetCameraTrackID();
				std::string cameraLabel = "Any Camera";
				for (const auto& cameraTrack : m_Asset->GetCameraTracks())
					if (cameraTrack && cameraTrack->GetID() == linkedCamera)
						cameraLabel = cameraTrack->GetName();

				UI::BeginPropertyGrid("SceneSequenceTrackSettings");

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Camera");
				ImGui::SameLine();
				UI::HelpMarker("Applies only while the chosen camera is live. 'Any Camera' applies whenever the track is in range");
				ImGui::NextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##PostProcessCamera", cameraLabel.c_str()))
				{
					if (ImGui::Selectable("Any Camera", linkedCamera.IsNull()))
					{
						postProcess->SetCameraTrackID(GUID(0, 0));
						MarkDirty();
					}
					for (const auto& cameraTrack : m_Asset->GetCameraTracks())
					{
						if (!cameraTrack)
							continue;

						ImGui::PushID((void*)cameraTrack->GetID().GetHash());
						if (ImGui::Selectable(cameraTrack->GetName().c_str(), cameraTrack->GetID() == linkedCamera))
						{
							postProcess->SetCameraTrackID(cameraTrack->GetID());
							MarkDirty();
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();

				bool bWholeSequence = postProcess->DoesApplyWholeSequence();
				if (UI::Property("Apply Whole Sequence", bWholeSequence,
					"Off: the track applies between its first and last key, and leaving it restores the scene's settings.\n"
					"On: it applies for the whole sequence, holding the first/last keyed values outside that range"))
				{
					postProcess->SetApplyWholeSequence(bWholeSequence);
					MarkDirty();
				}

				UI::EndPropertyGrid();

				ImGui::Separator();
				if (ImGui::Button("Add Property..."))
					ImGui::OpenPopup("##AddPostProcessProperty");

				if (ImGui::BeginPopup("##AddPostProcessProperty"))
				{
					DrawAddPostProcessPropertyMenu(track->GetID(), SnapTime(m_Player.GetTime()));
					ImGui::EndPopup();
				}

				ImGui::Separator();
				if (UI::PushTreeNode("Properties", false, true, "Only these rendering settings are overridden. Everything else keeps the scene's own values"))
				{
					// Enable/disable each property. Unticking one leaves that setting to the scene,
					// without losing its keys
					UI::BeginPropertyGrid("SceneSequenceTrackSettings");
					for (auto& channel : postProcess->GetChannels())
					{
						const PostProcessPropertyInfo* info = FindPostProcessProperty(channel.Property);
						ImGui::PushID(int(channel.Property));
						if (UI::Property(GetPostProcessPropertyLabel(info), channel.bEnabled, info ? info->Tooltip : ""))
							MarkDirty();
						ImGui::PopID();
					}
					UI::EndPropertyGrid();

					UI::PopTreeNode();
				}
				ImGui::Separator();
				break;
			}
			case SequenceTrackType::CameraCuts:
			{
				UI::BeginPropertyGrid("SceneSequenceTrackSettings");
				UI::Text("Cuts", std::to_string(((const SequenceCameraCutTrack*)track.get())->GetCutsChannel().GetKeysCount()),
					"Each key switches the scene to the camera it names, from its time until the next key. "
					"Before the first cut, the first cut's camera is live. "
					"Without cuts (or with this track disabled) the camera is chosen automatically: the one whose keys cover the current time");
				UI::EndPropertyGrid();
				break;
			}
			case SequenceTrackType::Event:
			{
				const SequenceEventTrack* events = (const SequenceEventTrack*)track.get();
				UI::BeginPropertyGrid("SceneSequenceTrackSettings");
				UI::Text("Events", std::to_string(events->GetEventsChannel().GetKeysCount()),
					"Each key fires C# `Entity.OnSequenceEvent(name, time)` on the entity playing this sequence, as the playhead passes it");
				UI::EndPropertyGrid();
				break;
			}
		}
	}

	void SceneSequenceAssetEditor::DrawKeyDetails()
	{
		if (m_Selection.empty())
		{
			if (UI::PushTreeNode("Key", true))
			{
				ImGui::TextDisabled("Select a key in the timeline to edit it");
				UI::PopTreeNode();
			}
			return;
		}

		if (m_Selection.size() == 1)
		{
			const KeyRef ref = m_Selection[0];
			SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
			size_t index = 0;
			if (!view || !view->FindKeyIndex(ref.KeyID, &index))
				return;

			if (UI::PushTreeNode("Key", true))
			{
				UI::BeginPropertyGrid("SceneSequenceKey");

				const int32_t trackIndex = FindTrackIndex(ref.TrackID);
				const std::string channelLabel = (trackIndex >= 0 ? m_TrackViews[trackIndex].Track->GetName() : std::string("?")) + " / " + view->GetName();
				UI::Text("Channel", channelLabel);

				float keyTime = view->GetKeyTime(index);
				if (UI::PropertyDrag("Time", keyTime, GetFrameDuration(), 0.f, 3600.f, "In seconds"))
				{
					view->SetKeyTime(ref.KeyID, SnapTime(keyTime));
					view->Sort();
					ResolveOverlapsAfterMove();
					MarkDirty();
				}

				SequenceInterpolation interpolation = view->GetKeyInterpolation(index);
				if (view->IsInterpolationEditable() && UI::ComboEnum("Interpolation", interpolation,
					"How the curve travels from this key to the next one.\n"
					"Constant: hold the value. Linear: straight line.\n"
					"Smooth: automatic tangents. Cubic: editable tangents (see the Curves tab)"))
				{
					view->SetKeyInterpolation(ref.KeyID, interpolation);
					MarkDirty();
				}

				if (view->DrawKeyValue(ref.KeyID))
					MarkDirty();

				UI::EndPropertyGrid();

				UI::PopTreeNode();
			}
			return;
		}

		// Multi-selection: operations that make sense in bulk
		if (UI::PushTreeNode("Multiple Keys: " + std::to_string(m_Selection.size()), true))
		{
			UI::BeginPropertyGrid("SceneSequenceKeys");

			SequenceInterpolation interpolation = SequenceInterpolation::Smooth;
			bool bMixed = false;
			bool bFirst = true;
			for (const auto& ref : m_Selection)
			{
				SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
				size_t index = 0;
				if (!view || !view->FindKeyIndex(ref.KeyID, &index))
					continue;

				if (bFirst)
				{
					interpolation = view->GetKeyInterpolation(index);
					bFirst = false;
				}
				else if (view->GetKeyInterpolation(index) != interpolation)
				{
					bMixed = true;
				}
			}

			if (UI::ComboEnum("Interpolation", interpolation, bMixed ? "Selected keys use different modes. Picking one applies it to all" : ""))
				SetSelectedInterpolation(interpolation);

			// Works like a jog wheel: the value always reads 0 and dragging nudges every selected key
			float shift = 0.f;
			if (UI::PropertyDrag("Shift Time", shift, GetFrameDuration(), 0.f, 0.f, "Drag to move all selected keys in time"))
			{
				float earliest = std::numeric_limits<float>::max();
				for (const auto& ref : m_Selection)
				{
					SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
					size_t index = 0;
					if (view && view->FindKeyIndex(ref.KeyID, &index))
						earliest = glm::min(earliest, view->GetKeyTime(index));
				}
				if (earliest + shift < 0.f)
					shift = -earliest;

				for (const auto& ref : m_Selection)
				{
					SequenceChannelView* view = FindView(ref.TrackID, ref.ChannelIndex);
					size_t index = 0;
					if (view && view->FindKeyIndex(ref.KeyID, &index))
					{
						view->SetKeyTime(ref.KeyID, view->GetKeyTime(index) + shift);
						view->Sort();
					}
				}
				ResolveOverlapsAfterMove();
				MarkDirty();
			}

			UI::EndPropertyGrid();

			UI::PopTreeNode();
		}
	}

	// ---- Curves ----
	void SceneSequenceAssetEditor::DrawCurvesTab()
	{
		const int32_t trackIndex = FindTrackIndex(m_SelectedTrackID);
		if (trackIndex < 0)
		{
			ImGui::TextDisabled("Select a track to edit its curves");
			return;
		}

		auto& views = m_TrackViews[trackIndex];
		const GUID trackID = views.Track->GetID();

		if (m_CurvesTrackID != trackID)
		{
			m_CurvesTrackID = trackID;
			m_CurveComponentVisible.clear();
			bFitCurvesRequested = true;
		}

		struct CurveEntry
		{
			uint32_t Channel = 0;
			uint32_t Component = 0;
			ImVec4 Color;
			std::string Label;
		};

		static const ImVec4 s_AxisColors[] = { ImVec4(0.95f, 0.35f, 0.35f, 1.f), ImVec4(0.4f, 0.9f, 0.4f, 1.f), ImVec4(0.4f, 0.55f, 1.f, 1.f) };

		std::vector<CurveEntry> curves;
		for (uint32_t ci = 0; ci < uint32_t(views.Channels.size()); ++ci)
		{
			const auto& view = views.Channels[ci];
			const uint32_t count = view->GetCurveComponentsCount();
			for (uint32_t c = 0; c < count; ++c)
			{
				CurveEntry& entry = curves.emplace_back();
				entry.Channel = ci;
				entry.Component = c;
				entry.Color = count == 3 ? s_AxisColors[c] : ImGui::ColorConvertU32ToFloat4(view->GetColor());
				entry.Label = count > 1 ? view->GetName() + "." + view->GetCurveComponentName(c) : view->GetName();
			}
		}

		if (curves.empty())
		{
			ImGui::TextDisabled("This track has no channels that can be shown as curves");
			return;
		}

		if (m_CurveComponentVisible.size() != curves.size())
			m_CurveComponentVisible.assign(curves.size(), true);

		const ImGuiStyle& style = ImGui::GetStyle();
		const float panelRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		for (size_t i = 0; i < curves.size(); ++i)
		{
			// Flow the checkboxes like text: stay on the line while they fit, wrap otherwise
			if (i > 0)
			{
				const float checkboxWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(curves[i].Label.c_str()).x;
				const float lastItemRight = ImGui::GetItemRectMax().x;
				if (lastItemRight + style.ItemSpacing.x + checkboxWidth <= panelRight)
					ImGui::SameLine();
			}

			ImGui::PushStyleColor(ImGuiCol_CheckMark, curves[i].Color);
			bool bVisible = m_CurveComponentVisible[i];
			if (ImGui::Checkbox(curves[i].Label.c_str(), &bVisible))
				m_CurveComponentVisible[i] = bVisible;
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();
		if (ImGui::Button("Fit"))
			bFitCurvesRequested = true;

		ImGui::SameLine();
		UI::HelpMarker("Drag points to change a key's time and value. Keys can't be dragged past their neighbours here; use the timeline for that.\n"
			"Select a key and set it to 'Cubic' to get tangent handles.\n"
			"Rotation is edited in the Details tab.\n"
			"Double-click the plot to fit it, drag the red line to scrub");

		const ImPlotFlags plotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus;
		if (!ImPlot::BeginPlot("##SequenceCurves", ImGui::GetContentRegionAvail(), plotFlags))
			return;

		ImPlot::SetupAxes("Time (s)", nullptr);

		if (bFitCurvesRequested)
		{
			bFitCurvesRequested = false;

			const float timeEnd = glm::max(0.1f, glm::max(m_Asset->GetDuration(), GetLastKeyTime()));
			float minValue = std::numeric_limits<float>::max();
			float maxValue = std::numeric_limits<float>::lowest();

			for (size_t i = 0; i < curves.size(); ++i)
			{
				if (!m_CurveComponentVisible[i])
					continue;

				const auto& view = views.Channels[curves[i].Channel];
				constexpr int fitSamples = 64;
				for (int s = 0; s < fitSamples; ++s)
				{
					const float value = view->EvaluateComponent(timeEnd * float(s) / float(fitSamples - 1), curves[i].Component);
					minValue = glm::min(minValue, value);
					maxValue = glm::max(maxValue, value);
				}
			}

			if (minValue > maxValue)
			{
				minValue = -1.f;
				maxValue = 1.f;
			}
			const float padding = glm::max(0.1f, (maxValue - minValue) * 0.1f);
			ImPlot::SetupAxesLimits(-timeEnd * 0.02, timeEnd * 1.02, minValue - padding, maxValue + padding, ImPlotCond_Always);
		}

		const ImPlotRect limits = ImPlot::GetPlotLimits();
		const double xMin = limits.X.Min;
		const double xMax = limits.X.Max;

		// Curves, sampled across whatever part of the timeline is visible
		constexpr int sampleCount = 300;
		std::vector<glm::dvec2> points(sampleCount);
		for (size_t i = 0; i < curves.size(); ++i)
		{
			if (!m_CurveComponentVisible[i])
				continue;

			const auto& entry = curves[i];
			const auto& view = views.Channels[entry.Channel];
			for (int s = 0; s < sampleCount; ++s)
			{
				const double t = xMin + (xMax - xMin) * double(s) / double(sampleCount - 1);
				points[s] = glm::dvec2(t, double(view->EvaluateComponent(float(glm::max(t, 0.0)), entry.Component)));
			}

			ImPlotSpec spec{};
			spec.Stride = sizeof(glm::dvec2);
			spec.LineColor = entry.Color;
			spec.LineWeight = 2.f;
			ImPlot::PlotLine(entry.Label.c_str(), &points[0].x, &points[0].y, sampleCount, spec);
		}

		// Key points and tangent handles.
		// Time changes are applied after the loop so indices stay valid while iterating
		struct PendingTime
		{
			SequenceChannelView* View = nullptr;
			GUID KeyID = GUID(0, 0);
			float Time = 0.f;
		};
		std::vector<PendingTime> pendingTimes;

		const ImVec4 selectedColor = ImVec4(1.f, 0.8f, 0.25f, 1.f);
		const ImVec4 handleColor = ImVec4(0.9f, 0.9f, 0.9f, 1.f);
		const double handleLength = (xMax - xMin) * 0.06;
		int dragID = 0;

		for (size_t ei = 0; ei < curves.size(); ++ei)
		{
			if (!m_CurveComponentVisible[ei])
				continue;

			const auto& entry = curves[ei];
			SequenceChannelView* view = views.Channels[entry.Channel].get();
			const size_t count = view->GetKeysCount();

			for (size_t i = 0; i < count; ++i)
			{
				const GUID keyID = view->GetKeyID(i);
				const KeyRef ref{ trackID, entry.Channel, keyID };
				const bool bSelected = IsSelected(ref);

				const double keyTime = view->GetKeyTime(i);
				const double keyValue = view->GetKeyComponent(i, entry.Component);

				double x = keyTime;
				double y = keyValue;
				bool bClicked = false;
				bool bHovered = false;
				bool bHeld = false;
				if (ImPlot::DragPoint(dragID++, &x, &y, bSelected ? selectedColor : entry.Color, bSelected ? 6.f : 4.5f,
					ImPlotDragToolFlags_None, &bClicked, &bHovered, &bHeld))
				{
					// Clamp between the neighbours: letting a key overtake another here would reorder
					// the channel mid-drag, and ImPlot would then hand the drag to a different point
					const float lowerBound = i > 0 ? view->GetKeyTime(i - 1) + 1e-3f : 0.f;
					const float upperBound = i + 1 < count ? view->GetKeyTime(i + 1) - 1e-3f : std::numeric_limits<float>::max();
					float newTime = SnapTime(float(x));
					newTime = lowerBound <= upperBound ? glm::clamp(newTime, lowerBound, upperBound) : float(keyTime);

					view->SetKeyComponent(keyID, entry.Component, float(y));
					if (glm::abs(double(newTime) - keyTime) > 1e-6)
						pendingTimes.push_back({ view, keyID, newTime });

					MarkDirty();
				}

				if (bClicked)
					Select(ref, ImGui::GetIO().KeyCtrl);

				if (bHovered)
					ImGui::SetTooltip("%s\nTime: %s\nValue: %.3f", entry.Label.c_str(), FormatTime(float(keyTime)).c_str(), keyValue);

				// Tangent handles for the selected Cubic keys. The handle has a fixed on-screen time
				// length; what the user edits is its slope
				if (bSelected && view->GetKeyInterpolation(i) == SequenceInterpolation::Cubic)
				{
					for (int side = 0; side < 2; ++side)
					{
						const bool bOut = side == 1;
						const double direction = bOut ? 1.0 : -1.0;
						const double tangent = view->GetKeyTangent(i, entry.Component, bOut);

						double handleX = keyTime + direction * handleLength;
						double handleY = keyValue + direction * handleLength * tangent;

						glm::dvec2 line[2] = { glm::dvec2(keyTime, keyValue), glm::dvec2(handleX, handleY) };
						ImPlotSpec handleSpec{};
						handleSpec.Stride = sizeof(glm::dvec2);
						handleSpec.LineColor = ImVec4(0.9f, 0.9f, 0.9f, 0.7f);
						handleSpec.LineWeight = 1.f;
						ImPlot::PlotLine("##Tangent", &line[0].x, &line[0].y, 2, handleSpec);

						if (ImPlot::DragPoint(dragID++, &handleX, &handleY, handleColor, 3.5f, ImPlotDragToolFlags_None, nullptr, nullptr, nullptr))
						{
							// Only accept the handle on its own side of the key, otherwise the slope flips sign
							const double dx = handleX - keyTime;
							if ((bOut && dx > 1e-6) || (!bOut && dx < -1e-6))
							{
								view->SetKeyTangent(keyID, entry.Component, bOut, float((handleY - keyValue) / dx));
								MarkDirty();
							}
						}
					}
				}
			}
		}

		for (const auto& pending : pendingTimes)
		{
			pending.View->SetKeyTime(pending.KeyID, pending.Time);
			pending.View->Sort();
		}

		// Playhead
		double playhead = m_Player.GetTime();
		if (ImPlot::DragLineX(1 << 20, &playhead, ImVec4(0.9f, 0.25f, 0.25f, 1.f), 1.5f))
			SetPlayheadTime(SnapTime(float(playhead)));

		ImPlot::EndPlot();
	}
}
