#include "TitleBar.h"

#include "Eagle/Core/Application.h"
#include "Eagle/UI/UI.h"

#include <imgui_internal.h>

namespace Eagle
{
	namespace
	{
		// Parts of a title bar drawn in the last frame, for the OS hit tests (see `TitleBar::HitTest`)
		struct HitRegion
		{
			ImGuiID ViewportID = 0;
			ImGuiID WindowID = 0;         // The window that draws the title bar
			ImRect Caption;               // The whole title bar
			ImRect MaximizeButton;        // Empty if there's no maximize button
			ImVector<ImRect> Interactive; // Parts of the title bar that are the client area (menus, buttons)
		};

		std::vector<HitRegion> s_Regions;

		enum class TitleBarButton
		{
			Minimize, Maximize, Restore, Close
		};

		void DrawButtonGlyph(ImDrawList* drawList, TitleBarButton button, const ImRect& rect, ImU32 color)
		{
			const float scale = rect.GetHeight() / 32.f;
			const float half = ImMax(3.f, IM_ROUND(5.f * scale));
			const float thickness = ImMax(1.f, IM_ROUND(scale));
			const ImVec2 center = ImFloor(rect.GetCenter());
			const float x0 = center.x - half;
			const float y0 = center.y - half;
			const float x1 = center.x + half;
			const float y1 = center.y + half;

			switch (button)
			{
				case TitleBarButton::Minimize:
					drawList->AddLine(ImVec2(x0, center.y), ImVec2(x1, center.y), color, thickness);
					break;

				case TitleBarButton::Maximize:
					drawList->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), color, 0.f, ImDrawFlags_None, thickness);
					break;

				case TitleBarButton::Restore:
				{
					// The front square, and the visible part of the back one
					const float offset = ImMax(2.f, IM_ROUND(2.f * scale));
					drawList->AddRect(ImVec2(x0, y0 + offset), ImVec2(x1 - offset, y1), color, 0.f, ImDrawFlags_None, thickness);
					const ImVec2 back[] =
					{
						ImVec2(x0 + offset, y0 + offset),
						ImVec2(x0 + offset, y0),
						ImVec2(x1, y0),
						ImVec2(x1, y1 - offset),
						ImVec2(x1 - offset, y1 - offset)
					};
					drawList->AddPolyline(back, IM_ARRAYSIZE(back), color, ImDrawFlags_None, thickness);
					break;
				}

				case TitleBarButton::Close:
					drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, thickness);
					drawList->AddLine(ImVec2(x0, y1), ImVec2(x1, y0), color, thickness);
					break;
			}
		}

		// The topmost window under `pos` in the viewport, resolved to the window that hosts it (child and docked windows resolve to their host).
		// Must work while ImGui isn't within a frame. OS asks for hit tests while the window processes its events
		ImGuiWindow* FindTopmostHostWindow(ImGuiContext& g, ImGuiID viewportID, const ImVec2& pos)
		{
			for (int i = g.Windows.Size - 1; i >= 0; --i)
			{
				ImGuiWindow* window = g.Windows[i];
				const bool bActive = g.WithinFrameScope ? window->WasActive : window->Active;
				if (!bActive || window->Hidden || !window->Viewport || window->Viewport->ID != viewportID)
					continue;
				if (window->Flags & ImGuiWindowFlags_NoMouseInputs)
					continue;
				if (window->OuterRectClipped.Contains(pos))
					return window->RootWindowDockTree;
			}
			return nullptr;
		}
	}

	float TitleBar::GetHeight()
	{
		return IM_ROUND(ImGui::GetFrameHeight() * 1.45f);
	}

	ImU32 TitleBar::GetBackgroundColor()
	{
		// The same color as the tab bars of dock nodes, so that the title bar and the tab well below it look like a single bar
		return ImGui::GetColorU32(ImGuiCol_TitleBg);
	}

	void TitleBar::NewFrame()
	{
		s_Regions.clear();
	}

	void TitleBar::Draw(const TitleBarDesc& desc)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImGuiViewportP* viewport = window->Viewport;
		const ImGuiStyle& style = ImGui::GetStyle();
		ImDrawList* drawList = window->DrawList;

		const float height = GetHeight();
		const ImRect bar(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + height));
		drawList->AddRectFilled(bar.Min, bar.Max, GetBackgroundColor());

		// Minimize/maximize only make sense if the window covers its own platform window
		void* nativeWindow = viewport ? viewport->PlatformHandleRaw : nullptr;
		const bool bOwnsPlatformWindow = nativeWindow && (viewport == ImGui::GetMainViewport() || viewport->Window == window);
		const bool bFocused = viewport && (viewport->Flags & ImGuiViewportFlags_IsFocused);
		const ImU32 textColor = ImGui::GetColorU32(bFocused ? ImGuiCol_Text : ImGuiCol_TextDisabled);

		HitRegion region;
		region.ViewportID = viewport ? viewport->ID : 0;
		region.WindowID = window->ID;
		region.Caption = bar;

		ImGui::PushID("##TitleBar");

		// --- Buttons, from right to left ---
		const float buttonWidth = IM_ROUND(height * 1.45f);
		float buttonsMinX = bar.Max.x;
		auto drawButton = [&](const char* id, TitleBarButton glyph, bool bCloseButton, bool bExternallyHovered, bool bExternallyPressed, ImRect& outRect)
		{
			outRect = ImRect(ImVec2(buttonsMinX - buttonWidth, bar.Min.y), ImVec2(buttonsMinX, bar.Max.y));
			buttonsMinX = outRect.Min.x;

			ImGui::SetCursorScreenPos(outRect.Min);
			const bool bClicked = ImGui::InvisibleButton(id, outRect.GetSize());
			const bool bHovered = ImGui::IsItemHovered() || bExternallyHovered;
			const bool bPressed = (ImGui::IsItemActive() && bHovered) || bExternallyPressed;

			ImU32 background = 0;
			ImU32 glyphColor = textColor;
			if (bCloseButton)
			{
				// Same as the native close button
				if (bPressed)
					background = IM_COL32(148, 33, 23, 255);
				else if (bHovered)
					background = IM_COL32(196, 43, 28, 255);
				if (bPressed || bHovered)
					glyphColor = IM_COL32_WHITE;
			}
			else if (bPressed)
				background = ImGui::GetColorU32(ImGuiCol_Text, 0.06f);
			else if (bHovered)
				background = ImGui::GetColorU32(ImGuiCol_Text, 0.1f);

			if (background)
				drawList->AddRectFilled(outRect.Min, outRect.Max, background);
			DrawButtonGlyph(drawList, glyph, outRect, glyphColor);
			return bClicked;
		};

		ImRect buttonRect;
		if (desc.OnClose)
		{
			if (drawButton("##Close", TitleBarButton::Close, true, false, false, buttonRect))
				desc.OnClose();
			region.Interactive.push_back(buttonRect);
		}

		if (bOwnsPlatformWindow)
		{
			// The OS handles clicks of the maximize button (so that it can show the snap layouts), so the button also displays the state reported by the OS.
			// If the OS doesn't handle it (no custom frame), the button gets the clicks
			const bool bMaximized = CustomWindowFrame::IsMaximized(nativeWindow);
			const CustomWindowFrame::ButtonState maximizeState = CustomWindowFrame::GetMaximizeButtonState(nativeWindow);
			if (drawButton("##Maximize", bMaximized ? TitleBarButton::Restore : TitleBarButton::Maximize, false, maximizeState.bHovered, maximizeState.bPressed, buttonRect))
			{
				if (bMaximized)
					CustomWindowFrame::Restore(nativeWindow);
				else
					CustomWindowFrame::Maximize(nativeWindow);
			}
			region.MaximizeButton = buttonRect;

			if (drawButton("##Minimize", TitleBarButton::Minimize, false, false, false, buttonRect))
				CustomWindowFrame::Minimize(nativeWindow);
			region.Interactive.push_back(buttonRect);
		}

		// --- Icon ---
		float x = bar.Min.x + IM_ROUND(height * 0.3f);
		if (desc.Icon)
		{
			const float iconSize = IM_ROUND(height * 0.6f);
			const ImVec2 iconMin(x, bar.Min.y + IM_ROUND((height - iconSize) * 0.5f));

			if (const ImTextureID iconID = UI::GetTextureID(desc.Icon))
				drawList->AddImage(iconID, iconMin, ImVec2(iconMin.x + iconSize, iconMin.y + iconSize));
			x += iconSize + IM_ROUND(height * 0.25f);
		}

		// --- Menus ---
		// They're submitted into a child window with a menu bar as tall as the title bar, so that they behave like regular menus (popups open below the bar, etc.)
		float contentMaxX = x;
		const float menusWidth = buttonsMinX - x;
		if (desc.DrawMenus && menusWidth > 0.f)
		{
			const float framePaddingY = ImMax(0.f, IM_TRUNC((height - ImGui::GetFontSize()) * 0.5f));

			ImGui::SetCursorScreenPos(ImVec2(x, bar.Min.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, framePaddingY)); // Defines the height of the menu bar
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleColor(ImGuiCol_MenuBarBg, IM_COL32(0, 0, 0, 0));
			const ImGuiWindowFlags menusFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
				| ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
			const bool bMenusVisible = ImGui::BeginChild("##Menus", ImVec2(menusWidth, height), ImGuiChildFlags_None, menusFlags);
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);

			if (bMenusVisible)
			{
				// Vertically centers the menu items
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, framePaddingY));
				const bool bMenuBar = ImGui::BeginMenuBar();
				ImGui::PopStyleVar();

				if (bMenuBar)
				{
					desc.DrawMenus();
					contentMaxX = ImMin(ImGui::GetCursorScreenPos().x, buttonsMinX);
					ImGui::EndMenuBar();
				}
			}
			ImGui::EndChild();

			if (contentMaxX > x)
				region.Interactive.push_back(ImRect(ImVec2(x, bar.Min.y), ImVec2(contentMaxX, bar.Max.y)));
		}

		// --- Title ---
		// Centered in the title bar, unless it'd overlap the menus or the buttons
		if (!desc.Title.empty())
		{
			const char* titleBegin = desc.Title.data();
			const char* titleEnd = titleBegin + desc.Title.size();
			const ImVec2 textSize = ImGui::CalcTextSize(titleBegin, titleEnd);
			const float spacing = IM_ROUND(height * 0.5f);
			const float minX = contentMaxX + spacing;
			const float maxX = buttonsMinX - spacing;
			if (maxX > minX)
			{
				const float textX = ImClamp(IM_ROUND(bar.GetCenter().x - textSize.x * 0.5f), minX, ImMax(minX, maxX - textSize.x));
				const float textY = IM_ROUND(bar.GetCenter().y - textSize.y * 0.5f);
				ImGui::PushStyleColor(ImGuiCol_Text, textColor);
				ImGui::RenderTextEllipsis(drawList, ImVec2(textX, textY), ImVec2(maxX, bar.Max.y), maxX, titleBegin, titleEnd, &textSize);
				ImGui::PopStyleColor();
			}
		}

		ImGui::PopID();
		ImGui::SetCursorScreenPos(ImVec2(bar.Min.x, bar.Max.y));

		s_Regions.push_back(std::move(region));
	}

	WindowHitTest TitleBar::HitTest(ImGuiID viewportID, const glm::ivec2& screenPos)
	{
		ImGuiContext* context = ImGui::GetCurrentContext();
		if (!context)
			return WindowHitTest::Client;

		const ImVec2 pos((float)screenPos.x, (float)screenPos.y);
		for (const HitRegion& region : s_Regions)
		{
			if (region.ViewportID != viewportID || !region.Caption.Contains(pos))
				continue;

			// Something on top of the title bar (a menu, a popup, a floating window) is the client area
			const ImGuiWindow* topmost = FindTopmostHostWindow(*context, viewportID, pos);
			if (!topmost || topmost->ID != region.WindowID)
				return WindowHitTest::Client;

			if (region.MaximizeButton.Contains(pos))
				return WindowHitTest::MaximizeButton;

			for (const ImRect& rect : region.Interactive)
			{
				if (rect.Contains(pos))
					return WindowHitTest::Client;
			}

			return WindowHitTest::Caption;
		}
		return WindowHitTest::Client;
	}

	WindowHitTestCallback TitleBar::MakeHitTestCallback(ImGuiID viewportID)
	{
		return [viewportID](const glm::ivec2& screenPos) { return HitTest(viewportID, screenPos); };
	}

	float TitleBar::GetResizeBorder(const ImGuiViewport* viewport)
	{
		if (!viewport || !viewport->PlatformHandleRaw)
			return 0.f;

		return (float)CustomWindowFrame::GetResizeBorderThickness(viewport->PlatformHandleRaw);
	}

	void TitleBar::RequestAppClose()
	{
		Application::Get().CallNextFrame([]()
		{
			WindowCloseEvent event(false);
			Application::Get().OnEvent(event);
		});
	}
}
