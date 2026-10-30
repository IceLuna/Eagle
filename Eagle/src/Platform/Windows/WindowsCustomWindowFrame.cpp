#include "egpch.h"
#include "Eagle/Core/CustomWindowFrame.h"

#include <dwmapi.h>
#include <shellapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

// From <windowsx.h>. It's not included since it defines macros like `IsMaximized`
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// How it works:
// - The window keeps the styles of a regular resizable window (WS_THICKFRAME, WS_CAPTION, ...), that's what gives it the native behaviour
//   (snapping, animations, the system menu, the shadow). But `WM_NCCALCSIZE` makes the client area take the whole window, so the OS frame isn't there.
// - `WM_NCHITTEST` reports the resizable edges (inside the window), and asks the app about the rest: the caption, the maximize button, or the client area.
// - The window is subclassed, its previous window procedure (GLFW's, possibly hooked by ImGui) handles everything else.

namespace Eagle
{
	namespace
	{
		constexpr const wchar_t* s_PropName = L"EagleCustomWindowFrame";

		// Styles of a regular resizable window. Only the ones that the window didn't have are added (and removed when the frame is disabled)
		constexpr DWORD s_FrameStyles = WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

		struct FrameData
		{
			WNDPROC PrevWndProc = nullptr;
			WindowHitTestCallback HitTest;
			glm::ivec2 MinSize = glm::ivec2(0);
			DWORD AddedStyles = 0;
			bool bInstalled = true; // False after `Uninstall()` if the procedure had to stay in the chain. Then it's a pass-through
			bool bEnabled = true;

			// Maximize button. The OS sends non-client mouse messages for it, see `WindowHitTest::MaximizeButton`
			bool bMaxButtonHovered = false;
			bool bMaxButtonPressed = false;
			bool bTrackingNonClientLeave = false;
		};

		FrameData* GetFrameData(HWND hwnd)
		{
			return (hwnd && IsWindow(hwnd)) ? (FrameData*)GetPropW(hwnd, s_PropName) : nullptr;
		}

		int GetResizeBorder(HWND hwnd)
		{
			UINT dpi = GetDpiForWindow(hwnd);
			if (dpi == 0)
				dpi = USER_DEFAULT_SCREEN_DPI;
			return std::max(1, GetSystemMetricsForDpi(SM_CXFRAME, dpi));
		}

		bool HasAutoHideTaskbar(UINT edge, const RECT& monitorRect)
		{
			APPBARDATA data{};
			data.cbSize = sizeof(data);
			data.uEdge = edge;
			data.rc = monitorRect;
			return SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &data) != 0;
		}

		void ApplyFrame(HWND hwnd, const FrameData& data)
		{
			const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
			const LONG_PTR newStyle = data.bEnabled ? (style | (LONG_PTR)data.AddedStyles) : (style & ~(LONG_PTR)data.AddedStyles);
			if (newStyle != style)
				SetWindowLongPtrW(hwnd, GWL_STYLE, newStyle);

			// A borderless window gets the DWM shadow only if its frame is extended into the client area.
			// The content covers that pixel, so it's not visible. It's the bottom one, so that nothing can show up next to the title bar
			const MARGINS margins = { 0, 0, 0, data.bEnabled ? 1 : 0 };
			DwmExtendFrameIntoClientArea(hwnd, &margins);

			// Let the window recalculate its client area (see `WM_NCCALCSIZE`)
			SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
		}

		void SetMaxButtonState(FrameData& data, bool bHovered, bool bPressed)
		{
			data.bMaxButtonHovered = bHovered;
			data.bMaxButtonPressed = bPressed;
		}

		LRESULT HitTestFrame(HWND hwnd, FrameData& data, LPARAM lParam)
		{
			const POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

			// Resizable edges. A maximized window can't be resized
			if (!IsZoomed(hwnd))
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);
				const int border = GetResizeBorder(hwnd);
				const bool bLeft = cursor.x < rect.left + border;
				const bool bRight = cursor.x >= rect.right - border;
				const bool bTop = cursor.y < rect.top + border;
				const bool bBottom = cursor.y >= rect.bottom - border;
				if (bLeft || bRight || bTop || bBottom)
				{
					// Corners extend along the edges, so that they're easier to grab
					const int corner = border * 4;
					const bool bNearLeft = cursor.x < rect.left + corner;
					const bool bNearRight = cursor.x >= rect.right - corner;
					const bool bNearTop = cursor.y < rect.top + corner;
					const bool bNearBottom = cursor.y >= rect.bottom - corner;

					if ((bTop && bNearLeft) || (bLeft && bNearTop))
						return HTTOPLEFT;
					if ((bTop && bNearRight) || (bRight && bNearTop))
						return HTTOPRIGHT;
					if ((bBottom && bNearLeft) || (bLeft && bNearBottom))
						return HTBOTTOMLEFT;
					if ((bBottom && bNearRight) || (bRight && bNearBottom))
						return HTBOTTOMRIGHT;
					if (bTop)
						return HTTOP;
					if (bBottom)
						return HTBOTTOM;
					return bLeft ? HTLEFT : HTRIGHT;
				}
			}

			if (data.HitTest)
			{
				switch (data.HitTest(glm::ivec2(cursor.x, cursor.y)))
				{
					case WindowHitTest::Caption: return HTCAPTION;
					case WindowHitTest::MaximizeButton: return HTMAXBUTTON;
					default: break;
				}
			}

			// Client area. Let the previous window procedure decide (it can make the window transparent for the mouse)
			return CallWindowProcW(data.PrevWndProc, hwnd, WM_NCHITTEST, 0, lParam);
		}

		LRESULT CALLBACK FrameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			FrameData* data = GetFrameData(hwnd);
			if (!data)
				return DefWindowProcW(hwnd, msg, wParam, lParam);

			const WNDPROC prevWndProc = data->PrevWndProc;
			switch (msg)
			{
				case WM_NCCALCSIZE:
				{
					if (!data->bInstalled || !data->bEnabled)
						break;

					// No OS frame: the client area takes the whole window
					RECT* rect = wParam ? &((NCCALCSIZE_PARAMS*)lParam)->rgrc[0] : (RECT*)lParam;
					if (IsZoomed(hwnd))
					{
						// Depending on how it was maximized, a maximized window can be bigger than the monitor by the size of the frame it'd have.
						// The content must stay within the work area
						if (HMONITOR monitor = MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST))
						{
							MONITORINFO info{};
							info.cbSize = sizeof(info);
							RECT clipped;
							if (GetMonitorInfoW(monitor, &info) && IntersectRect(&clipped, rect, &info.rcWork))
								*rect = clipped;
						}
					}
					return 0;
				}

				case WM_NCHITTEST:
				{
					if (!data->bInstalled || !data->bEnabled)
						break;
					return HitTestFrame(hwnd, *data, lParam);
				}

				case WM_GETMINMAXINFO:
				{
					const LRESULT result = CallWindowProcW(prevWndProc, hwnd, msg, wParam, lParam);
					MINMAXINFO* info = (MINMAXINFO*)lParam;
					if (data->MinSize.x > 0)
						info->ptMinTrackSize.x = std::max<LONG>(info->ptMinTrackSize.x, data->MinSize.x);
					if (data->MinSize.y > 0)
						info->ptMinTrackSize.y = std::max<LONG>(info->ptMinTrackSize.y, data->MinSize.y);

					// A maximized borderless window takes the work area (GLFW sets it up for undecorated windows). If the taskbar auto-hides, the work area is the whole monitor,
					// and a window that covers the whole monitor is treated as a fullscreen one: the taskbar can't be revealed. So leave a pixel for it
					if (data->bInstalled && data->bEnabled)
					{
						MONITORINFO monitorInfo{};
						monitorInfo.cbSize = sizeof(monitorInfo);
						if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo) && EqualRect(&monitorInfo.rcWork, &monitorInfo.rcMonitor))
						{
							const RECT& monitorRect = monitorInfo.rcMonitor;
							if (HasAutoHideTaskbar(ABE_BOTTOM, monitorRect))
							{
								info->ptMaxSize.y -= 1;
							}
							else if (HasAutoHideTaskbar(ABE_TOP, monitorRect))
							{
								info->ptMaxPosition.y += 1;
								info->ptMaxSize.y -= 1;
							}
							else if (HasAutoHideTaskbar(ABE_LEFT, monitorRect))
							{
								info->ptMaxPosition.x += 1;
								info->ptMaxSize.x -= 1;
							}
							else if (HasAutoHideTaskbar(ABE_RIGHT, monitorRect))
							{
								info->ptMaxSize.x -= 1;
							}
						}
					}
					return result;
				}

				// --- Maximize button ---
				// The OS sends non-client messages while the cursor is over it (`HTMAXBUTTON`). Clicks are handled here, the default handling would draw the classic button
				case WM_NCMOUSEMOVE:
				{
					const bool bHovered = wParam == HTMAXBUTTON;
					SetMaxButtonState(*data, bHovered, bHovered && data->bMaxButtonPressed);
					if (!data->bTrackingNonClientLeave)
					{
						TRACKMOUSEEVENT track{};
						track.cbSize = sizeof(track);
						track.dwFlags = TME_LEAVE | TME_NONCLIENT;
						track.hwndTrack = hwnd;
						data->bTrackingNonClientLeave = TrackMouseEvent(&track) != FALSE;
					}
					break;
				}
				case WM_NCMOUSELEAVE:
				{
					data->bTrackingNonClientLeave = false;
					SetMaxButtonState(*data, false, false);
					break;
				}
				case WM_MOUSEMOVE:
				{
					// Back in the client area
					SetMaxButtonState(*data, false, false);
					break;
				}
				case WM_NCLBUTTONDOWN:
				case WM_NCLBUTTONDBLCLK:
				{
					if (wParam == HTMAXBUTTON)
					{
						SetMaxButtonState(*data, true, true);
						return 0;
					}
					break;
				}
				case WM_NCLBUTTONUP:
				{
					if (wParam == HTMAXBUTTON)
					{
						const bool bClicked = data->bMaxButtonPressed;
						SetMaxButtonState(*data, true, false);
						if (bClicked)
							ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
						return 0;
					}
					break;
				}

				case WM_NCDESTROY:
				{
					// The window is being destroyed. Release everything before the previous window procedure gets it
					SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)prevWndProc);
					RemovePropW(hwnd, s_PropName);
					delete data;
					return CallWindowProcW(prevWndProc, hwnd, msg, wParam, lParam);
				}
			}

			return CallWindowProcW(prevWndProc, hwnd, msg, wParam, lParam);
		}

		void CopyIcons(HWND source, HWND destination)
		{
			if (!source || !IsWindow(source))
				return;

			// The icons stay owned by the source window
			const LRESULT bigIcon = SendMessageW(source, WM_GETICON, ICON_BIG, 0);
			const LRESULT smallIcon = SendMessageW(source, WM_GETICON, ICON_SMALL, 0);
			if (bigIcon)
				SendMessageW(destination, WM_SETICON, ICON_BIG, bigIcon);
			if (smallIcon)
				SendMessageW(destination, WM_SETICON, ICON_SMALL, smallIcon);
		}
	}

	bool CustomWindowFrame::Install(void* nativeWindow, const WindowHitTestCallback& hitTest, const glm::ivec2& minSize, void* iconSource)
	{
		HWND hwnd = (HWND)nativeWindow;
		if (!hwnd || !IsWindow(hwnd))
			return false;

		if (iconSource && iconSource != nativeWindow)
			CopyIcons((HWND)iconSource, hwnd);

		if (FrameData* data = GetFrameData(hwnd))
		{
			data->HitTest = hitTest;
			data->MinSize = minSize;
			if (!data->bInstalled)
			{
				// It was uninstalled, but its procedure had to stay in the chain
				data->bInstalled = true;
				data->bEnabled = true;
				ApplyFrame(hwnd, *data);
			}
			return true;
		}

		FrameData* data = new FrameData();
		data->HitTest = hitTest;
		data->MinSize = minSize;
		data->AddedStyles = s_FrameStyles & ~(DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
		if (!SetPropW(hwnd, s_PropName, data))
		{
			delete data;
			return false;
		}
		data->PrevWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&FrameWndProc);

		ApplyFrame(hwnd, *data);
		return true;
	}

	void CustomWindowFrame::Uninstall(void* nativeWindow)
	{
		HWND hwnd = (HWND)nativeWindow;
		FrameData* data = GetFrameData(hwnd);
		if (!data)
			return;

		data->bInstalled = false;
		data->bEnabled = false;
		data->HitTest = nullptr;
		SetMaxButtonState(*data, false, false);
		ApplyFrame(hwnd, *data);

		// If something has subclassed the window after us, our procedure must stay in the chain. It's a pass-through now, and it's released with the window
		if ((WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC) == &FrameWndProc)
		{
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)data->PrevWndProc);
			RemovePropW(hwnd, s_PropName);
			delete data;
		}
	}

	bool CustomWindowFrame::IsInstalled(void* nativeWindow)
	{
		const FrameData* data = GetFrameData((HWND)nativeWindow);
		return data && data->bInstalled;
	}

	void CustomWindowFrame::SetHitTestCallback(void* nativeWindow, const WindowHitTestCallback& hitTest)
	{
		if (FrameData* data = GetFrameData((HWND)nativeWindow))
			data->HitTest = hitTest;
	}

	void CustomWindowFrame::SetEnabled(void* nativeWindow, bool bEnabled)
	{
		HWND hwnd = (HWND)nativeWindow;
		FrameData* data = GetFrameData(hwnd);
		if (!data || !data->bInstalled || data->bEnabled == bEnabled)
			return;

		data->bEnabled = bEnabled;
		SetMaxButtonState(*data, false, false);
		ApplyFrame(hwnd, *data);
	}

	bool CustomWindowFrame::IsEnabled(void* nativeWindow)
	{
		const FrameData* data = GetFrameData((HWND)nativeWindow);
		return data && data->bInstalled && data->bEnabled;
	}

	int CustomWindowFrame::GetResizeBorderThickness(void* nativeWindow)
	{
		HWND hwnd = (HWND)nativeWindow;
		const FrameData* data = GetFrameData(hwnd);
		if (!data || !data->bInstalled || !data->bEnabled || IsZoomed(hwnd) || IsIconic(hwnd))
			return 0;
		return GetResizeBorder(hwnd);
	}

	CustomWindowFrame::ButtonState CustomWindowFrame::GetMaximizeButtonState(void* nativeWindow)
	{
		ButtonState state;
		if (const FrameData* data = GetFrameData((HWND)nativeWindow); data && data->bInstalled && data->bEnabled)
		{
			state.bHovered = data->bMaxButtonHovered;
			state.bPressed = data->bMaxButtonPressed;
		}
		return state;
	}

	void CustomWindowFrame::Minimize(void* nativeWindow)
	{
		if (HWND hwnd = (HWND)nativeWindow; hwnd && IsWindow(hwnd))
			ShowWindow(hwnd, SW_MINIMIZE);
	}

	void CustomWindowFrame::Maximize(void* nativeWindow)
	{
		if (HWND hwnd = (HWND)nativeWindow; hwnd && IsWindow(hwnd))
			ShowWindow(hwnd, SW_MAXIMIZE);
	}

	void CustomWindowFrame::Restore(void* nativeWindow)
	{
		if (HWND hwnd = (HWND)nativeWindow; hwnd && IsWindow(hwnd))
			ShowWindow(hwnd, SW_RESTORE);
	}

	bool CustomWindowFrame::IsMaximized(void* nativeWindow)
	{
		HWND hwnd = (HWND)nativeWindow;
		return hwnd && IsWindow(hwnd) && IsZoomed(hwnd);
	}
}
