#pragma once

#include "Eagle/Core/Core.h"

#include <glm/glm.hpp>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <vector>

// Image helpers of the Windows platform layer, based on WIC (Windows Imaging Component).
// Used for images that are needed outside of the renderer: the splash screen, window & executable icons.
namespace Eagle::WindowsImage
{
	using Microsoft::WRL::ComPtr;

	// Initializes COM for the current thread (if it's not yet) and creates a WIC factory.
	// WIC objects must only be used while it's alive, and on the thread that created it
	class WICContext
	{
	public:
		WICContext();
		~WICContext();
		WICContext(const WICContext&) = delete;
		WICContext& operator=(const WICContext&) = delete;

		IWICImagingFactory* GetFactory() const { return m_Factory.Get(); }
		explicit operator bool() const { return m_Factory != nullptr; }

	private:
		ComPtr<IWICImagingFactory> m_Factory;
		bool m_bUninitializeCOM = false;
	};

	// Decodes the whole image into memory (so that the file isn't kept open), as 32-bit premultiplied BGRA.
	// Supports png, jpg, bmp (and the other formats WIC can decode). Returns null on failure
	ComPtr<IWICBitmapSource> LoadImageFile(IWICImagingFactory* wic, const Path& path);

	glm::uvec2 GetImageSize(IWICBitmapSource* source);

	// Scales the image with a high quality filter and copies its pixels into `outPixels` (`width * height`, top-down, premultiplied BGRA)
	bool CopyScaledPixels(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height, uint32_t* outPixels);

	// Same as `CopyScaledPixels()`, but the pixels have straight (not premultiplied) alpha, as icons & PNG files expect
	bool CopyScaledPixelsStraightAlpha(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height, uint32_t* outPixels);

	// A square icon of the given size. The caller owns it (`DestroyIcon()`)
	HICON CreateIcon(IWICImagingFactory* wic, IWICBitmapSource* source, int size);

	// Scales the image and encodes it as PNG (straight alpha). Returns false on failure
	bool EncodeScaledPng(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height, std::vector<uint8_t>& outPng);
}
