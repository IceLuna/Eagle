#include "egpch.h"

#include "Eagle/Core/SplashScreen.h"
#include "Platform/Windows/Utils/WindowsImage.h"

#include <dwmapi.h>
#include <ShellScalingApi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "shcore.lib")

// How it works:
// - The splash screen is a regular popup window, owned by its own thread with its own message loop. So it's painted & animated
//   while the main thread is busy (loading), and the OS doesn't consider the app unresponsive.
// - It's drawn with GDI into a DIB section (a back buffer), and then copied into the window. Images are decoded & scaled by WIC.
// - The other threads only change the shared state and post a message to the window, so that it repaints.

namespace Eagle
{
	using WindowsImage::ComPtr;

	namespace
	{
		// Messages posted to the splash window by other threads
		constexpr UINT WM_SPLASH_UPDATE = WM_APP + 1; // Status/subtitle/progress changed
		constexpr UINT WM_SPLASH_RELOAD = WM_APP + 2; // The whole description changed (images, title)
		constexpr UINT WM_SPLASH_CLOSE  = WM_APP + 3;

		constexpr UINT_PTR s_AnimationTimerID = 1;
		constexpr UINT s_AnimationIntervalMs = 15;
		constexpr const wchar_t* s_WindowClassName = L"EagleSplashScreen";

		// Not in older SDKs
		constexpr DWORD s_DWMWA_WINDOW_CORNER_PREFERENCE = 33;
		constexpr int s_DWMWCP_ROUND = 2;

		// Layout, in DIPs (scaled by the monitor's DPI)
		constexpr int s_DefaultWidth = 560;
		constexpr int s_DefaultHeight = 300;
		constexpr int s_MaxImageWidth = 720;
		constexpr int s_MaxImageHeight = 450;
		constexpr int s_MinImageWidth = 360;
		constexpr int s_Padding = 28;
		constexpr int s_IconSize = 64;
		constexpr int s_IconSpacing = 20;
		constexpr int s_TitleFontSize = 30;
		constexpr int s_SubtitleFontSize = 16;
		constexpr int s_StatusFontSize = 13;
		constexpr int s_StatusBarHeight = 40;
		constexpr int s_ProgressBarHeight = 3;
		constexpr float s_MaxWorkAreaFraction = 0.9f; // The splash screen never takes more than this of the monitor's work area

		// Colors
		constexpr COLORREF s_BackgroundTop = RGB(0x26, 0x28, 0x2C);
		constexpr COLORREF s_BackgroundBottom = RGB(0x1A, 0x1B, 0x1E);
		constexpr COLORREF s_StatusBarColor = RGB(0x14, 0x15, 0x17);
		constexpr float s_StatusBarImageDarkening = 0.6f; // Over a custom image, the status bar darkens it instead
		constexpr COLORREF s_SeparatorColor = RGB(0x33, 0x35, 0x3A);
		constexpr COLORREF s_StatusColor = RGB(0xC4, 0xC7, 0xCC);
		constexpr COLORREF s_ProgressTrackColor = RGB(0x2C, 0x2E, 0x33);
		constexpr COLORREF s_TextShadowColor = RGB(0, 0, 0); // Behind the text that's drawn over a custom image
		// Title, subtitle & progress colors are configurable (see `SplashScreenColors`)

		// Everything the other threads can change
		struct SharedState
		{
			SplashScreenDesc Desc;
			std::string Status;
			float Progress = -1.f;
		};

		std::mutex s_StateMutex;
		SharedState s_State;

		std::mutex s_ControlMutex;              // Serializes Show() & Hide()
		std::thread* s_Thread = nullptr;        // Not a `std::thread` object, so that a still running thread doesn't call `std::terminate` on exit
		std::atomic<HWND> s_Window = nullptr;   // Set while the splash window exists
		std::atomic<bool> s_bUpdatePosted = false; // Coalesces the update messages, status can be reported very often

		std::wstring ToWide(const std::string& str)
		{
			if (str.empty())
				return {};
			const int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), int(str.size()), nullptr, 0);
			std::wstring result(size_t(std::max(size, 0)), L'\0');
			if (size > 0)
				MultiByteToWideChar(CP_UTF8, 0, str.data(), int(str.size()), result.data(), size);
			return result;
		}

		COLORREF ToColorRef(const glm::vec3& color)
		{
			const glm::uvec3 value = glm::uvec3(glm::round(glm::clamp(color, glm::vec3(0.f), glm::vec3(1.f)) * 255.f));
			return RGB(value.r, value.g, value.b);
		}

		uint32_t ToPixel(COLORREF color)
		{
			return 0xFF000000u | (uint32_t(GetRValue(color)) << 16) | (uint32_t(GetGValue(color)) << 8) | uint32_t(GetBValue(color));
		}

		uint32_t LerpPixel(uint32_t a, uint32_t b, float t)
		{
			auto channel = [t](uint32_t x, uint32_t y, int shift)
			{
				const float fx = float((x >> shift) & 0xFF);
				const float fy = float((y >> shift) & 0xFF);
				return uint32_t(fx + (fy - fx) * t + 0.5f) << shift;
			};
			return 0xFF000000u | channel(a, b, 16) | channel(a, b, 8) | channel(a, b, 0);
		}

		// A top-down 32-bit DIB section, so that its pixels can be accessed directly
		struct DIB
		{
			HBITMAP Handle = nullptr;
			uint32_t* Pixels = nullptr;
			int Width = 0;
			int Height = 0;

			bool Create(int width, int height)
			{
				Destroy();
				BITMAPINFO info{};
				info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				info.bmiHeader.biWidth = width;
				info.bmiHeader.biHeight = -height; // Top-down
				info.bmiHeader.biPlanes = 1;
				info.bmiHeader.biBitCount = 32;
				info.bmiHeader.biCompression = BI_RGB;

				void* bits = nullptr;
				Handle = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
				if (!Handle)
					return false;
				Pixels = (uint32_t*)bits;
				Width = width;
				Height = height;
				return true;
			}

			void Destroy()
			{
				if (Handle)
					DeleteObject(Handle);
				*this = DIB{};
			}

			explicit operator bool() const { return Handle != nullptr; }
		};

		// Premultiplied BGRA, scaled with a high quality filter
		DIB CreateScaledDIB(IWICImagingFactory* wic, IWICBitmapSource* source, int width, int height)
		{
			DIB dib;
			if (!wic || !source || width <= 0 || height <= 0 || !dib.Create(width, height))
				return dib;
			if (!WindowsImage::CopyScaledPixels(wic, source, uint32_t(width), uint32_t(height), dib.Pixels))
				dib.Destroy();
			return dib;
		}

		void DrawPremultiplied(HDC target, const DIB& dib, int x, int y)
		{
			if (!dib)
				return;
			HDC sourceDC = CreateCompatibleDC(target);
			HGDIOBJ previous = SelectObject(sourceDC, dib.Handle);
			BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
			AlphaBlend(target, x, y, dib.Width, dib.Height, sourceDC, 0, 0, dib.Width, dib.Height, blend);
			SelectObject(sourceDC, previous);
			DeleteDC(sourceDC);
		}

		void FillSolidRect(HDC dc, const RECT& rect, COLORREF color)
		{
			const COLORREF previous = SetBkColor(dc, color);
			ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
			SetBkColor(dc, previous);
		}

		HFONT CreateUIFont(int pixelHeight, int weight)
		{
			return CreateFontW(-pixelHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
		}

		class SplashWindow
		{
		public:
			~SplashWindow() { ReleaseResources(); }

			bool Create(IWICImagingFactory* wic)
			{
				m_WIC = wic;
				const HINSTANCE instance = GetModuleHandleW(nullptr);

				WNDCLASSEXW windowClass{};
				windowClass.cbSize = sizeof(windowClass);
				windowClass.style = CS_DROPSHADOW;
				windowClass.lpfnWndProc = &SplashWindow::WndProc;
				windowClass.hInstance = instance;
				windowClass.hCursor = LoadCursor(nullptr, IDC_APPSTARTING); // The arrow with a busy indicator
				windowClass.lpszClassName = s_WindowClassName;
				if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
				{
					EG_CORE_ERROR("[SplashScreen] Failed to register the window class");
					return false;
				}

				{
					std::scoped_lock lock(s_StateMutex);
					m_Desc = s_State.Desc;
				}
				LoadImages();
				const RECT rect = CalculateWindowRect();

				m_Window = CreateWindowExW(WS_EX_APPWINDOW, s_WindowClassName, GetWindowTitle().c_str(), WS_POPUP,
					rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, this);
				if (!m_Window)
				{
					EG_CORE_ERROR("[SplashScreen] Failed to create the window");
					return false;
				}

				// Rounded corners on Windows 11. Fails harmlessly on older versions
				DwmSetWindowAttribute(m_Window, s_DWMWA_WINDOW_CORNER_PREFERENCE, &s_DWMWCP_ROUND, sizeof(s_DWMWCP_ROUND));

				UpdateIcons();
				UpdateAnimationTimer();
				ShowWindow(m_Window, SW_SHOWNORMAL);
				UpdateWindow(m_Window);
				return true;
			}

			HWND GetHandle() const { return m_Window; }

		private:
			static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
			{
				if (msg == WM_NCCREATE)
				{
					const CREATESTRUCTW* createStruct = (const CREATESTRUCTW*)lParam;
					SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)createStruct->lpCreateParams);
				}

				SplashWindow* splash = (SplashWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
				if (splash)
				{
					LRESULT result = 0;
					if (splash->HandleMessage(hwnd, msg, wParam, lParam, result))
						return result;
				}
				return DefWindowProcW(hwnd, msg, wParam, lParam);
			}

			bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result)
			{
				switch (msg)
				{
					case WM_ERASEBKGND:
						result = 1; // Everything is painted in WM_PAINT
						return true;

					case WM_PAINT:
					{
						PAINTSTRUCT paint;
						HDC dc = BeginPaint(hwnd, &paint);
						Paint(dc);
						EndPaint(hwnd, &paint);
						return true;
					}

					case WM_TIMER:
						if (wParam == s_AnimationTimerID)
						{
							InvalidateRect(hwnd, nullptr, FALSE);
							return true;
						}
						return false;

					case WM_SPLASH_UPDATE:
						s_bUpdatePosted = false;
						UpdateAnimationTimer();
						InvalidateRect(hwnd, nullptr, FALSE);
						return true;

					case WM_SPLASH_RELOAD:
					{
						{
							std::scoped_lock lock(s_StateMutex);
							m_Desc = s_State.Desc;
						}
						LoadImages();
						SetWindowTextW(hwnd, GetWindowTitle().c_str());
						UpdateIcons();
						UpdateWindowRect();
						return true;
					}

					case WM_SPLASH_CLOSE:
						DestroyWindow(hwnd);
						return true;

					case WM_CLOSE:
						// It's closed by the app once it's ready (Alt+F4 or closing it from the taskbar wouldn't stop the loading anyway)
						return true;

					case WM_DPICHANGED:
					case WM_DISPLAYCHANGE:
						UpdateWindowRect();
						return true;

					case WM_DESTROY:
						KillTimer(hwnd, s_AnimationTimerID);
						PostQuitMessage(0);
						return true;
				}
				return false;
			}

			std::wstring GetWindowTitle() const
			{
				return ToWide(m_Desc.Title.empty() ? m_Desc.Subtitle : m_Desc.Title);
			}

			int Scale(int dips) const { return MulDiv(dips, int(m_DPI), USER_DEFAULT_SCREEN_DPI); }

			void LoadImages()
			{
				m_ImageSource = WindowsImage::LoadImageFile(m_WIC, m_Desc.ImagePath);
				m_IconSource = WindowsImage::LoadImageFile(m_WIC, m_Desc.IconPath);
				InvalidateCachedResources();
			}

			HMONITOR GetTargetMonitor() const
			{
				const HWND nearWindow = (HWND)m_Desc.NearWindow;
				if (nearWindow && IsWindow(nearWindow))
					return MonitorFromWindow(nearWindow, MONITOR_DEFAULTTOPRIMARY);
				return MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
			}

			// Also updates `m_DPI`
			RECT CalculateWindowRect()
			{
				const HMONITOR monitor = GetTargetMonitor();
				UINT dpiX = USER_DEFAULT_SCREEN_DPI, dpiY = USER_DEFAULT_SCREEN_DPI;
				if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
					dpiX = USER_DEFAULT_SCREEN_DPI;
				m_DPI = dpiX;

				MONITORINFO monitorInfo{};
				monitorInfo.cbSize = sizeof(monitorInfo);
				GetMonitorInfoW(monitor, &monitorInfo);
				const RECT& workArea = monitorInfo.rcWork;

				// The size in DIPs. A custom image keeps its aspect ratio
				float width = float(s_DefaultWidth);
				float height = float(s_DefaultHeight);
				const glm::uvec2 imageSize = WindowsImage::GetImageSize(m_ImageSource.Get());
				if (imageSize.x > 0 && imageSize.y > 0)
				{
					width = float(imageSize.x);
					height = float(imageSize.y);
					const float downscale = std::min({ 1.f, float(s_MaxImageWidth) / width, float(s_MaxImageHeight) / height });
					width *= downscale;
					height *= downscale;
					if (width < float(s_MinImageWidth))
					{
						height *= float(s_MinImageWidth) / width;
						width = float(s_MinImageWidth);
					}
				}

				// To pixels. It must fit the monitor
				width = width * float(m_DPI) / float(USER_DEFAULT_SCREEN_DPI);
				height = height * float(m_DPI) / float(USER_DEFAULT_SCREEN_DPI);
				const float maxWidth = float(workArea.right - workArea.left) * s_MaxWorkAreaFraction;
				const float maxHeight = float(workArea.bottom - workArea.top) * s_MaxWorkAreaFraction;
				const float fit = std::min({ 1.f, maxWidth / width, maxHeight / height });
				const int pixelWidth = std::max(1, int(std::lround(width * fit)));
				const int pixelHeight = std::max(1, int(std::lround(height * fit)));

				RECT rect;
				rect.left = workArea.left + (workArea.right - workArea.left - pixelWidth) / 2;
				rect.top = workArea.top + (workArea.bottom - workArea.top - pixelHeight) / 2;
				rect.right = rect.left + pixelWidth;
				rect.bottom = rect.top + pixelHeight;
				return rect;
			}

			void UpdateWindowRect()
			{
				const RECT rect = CalculateWindowRect();
				InvalidateCachedResources();
				SetWindowPos(m_Window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
				InvalidateRect(m_Window, nullptr, FALSE);
			}

			void UpdateIcons()
			{
				HICON bigIcon = WindowsImage::CreateIcon(m_WIC, m_IconSource.Get(), GetSystemMetricsForDpi(SM_CXICON, m_DPI));
				HICON smallIcon = WindowsImage::CreateIcon(m_WIC, m_IconSource.Get(), GetSystemMetricsForDpi(SM_CXSMICON, m_DPI));
				SendMessageW(m_Window, WM_SETICON, ICON_BIG, (LPARAM)bigIcon);
				SendMessageW(m_Window, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);

				// The window doesn't own its icons
				if (m_BigIcon)
					DestroyIcon(m_BigIcon);
				if (m_SmallIcon)
					DestroyIcon(m_SmallIcon);

				m_BigIcon = bigIcon;
				m_SmallIcon = smallIcon;
			}

			// The animation is only needed for the indeterminate progress bar
			void UpdateAnimationTimer()
			{
				bool bIndeterminate = false;
				{
					std::scoped_lock lock(s_StateMutex);
					bIndeterminate = s_State.Progress < 0.f;
				}
				if (bIndeterminate == m_bAnimating)
					return;

				m_bAnimating = bIndeterminate;
				if (bIndeterminate)
					SetTimer(m_Window, s_AnimationTimerID, s_AnimationIntervalMs, nullptr);
				else
					KillTimer(m_Window, s_AnimationTimerID);
			}

			// Scaled images & fonts depend on the window size and DPI
			void InvalidateCachedResources()
			{
				m_ScaledImage.Destroy();
				m_ScaledIcon.Destroy();
				DeleteFonts();
				m_CachedSize = SIZE{ 0, 0 };
			}

			void UpdateCachedResources(int width, int height)
			{
				if (m_CachedSize.cx == width && m_CachedSize.cy == height && m_CachedDPI == m_DPI)
					return;

				InvalidateCachedResources();
				m_CachedSize = SIZE{ width, height };
				m_CachedDPI = m_DPI;

				if (m_ImageSource)
					m_ScaledImage = CreateScaledDIB(m_WIC, m_ImageSource.Get(), width, height);
				if (m_IconSource)
					m_ScaledIcon = CreateScaledDIB(m_WIC, m_IconSource.Get(), Scale(s_IconSize), Scale(s_IconSize));

				m_TitleFont = CreateUIFont(Scale(s_TitleFontSize), FW_SEMIBOLD);
				m_SubtitleFont = CreateUIFont(Scale(s_SubtitleFontSize), FW_NORMAL);
				m_StatusFont = CreateUIFont(Scale(s_StatusFontSize), FW_NORMAL);
			}

			bool UpdateBackBuffer(HDC windowDC, int width, int height)
			{
				if (m_BackBuffer.Width == width && m_BackBuffer.Height == height && m_BackDC)
					return true;

				if (m_BackDC)
				{
					SelectObject(m_BackDC, m_PreviousBackBitmap);
					DeleteDC(m_BackDC);
					m_BackDC = nullptr;
				}

				if (!m_BackBuffer.Create(width, height))
					return false;

				m_BackDC = CreateCompatibleDC(windowDC);
				m_PreviousBackBitmap = SelectObject(m_BackDC, m_BackBuffer.Handle);
				return true;
			}

			void Paint(HDC windowDC)
			{
				RECT client;
				GetClientRect(m_Window, &client);
				const int width = client.right - client.left;
				const int height = client.bottom - client.top;
				if (width <= 0 || height <= 0 || !UpdateBackBuffer(windowDC, width, height))
					return;

				UpdateCachedResources(width, height);

				std::wstring subtitle, status;
				float progress = -1.f;
				{
					std::scoped_lock lock(s_StateMutex);
					subtitle = ToWide(s_State.Desc.Subtitle);
					status = ToWide(s_State.Status);
					progress = s_State.Progress;
				}

				HDC dc = m_BackDC;
				const bool bCustomImage = bool(m_ScaledImage);
				const int statusBarHeight = Scale(s_StatusBarHeight);
				const int statusBarTop = height - statusBarHeight;
				uint32_t* pixels = m_BackBuffer.Pixels;

				// Background
				if (bCustomImage)
				{
					FillSolidRect(dc, client, s_BackgroundBottom); // Behind the transparent parts of the image
					DrawPremultiplied(dc, m_ScaledImage, 0, 0);
					GdiFlush(); // Before accessing the pixels directly

					// The status bar darkens the image, so that the text is readable
					const uint32_t black = ToPixel(RGB(0, 0, 0));
					for (int y = std::max(0, statusBarTop); y < height; ++y)
						for (int x = 0; x < width; ++x)
							pixels[size_t(y) * size_t(width) + size_t(x)] = LerpPixel(pixels[size_t(y) * size_t(width) + size_t(x)], black, s_StatusBarImageDarkening);
				}
				else
				{
					GdiFlush();
					const uint32_t top = ToPixel(s_BackgroundTop);
					const uint32_t bottom = ToPixel(s_BackgroundBottom);
					const uint32_t statusBar = ToPixel(s_StatusBarColor);
					for (int y = 0; y < height; ++y)
					{
						const uint32_t color = y >= statusBarTop ? statusBar : LerpPixel(top, bottom, float(y) / float(std::max(1, statusBarTop - 1)));
						std::fill_n(pixels + size_t(y) * size_t(width), size_t(width), color);
					}
					FillSolidRect(dc, RECT{ 0, statusBarTop, width, statusBarTop + std::max(1, Scale(1)) }, s_SeparatorColor);
				}

				if (!bCustomImage || m_Desc.bShowTextOverImage)
					DrawHeader(dc, width, statusBarTop, subtitle, bCustomImage);

				// Status bar: the status on the left, the progress bar along the bottom edge
				{
					const int padding = Scale(16);
					const int progressHeight = std::max(2, Scale(s_ProgressBarHeight));
					RECT statusRect{ padding, statusBarTop, width - padding, height - progressHeight };

					SetBkMode(dc, TRANSPARENT);
					HGDIOBJ previousFont = SelectObject(dc, m_StatusFont);
					if (!status.empty())
					{
						SetTextColor(dc, s_StatusColor);
						DrawTextW(dc, status.c_str(), int(status.size()), &statusRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
					}
					SelectObject(dc, previousFont);

					const COLORREF progressColor = ToColorRef(m_Desc.Colors.Progress);
					const RECT track{ 0, height - progressHeight, width, height };
					FillSolidRect(dc, track, s_ProgressTrackColor);
					if (progress >= 0.f)
					{
						const int fillWidth = int(std::lround(float(width) * std::min(progress, 1.f)));
						if (fillWidth > 0)
							FillSolidRect(dc, RECT{ 0, track.top, fillWidth, track.bottom }, progressColor);
					}
					else
					{
						// A segment that sweeps across, easing in & out
						constexpr double cycleSeconds = 1.6;
						const double phase = std::fmod(double(GetTickCount64()) / 1000.0, cycleSeconds) / cycleSeconds;
						const double eased = phase * phase * (3.0 - 2.0 * phase);
						const int segmentWidth = std::max(1, width / 4);
						const int left = int(-segmentWidth + eased * double(width + segmentWidth));
						FillSolidRect(dc, RECT{ std::max(0, left), track.top, std::min(width, left + segmentWidth), track.bottom }, progressColor);
					}
				}

				BitBlt(windowDC, 0, 0, width, height, dc, 0, 0, SRCCOPY);
			}

			// The icon, title & subtitle, centered vertically above the status bar
			void DrawHeader(HDC dc, int width, int statusBarTop, const std::wstring& subtitle, bool bShadow)
			{
				const int padding = Scale(s_Padding);
				int textLeft = padding;
				const int contentCenterY = statusBarTop / 2;
				if (m_ScaledIcon)
				{
					DrawPremultiplied(dc, m_ScaledIcon, padding, contentCenterY - m_ScaledIcon.Height / 2);
					textLeft += m_ScaledIcon.Width + Scale(s_IconSpacing);
				}

				const std::wstring title = ToWide(m_Desc.Title);
				SetBkMode(dc, TRANSPARENT);
				RECT titleRect{ textLeft, 0, width - padding, 0 };
				RECT subtitleRect = titleRect;
				HGDIOBJ previousFont = SelectObject(dc, m_TitleFont);
				if (!title.empty())
					DrawTextW(dc, title.c_str(), int(title.size()), &titleRect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
				SelectObject(dc, m_SubtitleFont);
				if (!subtitle.empty())
					DrawTextW(dc, subtitle.c_str(), int(subtitle.size()), &subtitleRect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

				const int titleHeight = title.empty() ? 0 : titleRect.bottom - titleRect.top;
				const int subtitleHeight = subtitle.empty() ? 0 : subtitleRect.bottom - subtitleRect.top;
				const int spacing = (titleHeight > 0 && subtitleHeight > 0) ? Scale(2) : 0;
				int y = contentCenterY - (titleHeight + spacing + subtitleHeight) / 2;

				// Over an image, the text gets a shadow, so that it stays readable on most backgrounds
				const int shadowOffset = std::max(1, Scale(1));
				auto drawText = [&](const std::wstring& text, RECT rect, COLORREF color)
				{
					constexpr UINT flags = DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
					if (bShadow)
					{
						RECT shadowRect = rect;
						OffsetRect(&shadowRect, shadowOffset, shadowOffset);
						SetTextColor(dc, s_TextShadowColor);
						DrawTextW(dc, text.c_str(), int(text.size()), &shadowRect, flags);
					}
					SetTextColor(dc, color);
					DrawTextW(dc, text.c_str(), int(text.size()), &rect, flags);
				};

				if (!title.empty())
				{
					SelectObject(dc, m_TitleFont);
					drawText(title, RECT{ textLeft, y, width - padding, y + titleHeight }, ToColorRef(m_Desc.Colors.Title));
					y += titleHeight + spacing;
				}
				if (!subtitle.empty())
				{
					SelectObject(dc, m_SubtitleFont);
					drawText(subtitle, RECT{ textLeft, y, width - padding, y + subtitleHeight }, ToColorRef(m_Desc.Colors.Subtitle));
				}
				SelectObject(dc, previousFont);
			}

			void DeleteFonts()
			{
				for (HFONT* font : { &m_TitleFont, &m_SubtitleFont, &m_StatusFont })
				{
					if (*font)
						DeleteObject(*font);
					*font = nullptr;
				}
			}

			void ReleaseResources()
			{
				InvalidateCachedResources();
				if (m_BackDC)
				{
					SelectObject(m_BackDC, m_PreviousBackBitmap);
					DeleteDC(m_BackDC);
					m_BackDC = nullptr;
				}
				m_BackBuffer.Destroy();

				if (m_BigIcon)
					DestroyIcon(m_BigIcon);
				if (m_SmallIcon)
					DestroyIcon(m_SmallIcon);

				m_BigIcon = m_SmallIcon = nullptr;
				m_ImageSource.Reset();
				m_IconSource.Reset();
			}

		private:
			HWND m_Window = nullptr;
			IWICImagingFactory* m_WIC = nullptr;
			SplashScreenDesc m_Desc; // The splash thread's copy
			UINT m_DPI = USER_DEFAULT_SCREEN_DPI;
			bool m_bAnimating = false;

			ComPtr<IWICBitmapSource> m_ImageSource;
			ComPtr<IWICBitmapSource> m_IconSource;
			HICON m_BigIcon = nullptr;
			HICON m_SmallIcon = nullptr;

			// Depend on the window size & DPI
			SIZE m_CachedSize{ 0, 0 };
			UINT m_CachedDPI = 0;
			DIB m_ScaledImage;
			DIB m_ScaledIcon;
			HFONT m_TitleFont = nullptr;
			HFONT m_SubtitleFont = nullptr;
			HFONT m_StatusFont = nullptr;

			DIB m_BackBuffer;
			HDC m_BackDC = nullptr;
			HGDIOBJ m_PreviousBackBitmap = nullptr;
		};

		void SplashThreadMain(std::promise<bool>& created)
		{
			SetThreadDescription(GetCurrentThread(), L"Splash Screen");

			// The splash screen can be shown before the process is made DPI aware (that's done when the main window is created)
			SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

			const WindowsImage::WICContext wic; // Images aren't shown if it fails
			{
				SplashWindow window;
				const bool bCreated = window.Create(wic.GetFactory());
				if (bCreated)
					s_Window = window.GetHandle();
				created.set_value(bCreated); // `created` must not be accessed after this

				if (bCreated)
				{
					MSG msg;
					while (GetMessageW(&msg, nullptr, 0, 0) > 0)
					{
						TranslateMessage(&msg);
						DispatchMessageW(&msg);
					}
				}
				s_Window = nullptr;
			}
		}

		void PostUpdate()
		{
			if (HWND window = s_Window.load())
			{
				if (!s_bUpdatePosted.exchange(true))
					PostMessageW(window, WM_SPLASH_UPDATE, 0, 0);
			}
		}
	}

	void SplashScreen::Show(const SplashScreenDesc& desc)
	{
		std::scoped_lock control(s_ControlMutex);
		{
			std::scoped_lock lock(s_StateMutex);
			s_State.Desc = desc;
		}

		if (HWND window = s_Window.load())
		{
			PostMessageW(window, WM_SPLASH_RELOAD, 0, 0);
			return;
		}

		// The previous splash thread might have finished on its own
		if (s_Thread)
		{
			s_Thread->join();
			delete s_Thread;
			s_Thread = nullptr;
		}

		{
			std::scoped_lock lock(s_StateMutex);
			s_State.Status.clear();
			s_State.Progress = -1.f;
		}
		s_bUpdatePosted = false;

		std::promise<bool> created;
		std::future<bool> bCreated = created.get_future();
		s_Thread = new std::thread([&created]() { SplashThreadMain(created); });
		if (!bCreated.get())
		{
			s_Thread->join();
			delete s_Thread;
			s_Thread = nullptr;
		}
	}

	void SplashScreen::Hide()
	{
		std::scoped_lock control(s_ControlMutex);
		if (!s_Thread)
			return;

		if (HWND window = s_Window.load())
			PostMessageW(window, WM_SPLASH_CLOSE, 0, 0);
		s_Thread->join();
		delete s_Thread;
		s_Thread = nullptr;
	}

	bool SplashScreen::IsVisible()
	{
		return s_Window.load() != nullptr;
	}

	void SplashScreen::SetSubtitle(const std::string& subtitle)
	{
		{
			std::scoped_lock lock(s_StateMutex);
			if (s_State.Desc.Subtitle == subtitle)
				return;
			s_State.Desc.Subtitle = subtitle;
		}
		PostUpdate();
	}

	void SplashScreen::SetStatus(std::string_view status)
	{
		{
			std::scoped_lock lock(s_StateMutex);
			if (s_State.Status == status)
				return;
			s_State.Status = status;
		}
		PostUpdate();
	}

	void SplashScreen::SetProgress(float progress)
	{
		progress = progress < 0.f ? -1.f : std::min(progress, 1.f);
		{
			std::scoped_lock lock(s_StateMutex);
			if (s_State.Progress == progress)
				return;
			s_State.Progress = progress;
		}
		PostUpdate();
	}
}
