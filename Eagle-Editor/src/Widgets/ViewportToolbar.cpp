#include "ViewportToolbar.h"

#include "Eagle/Camera/EditorCamera.h"
#include "Eagle/Input/Input.h"

#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Eagle
{
	namespace
	{
		enum class ToolbarIcon
		{
			Select, Translate, Rotate, Scale, World, Local, SnapTranslation, SnapRotation, SnapScale, Camera, Show, ViewMode
		};

		struct ToolbarStyle
		{
			float Height = 0.f; // Of the buttons
			float IconSize = 0.f;
			float PaddingX = 0.f;
			float ArrowSize = 0.f;
			float Rounding = 0.f;
			float Spacing = 0.f; // Between groups, and from the edges of the viewport
			ImVec2 PopupPadding;
			ImVec2 PopupItemSpacing;
			ImU32 Background = 0;
			ImU32 Hovered = 0;
			ImU32 Pressed = 0;
			ImU32 Selected = 0;
			ImU32 SelectedHovered = 0;
			ImU32 Content = 0;
			ImU32 ContentDisabled = 0;
		};
		
		struct ToolbarItem
		{
			const char* ID = "";
			std::optional<ToolbarIcon> Icon;
			std::string Text;
			const char* Tooltip = nullptr;
			bool bDropdown = false;
			bool bSelected = false;
			bool bEnabled = true;

			std::function<void()> OnClick;

			// A dropdown opens a popup below the button
			const char* PopupID = nullptr;
			std::function<void()> PopupContent;
			bool bAlignPopupRight = false;

			// Set by `DrawGroup`
			float Width = 0.f;
			ImRect Rect;
		};

		struct ToolbarGroup
		{
			std::vector<ToolbarItem> Items;
			float Width = 0.f;
		};

		ToolbarStyle GetToolbarStyle()
		{
			const ImGuiStyle& imguiStyle = ImGui::GetStyle();
			ToolbarStyle style;
			style.Height = IM_ROUND(ImGui::GetFrameHeight() * 1.15f);
			style.IconSize = IM_ROUND(style.Height * 0.62f);
			style.PaddingX = IM_ROUND(style.Height * 0.3f);
			style.ArrowSize = IM_ROUND(style.Height * 0.3f);
			style.Rounding = ImMin(imguiStyle.FrameRounding, style.Height * 0.5f);
			style.Spacing = IM_ROUND(style.Height * 0.3f);

			// The padding of the popups exceeds their rounding, so that the content (and the highlight of the hovered item) stays clear of the corners
			const float fontSize = ImGui::GetFontSize();
			style.PopupPadding = ImVec2(IM_ROUND(ImMax(imguiStyle.WindowPadding.x, imguiStyle.PopupRounding) + fontSize * 0.4f),
				IM_ROUND(ImMax(imguiStyle.WindowPadding.y, imguiStyle.PopupRounding * 0.75f) + fontSize * 0.2f));
			style.PopupItemSpacing = ImVec2(imguiStyle.ItemSpacing.x, ImMax(imguiStyle.ItemSpacing.y, IM_ROUND(fontSize * 0.35f)));

			// Readable over any image
			style.Background = IM_COL32(18, 18, 20, 200);
			style.Hovered = IM_COL32(255, 255, 255, 34);
			style.Pressed = IM_COL32(255, 255, 255, 18);
			ImVec4 accent = imguiStyle.Colors[ImGuiCol_CheckMark];
			accent.w = 0.85f;
			style.Selected = ImGui::ColorConvertFloat4ToU32(accent);
			accent.w = 1.f;
			style.SelectedHovered = ImGui::ColorConvertFloat4ToU32(accent);
			style.Content = IM_COL32(235, 235, 235, 255);
			style.ContentDisabled = IM_COL32(235, 235, 235, 90);
			return style;
		}

		void DrawArrowHead(ImDrawList* drawList, const ImVec2& tip, const ImVec2& dir, float length, float halfWidth, ImU32 color)
		{
			const ImVec2 perpendicular(-dir.y, dir.x);
			const ImVec2 base(tip.x - dir.x * length, tip.y - dir.y * length);
			drawList->AddTriangleFilled(tip, ImVec2(base.x + perpendicular.x * halfWidth, base.y + perpendicular.y * halfWidth),
				ImVec2(base.x - perpendicular.x * halfWidth, base.y - perpendicular.y * halfWidth), color);
		}

		void DrawIcon(ImDrawList* drawList, ToolbarIcon icon, const ImVec2& c, float size, ImU32 color)
		{
			const float r = size * 0.5f;
			const float thickness = ImMax(1.f, size / 11.f);

			switch (icon)
			{
				case ToolbarIcon::Select:
				{
					// A mouse cursor, designed on a 12x18 grid
					const float k = size / 18.f;
					const ImVec2 o(c.x - 5.5f * k, c.y - 9.f * k);
					auto point = [&](float x, float y) { return ImVec2(o.x + x * k, o.y + y * k); };
					const ImVec2 points[] =
					{
						point(0.f, 0.f), point(0.f, 15.f), point(3.8f, 11.6f), point(6.4f, 17.6f),
						point(8.8f, 16.6f), point(6.2f, 10.8f), point(11.f, 10.8f)
					};
					drawList->AddConcavePolyFilled(points, IM_ARRAYSIZE(points), color);
					break;
				}
				case ToolbarIcon::Translate:
				{
					// Arrows in four directions
					const float arm = r * 0.95f;
					const float head = r * 0.42f;
					drawList->AddLine(ImVec2(c.x - arm + head, c.y), ImVec2(c.x + arm - head, c.y), color, thickness);
					drawList->AddLine(ImVec2(c.x, c.y - arm + head), ImVec2(c.x, c.y + arm - head), color, thickness);
					DrawArrowHead(drawList, ImVec2(c.x + arm, c.y), ImVec2(1.f, 0.f), head, head * 0.62f, color);
					DrawArrowHead(drawList, ImVec2(c.x - arm, c.y), ImVec2(-1.f, 0.f), head, head * 0.62f, color);
					DrawArrowHead(drawList, ImVec2(c.x, c.y + arm), ImVec2(0.f, 1.f), head, head * 0.62f, color);
					DrawArrowHead(drawList, ImVec2(c.x, c.y - arm), ImVec2(0.f, -1.f), head, head * 0.62f, color);
					break;
				}
				case ToolbarIcon::Rotate:
				{
					// A circular arrow
					const float radius = r * 0.72f;
					const float angleMin = IM_PI * 0.25f;
					const float angleMax = IM_PI * 1.8f;
					drawList->PathArcTo(c, radius, angleMin, angleMax, 24);
					drawList->PathStroke(color, ImDrawFlags_None, thickness * 1.2f);

					const ImVec2 end(c.x + cosf(angleMax) * radius, c.y + sinf(angleMax) * radius);
					const ImVec2 tangent(-sinf(angleMax), cosf(angleMax));
					const float head = r * 0.7f;
					DrawArrowHead(drawList, ImVec2(end.x + tangent.x * head * 0.55f, end.y + tangent.y * head * 0.55f), tangent, head, head * 0.62f, color);
					break;
				}
				case ToolbarIcon::Scale:
				{
					// A box with the original box in its corner, and an arrow that scales it
					const float a = r * 0.85f;
					const float corner = a * 0.8f;
					drawList->AddRect(ImVec2(c.x - a, c.y - a), ImVec2(c.x + a, c.y + a), color, 0.f, ImDrawFlags_None, thickness);
					drawList->AddRectFilled(ImVec2(c.x - a, c.y + a - corner), ImVec2(c.x - a + corner, c.y + a), color);

					const ImVec2 dir(0.7071f, -0.7071f);
					const ImVec2 to(c.x + a * 0.55f, c.y - a * 0.55f);
					const float head = r * 0.5f;
					drawList->AddLine(ImVec2(c.x - a + corner, c.y + a - corner), ImVec2(to.x - dir.x * head * 0.5f, to.y - dir.y * head * 0.5f), color, thickness);
					DrawArrowHead(drawList, to, dir, head, head * 0.55f, color);
					break;
				}
				case ToolbarIcon::World:
				{
					// A globe
					const float radius = r * 0.88f;
					drawList->AddCircle(c, radius, color, 0, thickness);
					drawList->AddEllipse(c, ImVec2(radius * 0.42f, radius), color, 0.f, 0, thickness);
					drawList->AddLine(ImVec2(c.x - radius, c.y), ImVec2(c.x + radius, c.y), color, thickness);
					break;
				}
				case ToolbarIcon::Local:
				{
					// A cube
					const float a = r * 0.9f;
					const float dx = a * 0.866f;
					const float dy = a * 0.5f;
					const ImVec2 upperLeft(c.x - dx, c.y - dy);
					const ImVec2 upperRight(c.x + dx, c.y - dy);
					const ImVec2 bottom(c.x, c.y + a);
					const ImVec2 outline[] = { ImVec2(c.x, c.y - a), upperRight, ImVec2(c.x + dx, c.y + dy), bottom, ImVec2(c.x - dx, c.y + dy), upperLeft };
					drawList->AddPolyline(outline, IM_ARRAYSIZE(outline), color, ImDrawFlags_Closed, thickness);
					drawList->AddLine(upperLeft, c, color, thickness);
					drawList->AddLine(upperRight, c, color, thickness);
					drawList->AddLine(c, bottom, color, thickness);
					break;
				}
				case ToolbarIcon::SnapTranslation:
				{
					// A grid
					const float a = r * 0.9f;
					for (float offset : { -a / 3.f, a / 3.f })
					{
						drawList->AddLine(ImVec2(c.x + offset, c.y - a), ImVec2(c.x + offset, c.y + a), color, thickness);
						drawList->AddLine(ImVec2(c.x - a, c.y + offset), ImVec2(c.x + a, c.y + offset), color, thickness);
					}
					break;
				}
				case ToolbarIcon::SnapRotation:
				{
					// An angle
					const ImVec2 vertex(c.x - r * 0.8f, c.y + r * 0.7f);
					const float angle = -IM_PI * 0.3f;
					const float length = r * 1.7f;
					drawList->AddLine(vertex, ImVec2(vertex.x + length, vertex.y), color, thickness);
					drawList->AddLine(vertex, ImVec2(vertex.x + cosf(angle) * length, vertex.y + sinf(angle) * length), color, thickness);
					drawList->PathArcTo(vertex, r * 0.95f, angle, 0.f, 10);
					drawList->PathStroke(color, ImDrawFlags_None, thickness);
					break;
				}
				case ToolbarIcon::SnapScale:
				{
					// A box, and a smaller one inside of it
					const float a = r * 0.85f;
					drawList->AddRect(ImVec2(c.x - a, c.y - a), ImVec2(c.x + a, c.y + a), color, 0.f, ImDrawFlags_None, thickness);
					drawList->AddRect(ImVec2(c.x - a, c.y - a * 0.1f), ImVec2(c.x + a * 0.1f, c.y + a), color, 0.f, ImDrawFlags_None, thickness);
					break;
				}
				case ToolbarIcon::Camera:
				{
					// A video camera
					const float halfHeight = r * 0.58f;
					drawList->AddRectFilled(ImVec2(c.x - r * 0.95f, c.y - halfHeight), ImVec2(c.x + r * 0.3f, c.y + halfHeight), color, r * 0.18f);
					drawList->AddTriangleFilled(ImVec2(c.x + r * 0.38f, c.y), ImVec2(c.x + r * 0.98f, c.y - halfHeight * 0.85f), ImVec2(c.x + r * 0.98f, c.y + halfHeight * 0.85f), color);
					break;
				}
				case ToolbarIcon::Show:
				{
					// An eye
					drawList->AddEllipse(c, ImVec2(r * 0.95f, r * 0.55f), color, 0.f, 0, thickness);
					drawList->AddCircleFilled(c, r * 0.3f, color);
					break;
				}
				case ToolbarIcon::ViewMode:
				{
					// A half-lit sphere
					const float radius = r * 0.82f;
					drawList->AddCircle(c, radius, color, 0, thickness);
					drawList->PathArcTo(c, radius, -IM_PI * 0.5f, IM_PI * 0.5f, 16);
					drawList->PathFillConvex(color);
					break;
				}
			}
		}

		float CalcItemWidth(const ToolbarStyle& style, const ToolbarItem& item)
		{
			float width = item.Icon ? style.Height : style.PaddingX;
			if (!item.Text.empty())
				width += ImGui::CalcTextSize(item.Text.c_str()).x + style.PaddingX * (item.Icon ? 0.7f : 1.f);
			if (item.bDropdown)
				width += style.ArrowSize + style.PaddingX * (item.Text.empty() && item.Icon ? 0.4f : 0.8f);
			return IM_ROUND(width);
		}

		void CalcGroupWidth(const ToolbarStyle& style, ToolbarGroup& group)
		{
			group.Width = 0.f;
			for (ToolbarItem& item : group.Items)
			{
				item.Width = CalcItemWidth(style, item);
				group.Width += item.Width;
			}
		}

		// Returns true if one of the buttons is hovered
		bool DrawGroup(const ToolbarStyle& style, ToolbarGroup& group, const ImVec2& pos)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(pos, ImVec2(pos.x + group.Width, pos.y + style.Height), style.Background, style.Rounding);

			bool bHovered = false;
			float x = pos.x;
			for (ToolbarItem& item : group.Items)
			{
				item.Rect = ImRect(ImVec2(x, pos.y), ImVec2(x + item.Width, pos.y + style.Height));
				x += item.Width;

				ImGui::SetCursorScreenPos(item.Rect.Min);
				ImGui::BeginDisabled(!item.bEnabled);
				const bool bClicked = ImGui::InvisibleButton(item.ID, item.Rect.GetSize());
				const bool bItemHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
				const bool bHeld = ImGui::IsItemActive();
				const bool bShowTooltip = item.Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled);
				ImGui::EndDisabled();
				if (bShowTooltip)
					ImGui::SetTooltip("%s", item.Tooltip);
				bHovered |= bItemHovered;

				ImU32 background = item.bSelected ? style.Selected : 0;
				if (item.bEnabled && bItemHovered)
					background = item.bSelected ? style.SelectedHovered : (bHeld ? style.Pressed : style.Hovered);
				if (background)
					drawList->AddRectFilled(item.Rect.Min, item.Rect.Max, background, style.Rounding);

				const ImU32 content = item.bEnabled ? style.Content : style.ContentDisabled;
				const float centerY = item.Rect.GetCenter().y;
				float contentX = item.Rect.Min.x;
				if (item.Icon)
				{
					DrawIcon(drawList, *item.Icon, ImVec2(IM_ROUND(contentX + style.Height * 0.5f), IM_ROUND(centerY)), style.IconSize, content);
					contentX += style.Height;
				}
				else
					contentX += style.PaddingX;

				if (!item.Text.empty())
				{
					const ImVec2 textSize = ImGui::CalcTextSize(item.Text.c_str());
					if (item.Icon)
						contentX -= style.PaddingX * 0.3f;
					drawList->AddText(ImVec2(IM_ROUND(contentX), IM_ROUND(centerY - textSize.y * 0.5f)), content, item.Text.c_str());
					contentX += textSize.x + style.PaddingX * 0.4f;
				}

				if (item.bDropdown)
				{
					const float arrow = style.ArrowSize;
					const float arrowX = IM_ROUND(item.Rect.Max.x - style.PaddingX * 0.6f - arrow);
					drawList->AddTriangleFilled(ImVec2(arrowX, centerY - arrow * 0.25f), ImVec2(arrowX + arrow, centerY - arrow * 0.25f), ImVec2(arrowX + arrow * 0.5f, centerY + arrow * 0.3f), content);
				}

				if (bClicked)
				{
					if (item.OnClick)
						item.OnClick();
					if (item.PopupID)
						ImGui::OpenPopup(item.PopupID);
				}
			}
			return bHovered;
		}

		// Returns true if one of the popups is open
		bool DrawGroupPopups(const ToolbarStyle& style, const ToolbarGroup& group)
		{
			bool bOpen = false;
			for (const ToolbarItem& item : group.Items)
			{
				if (!item.PopupID)
					continue;

				const ImVec2 anchor(item.bAlignPopupRight ? item.Rect.Max.x : item.Rect.Min.x, item.Rect.Max.y + style.Spacing * 0.5f);
				ImGui::SetNextWindowPos(anchor, ImGuiCond_Appearing, ImVec2(item.bAlignPopupRight ? 1.f : 0.f, 0.f));

				// These popups are small, so the default padding puts their content right next to their borders
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.PopupPadding);
				const bool bPopupOpen = ImGui::BeginPopup(item.PopupID);
				ImGui::PopStyleVar();
				if (bPopupOpen)
				{
					bOpen = true;
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.PopupItemSpacing);
					if (item.PopupContent)
						item.PopupContent();
					ImGui::PopStyleVar();
					ImGui::EndPopup();
				}
			}
			return bOpen;
		}

		std::string FormatSnapValue(int index, float value)
		{
			char buffer[32];
			snprintf(buffer, sizeof(buffer), index == 1 ? "%g\xC2\xB0" : "%g", value); // Degree sign for the rotation
			return buffer;
		}

		std::string FormatSpeed(float speed)
		{
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "%.2f", speed);
			// Trailing zeros aren't needed: 12.50 -> 12.5, 12.00 -> 12
			std::string result = buffer;
			if (result.find('.') != std::string::npos)
			{
				while (!result.empty() && result.back() == '0')
					result.pop_back();
				if (!result.empty() && result.back() == '.')
					result.pop_back();
			}
			return result;
		}

		void DrawSnapPopup(SnapSettings& snap, int index)
		{
			static constexpr float s_TranslationPresets[] = { 0.01f, 0.05f, 0.1f, 0.5f, 1.f, 5.f, 10.f, 50.f, 100.f };
			static constexpr float s_RotationPresets[] = { 1.f, 5.f, 10.f, 15.f, 22.5f, 30.f, 45.f, 90.f };
			static constexpr float s_ScalePresets[] = { 0.01f, 0.05f, 0.1f, 0.25f, 0.5f, 1.f };
			static constexpr const char* s_Titles[] = { "Translation snap", "Rotation snap", "Scale snap" };

			const float* presets = index == 0 ? s_TranslationPresets : index == 1 ? s_RotationPresets : s_ScalePresets;
			const int presetsCount = index == 0 ? IM_ARRAYSIZE(s_TranslationPresets) : index == 1 ? IM_ARRAYSIZE(s_RotationPresets) : IM_ARRAYSIZE(s_ScalePresets);

			float& value = snap.Values[index];
			ImGui::TextDisabled("%s", s_Titles[index]);
			ImGui::Separator();
			for (int i = 0; i < presetsCount; ++i)
			{
				// Picking a value enables snapping
				if (ImGui::Selectable(FormatSnapValue(index, presets[i]).c_str(), value == presets[i]))
				{
					value = presets[i];
					snap.bEnabled[index] = true;
				}
			}

			ImGui::Separator();
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.f);
			const float maxValue = index == 1 ? 360.f : 10000.f;
			if (ImGui::DragFloat("##CustomSnap", &value, index == 1 ? 0.5f : 0.01f, 0.001f, maxValue, index == 1 ? "%g\xC2\xB0" : "%g", ImGuiSliderFlags_AlwaysClamp))
				snap.bEnabled[index] = true;
			ImGui::SetItemTooltip("Custom value");
		}

		void DrawCameraPopup(EditorCamera& camera)
		{
			ImGui::TextDisabled("Camera");
			ImGui::Separator();

			const float width = ImGui::GetFontSize() * 9.f;
			float speed = camera.GetMoveSpeed();
			ImGui::SetNextItemWidth(width);
			if (ImGui::SliderFloat("Speed", &speed, 0.1f, 1000.f, "%.2f", ImGuiSliderFlags_Logarithmic))
				camera.SetMoveSpeed(ImMax(0.1f, speed));

			float rotationSpeed = camera.GetRotationSpeed();
			ImGui::SetNextItemWidth(width);
			if (ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.1f, 20.f, "%.2f"))
				camera.SetRotationSpeed(ImMax(0.01f, rotationSpeed));

			ImGui::TextDisabled("Hold the right mouse button and scroll\nto change the speed while moving");
		}
	}

	int SnapSettings::GetIndex(int guizmoType)
	{
		switch (guizmoType)
		{
			case ImGuizmo::OPERATION::TRANSLATE: return 0;
			case ImGuizmo::OPERATION::ROTATE: return 1;
			case ImGuizmo::OPERATION::SCALE: return 2;
			default: return -1;
		}
	}

	bool SnapSettings::IsSnapping(int guizmoType) const
	{
		const int index = GetIndex(guizmoType);
		if (index < 0)
			return false;

		const bool bShift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
		return bEnabled[index] != bShift;
	}

	float SnapSettings::GetValue(int guizmoType) const
	{
		const int index = GetIndex(guizmoType);
		return index < 0 ? 0.f : Values[index];
	}

	bool ViewportToolbar::Draw(const char* id, const ImVec2& viewportMin, const ImVec2& viewportMax) const
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		const ToolbarStyle style = GetToolbarStyle();

		// Too small to have a toolbar
		if (viewportMax.x - viewportMin.x < style.Height * 10.f || viewportMax.y - viewportMin.y < style.Height * 3.f)
			return false;

		ImGui::PushID(id);
		ImGui::PushClipRect(viewportMin, viewportMax, true);

		std::vector<ToolbarGroup> leftGroups;
		if (GuizmoType)
		{
			int* guizmoType = GuizmoType;
			auto toolItem = [guizmoType](const char* itemID, ToolbarIcon icon, int type, const char* tooltip)
			{
				ToolbarItem item;
				item.ID = itemID;
				item.Icon = icon;
				item.Tooltip = tooltip;
				item.bSelected = *guizmoType == type;
				item.OnClick = [guizmoType, type]() { *guizmoType = type; };
				return item;
			};

			ToolbarGroup& tools = leftGroups.emplace_back();
			tools.Items.push_back(toolItem("##Select", ToolbarIcon::Select, -1, "Select (Q)"));
			tools.Items.push_back(toolItem("##Translate", ToolbarIcon::Translate, ImGuizmo::OPERATION::TRANSLATE, "Translate (W)"));
			tools.Items.push_back(toolItem("##Rotate", ToolbarIcon::Rotate, ImGuizmo::OPERATION::ROTATE, "Rotate (E)"));
			tools.Items.push_back(toolItem("##Scale", ToolbarIcon::Scale, ImGuizmo::OPERATION::SCALE, "Scale (R)"));

			if (GuizmoMode)
			{
				// Scaling is always done in the local space
				int* guizmoMode = GuizmoMode;
				const bool bWorld = *guizmoMode == ImGuizmo::MODE::WORLD;
				const bool bScale = *guizmoType == ImGuizmo::OPERATION::SCALE;

				ToolbarItem& item = leftGroups.emplace_back().Items.emplace_back();
				item.ID = "##Space";
				item.Icon = (bWorld && !bScale) ? ToolbarIcon::World : ToolbarIcon::Local;
				item.bEnabled = !bScale;
				item.Tooltip = bScale ? "Scaling is always done in the local space" : bWorld ? "World space. Click to switch to the local space" : "Local space. Click to switch to the world space";
				item.OnClick = [guizmoMode, bWorld]() { *guizmoMode = bWorld ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD; };
			}

			if (Snap)
			{
				static constexpr ToolbarIcon s_SnapIcons[] = { ToolbarIcon::SnapTranslation, ToolbarIcon::SnapRotation, ToolbarIcon::SnapScale };
				static constexpr const char* s_ToggleIDs[] = { "##SnapTranslation", "##SnapRotation", "##SnapScale" };
				static constexpr const char* s_ValueIDs[] = { "##SnapTranslationValue", "##SnapRotationValue", "##SnapScaleValue" };
				static constexpr const char* s_PopupIDs[] = { "##SnapTranslationPopup", "##SnapRotationPopup", "##SnapScalePopup" };
				static constexpr const char* s_Tooltips[] = { "Snap translation. Hold Shift to invert it while moving", "Snap rotation. Hold Shift to invert it while rotating", "Snap scale. Hold Shift to invert it while scaling" };

				SnapSettings* snap = Snap;
				ToolbarGroup& snapGroup = leftGroups.emplace_back();
				for (int i = 0; i < 3; ++i)
				{
					ToolbarItem& toggle = snapGroup.Items.emplace_back();
					toggle.ID = s_ToggleIDs[i];
					toggle.Icon = s_SnapIcons[i];
					toggle.Tooltip = s_Tooltips[i];
					toggle.bSelected = snap->bEnabled[i];
					toggle.OnClick = [snap, i]() { snap->bEnabled[i] = !snap->bEnabled[i]; };

					ToolbarItem& value = snapGroup.Items.emplace_back();
					value.ID = s_ValueIDs[i];
					value.Text = FormatSnapValue(i, snap->Values[i]);
					value.Tooltip = "Snap value";
					value.bDropdown = true;
					value.PopupID = s_PopupIDs[i];
					value.PopupContent = [snap, i]() { DrawSnapPopup(*snap, i); };
				}
			}
		}

		std::vector<ToolbarGroup> rightGroups;
		if (ViewModeName && ViewModeMenu)
		{
			ToolbarItem& item = rightGroups.emplace_back().Items.emplace_back();
			item.ID = "##ViewMode";
			item.Icon = ToolbarIcon::ViewMode;
			item.Text = ViewModeName;
			item.Tooltip = "View mode";
			item.bDropdown = true;
			item.PopupID = "##ViewModePopup";
			item.PopupContent = ViewModeMenu;
			item.bAlignPopupRight = true;
		}
		if (ShowMenu)
		{
			ToolbarItem& item = rightGroups.emplace_back().Items.emplace_back();
			item.ID = "##Show";
			item.Icon = ToolbarIcon::Show;
			item.Tooltip = "Show";
			item.bDropdown = true;
			item.PopupID = "##ShowPopup";
			item.PopupContent = ShowMenu;
			item.bAlignPopupRight = true;
		}
		if (Camera)
		{
			EditorCamera* camera = Camera;

			ToolbarItem& item = rightGroups.emplace_back().Items.emplace_back();
			item.ID = "##CameraSpeed";
			item.Icon = ToolbarIcon::Camera;
			item.Text = FormatSpeed(camera->GetMoveSpeed());
			item.Tooltip = "Camera speed";
			item.PopupID = "##CameraPopup";
			item.PopupContent = [camera]() { DrawCameraPopup(*camera); };
			item.bAlignPopupRight = true;
		}

		// --- Layout ---
		// The left groups start at the top left corner, the right ones end at the top right corner.
		// If they don't fit into a single row, the right groups go to the second row
		float leftWidth = 0.f;
		for (auto& group : leftGroups)
		{
			CalcGroupWidth(style, group);
			leftWidth += group.Width + style.Spacing;
		}
		float rightWidth = 0.f;
		for (auto& group : rightGroups)
		{
			CalcGroupWidth(style, group);
			rightWidth += group.Width + style.Spacing;
		}

		const float top = IM_ROUND(viewportMin.y + style.Spacing);
		float rightTop = top;
		const float leftEnd = viewportMin.x + style.Spacing + leftWidth;
		const float rightStart = viewportMax.x - rightWidth;
		if (!leftGroups.empty() && !rightGroups.empty() && rightStart < leftEnd)
			rightTop += style.Height + style.Spacing * 0.5f;

		bool bHovered = false;
		float x = IM_ROUND(viewportMin.x + style.Spacing);
		for (auto& group : leftGroups)
		{
			bHovered |= DrawGroup(style, group, ImVec2(x, top));
			x += group.Width + style.Spacing;
		}
		x = IM_ROUND(ImMax(viewportMin.x + style.Spacing, rightStart));
		for (auto& group : rightGroups)
		{
			bHovered |= DrawGroup(style, group, ImVec2(x, rightTop));
			x += group.Width + style.Spacing;
		}

		ImGui::PopClipRect();

		// Popups aren't clipped by the viewport
		bool bPopupOpen = false;
		for (const auto& group : leftGroups)
			bPopupOpen |= DrawGroupPopups(style, group);
		for (const auto& group : rightGroups)
			bPopupOpen |= DrawGroupPopups(style, group);

		ImGui::PopID();

		return bHovered || bPopupOpen;
	}
}
