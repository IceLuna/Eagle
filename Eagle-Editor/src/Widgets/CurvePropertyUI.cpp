#include "egpch.h"
#include "CurvePropertyUI.h"

#include "CurveView.h"
#include "GradientEditor.h"

namespace Eagle::CurveUI
{
	namespace
	{
		// Same as `CurveAccess`, but a bool is shown as on (1) / off (0).
		// `CurveAccess<bool>` has no components, since the curve editor hides bools unless asked to (see `OnOffCurveView`)
		template <typename T>
		struct PreviewAccess : CurveAccess<T> {};

		template <>
		struct PreviewAccess<bool>
		{
			static constexpr uint32_t Count = 1;
			static float Get(bool value, uint32_t) { return value ? 1.f : 0.f; }
		};

		constexpr static float s_ModeComboWidth = 84.f;
		constexpr static ImU32 s_ActiveOutlineColor = IM_COL32(255, 200, 60, 255);
		constexpr static ImU32 s_ComponentColors[] = { IM_COL32(242, 90, 90, 255), IM_COL32(100, 230, 100, 255), IM_COL32(100, 140, 255, 255), IM_COL32(230, 230, 230, 255) };

		void BeginRow(std::string_view label, std::string_view helpMessage)
		{
			ImGui::PushID(label.data(), label.data() + label.size());
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::TextUnformatted(label.data(), label.data() + label.size());
			if (!helpMessage.empty())
			{
				ImGui::SameLine();
				UI::HelpMarker(helpMessage);
			}
			ImGui::NextColumn();
		}

		void EndRow()
		{
			ImGui::NextColumn();
			ImGui::PopID();
		}

		template <typename T>
		bool DrawModeCombo(CurveProperty<T>& property, const char* curveName)
		{
			const char* modes[] = { "Constant", curveName };
			int mode = int(property.Mode);
			ImGui::SetNextItemWidth(s_ModeComboWidth);
			bool bChanged = false;
			if (ImGui::Combo("##Mode", &mode, modes, IM_ARRAYSIZE(modes)))
			{
				property.SetMode(CurvePropertyMode(mode));
				bChanged = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Constant: the same value for the whole lifetime\n%s: the value changes over the lifetime", curveName);
			ImGui::SameLine();
			return bChanged;
		}

		bool DrawConstant(float& value, const CurvePropertyParams& params)
		{
			ImGui::SetNextItemWidth(-1.f);
			return ImGui::DragFloat("##Value", &value, params.Speed, params.Min, params.Max, "%.3f");
		}

		bool DrawConstant(glm::vec2& value, const CurvePropertyParams& params)
		{
			ImGui::SetNextItemWidth(-1.f);
			return ImGui::DragFloat2("##Value", &value.x, params.Speed, params.Min, params.Max, "%.3f");
		}

		bool DrawConstant(bool& value, const CurvePropertyParams&)
		{
			return ImGui::Checkbox("##Value", &value);
		}

		bool DrawConstant(glm::vec3& value, const CurvePropertyParams& params)
		{
			ImGui::SetNextItemWidth(-1.f);
			return ImGui::DragFloat3("##Value", &value.x, params.Speed, params.Min, params.Max, "%.3f");
		}

		// Frame of a clickable preview. Returns true if it was clicked
		bool PreviewButton(ImVec2& outMin, ImVec2& outMax)
		{
			const ImVec2 size(glm::max(ImGui::GetContentRegionAvail().x, 20.f), ImGui::GetFrameHeight());
			outMin = ImGui::GetCursorScreenPos();
			outMax = ImVec2(outMin.x + size.x, outMin.y + size.y);

			const bool bClicked = ImGui::InvisibleButton("##Preview", size);
			const bool bHovered = ImGui::IsItemHovered();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(outMin, outMax, ImGui::GetColorU32(bHovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
			return bClicked;
		}

		void PreviewOutline(const ImVec2& min, const ImVec2& max, bool bActive)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			if (bActive)
				drawList->AddRect(min, max, s_ActiveOutlineColor, ImGui::GetStyle().FrameRounding, 0, 2.f);
		}

		// Every component as a polyline, all sharing one vertical range so their relative values stay readable
		template <typename T>
		bool DrawCurvePreview(const CurveProperty<T>& property, bool bActive)
		{
			using Access = PreviewAccess<T>;

			ImVec2 min, max;
			const bool bClicked = PreviewButton(min, max);

			constexpr int samples = 48;
			float values[Access::Count][samples];
			float minValue = std::numeric_limits<float>::max();
			float maxValue = std::numeric_limits<float>::lowest();
			for (int s = 0; s < samples; ++s)
			{
				const T value = property.Evaluate(float(s) / float(samples - 1));
				for (uint32_t c = 0; c < Access::Count; ++c)
				{
					values[c][s] = Access::Get(value, c);
					minValue = glm::min(minValue, values[c][s]);
					maxValue = glm::max(maxValue, values[c][s]);
				}
			}
			if (maxValue - minValue < 1e-4f)
			{
				minValue -= 1.f;
				maxValue += 1.f;
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const float padding = 3.f;
			const ImVec2 innerMin(min.x + padding, min.y + padding);
			const ImVec2 innerMax(max.x - padding, max.y - padding);
			ImVec2 points[samples];
			for (uint32_t c = 0; c < Access::Count; ++c)
			{
				for (int s = 0; s < samples; ++s)
				{
					const float x = innerMin.x + (innerMax.x - innerMin.x) * float(s) / float(samples - 1);
					const float y = innerMax.y - (innerMax.y - innerMin.y) * (values[c][s] - minValue) / (maxValue - minValue);
					points[s] = ImVec2(x, y);
				}
				const ImU32 color = Access::Count == 1 ? s_ComponentColors[3] : s_ComponentColors[c % 4];
				drawList->AddPolyline(points, samples, color, 0, 1.5f);
			}
			PreviewOutline(min, max, bActive);

			if (ImGui::IsItemHovered())
			{
				const T start = property.Evaluate(0.f);
				const T end = property.Evaluate(1.f);
				std::string startText, endText;
				for (uint32_t c = 0; c < Access::Count; ++c)
				{
					char buffer[32];
					snprintf(buffer, sizeof(buffer), c == 0 ? "%.2f" : ", %.2f", Access::Get(start, c));
					startText += buffer;
					snprintf(buffer, sizeof(buffer), c == 0 ? "%.2f" : ", %.2f", Access::Get(end, c));
					endText += buffer;
				}
				ImGui::SetTooltip("Click to edit the curve below\nStart: %s\nEnd: %s\nKeys: %zu", startText.c_str(), endText.c_str(), property.Keys.GetKeysCount());
			}
			return bClicked;
		}

		template <typename T>
		bool PropertyCurve_Internal(std::string_view label, CurveProperty<T>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked)
		{
			BeginRow(label, params.HelpMessage);

			bool bChanged = DrawModeCombo(property, "Curve");
			if (property.Mode == CurvePropertyMode::Constant)
			{
				bChanged |= DrawConstant(property.Constant, params);
			}
			else if (DrawCurvePreview(property, bActive))
			{
				bOutEditClicked = true;
			}

			if (bChanged && property.Mode == CurvePropertyMode::Curve)
				bOutEditClicked = true;

			EndRow();
			return bChanged;
		}
	}

	bool PropertyCurve(std::string_view label, CurveProperty<float>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked)
	{
		return PropertyCurve_Internal(label, property, params, bActive, bOutEditClicked);
	}

	bool PropertyCurve(std::string_view label, CurveProperty<glm::vec2>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked)
	{
		return PropertyCurve_Internal(label, property, params, bActive, bOutEditClicked);
	}

	bool PropertyCurve(std::string_view label, CurveProperty<glm::vec3>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked)
	{
		return PropertyCurve_Internal(label, property, params, bActive, bOutEditClicked);
	}

	bool PropertyCurve(std::string_view label, CurveProperty<bool>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked)
	{
		return PropertyCurve_Internal(label, property, params, bActive, bOutEditClicked);
	}

	bool PropertyGradient(std::string_view label, CurveProperty<glm::vec4>& property, std::string_view helpMessage, bool bActive, bool& bOutEditClicked)
	{
		BeginRow(label, helpMessage);

		bool bChanged = DrawModeCombo(property, "Gradient");
		if (property.Mode == CurvePropertyMode::Constant)
		{
			ImGui::SetNextItemWidth(-1.f);
			bChanged |= ImGui::ColorEdit4("##Value", &property.Constant.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
		}
		else
		{
			ImVec2 min, max;
			if (PreviewButton(min, max))
				bOutEditClicked = true;
			const bool bHovered = ImGui::IsItemHovered();

			GradientEditor::DrawGradient(ImGui::GetWindowDrawList(), property.Keys, min, max, property.Constant);
			PreviewOutline(min, max, bActive);
			if (bHovered)
				ImGui::SetTooltip("Click to edit the gradient below");
		}

		if (bChanged && property.Mode == CurvePropertyMode::Curve)
			bOutEditClicked = true;

		EndRow();
		return bChanged;
	}
}
