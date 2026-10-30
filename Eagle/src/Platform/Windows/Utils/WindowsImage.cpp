#include "egpch.h"
#include "WindowsImage.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace Eagle::WindowsImage
{
	namespace
	{
		constexpr WICBitmapInterpolationMode s_HighQualityCubic = WICBitmapInterpolationMode(4); // WICBitmapInterpolationModeHighQualityCubic, Windows 10+. Not in older SDKs

		ComPtr<IWICBitmapSource> Scale(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height)
		{
			if (!wic || !source || width == 0 || height == 0)
				return {};

			const glm::uvec2 size = GetImageSize(source);
			if (size.x == width && size.y == height)
				return source;

			ComPtr<IWICBitmapScaler> scaler;
			if (FAILED(wic->CreateBitmapScaler(&scaler)))
				return {};
			if (FAILED(scaler->Initialize(source, width, height, s_HighQualityCubic))
				&& FAILED(scaler->Initialize(source, width, height, WICBitmapInterpolationModeFant)))
				return {};
			return scaler;
		}

		void Unpremultiply(uint32_t* pixels, size_t count)
		{
			for (size_t i = 0; i < count; ++i)
			{
				const uint32_t pixel = pixels[i];
				const uint32_t alpha = pixel >> 24;
				if (alpha == 0 || alpha == 255)
					continue;

				auto unpremultiply = [alpha](uint32_t value) { return std::min(255u, (value * 255u + alpha / 2u) / alpha); };
				pixels[i] = (alpha << 24) | (unpremultiply((pixel >> 16) & 0xFF) << 16) | (unpremultiply((pixel >> 8) & 0xFF) << 8) | unpremultiply(pixel & 0xFF);
			}
		}
	}

	WICContext::WICContext()
	{
		// If COM is already initialized with another concurrency model, it's still usable for WIC. It just doesn't need to be uninitialized then
		const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		m_bUninitializeCOM = SUCCEEDED(result);

		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_Factory))))
		{
			EG_CORE_ERROR("[WindowsImage] Failed to create WIC factory");
			m_Factory.Reset();
		}
	}

	WICContext::~WICContext()
	{
		m_Factory.Reset();
		if (m_bUninitializeCOM)
			CoUninitialize();
	}

	ComPtr<IWICBitmapSource> LoadImageFile(IWICImagingFactory* wic, const Path& path)
	{
		if (!wic || path.empty())
			return {};

		std::error_code error;
		if (!std::filesystem::is_regular_file(path, error))
		{
			EG_CORE_WARN("[WindowsImage] Image doesn't exist: {}", path);
			return {};
		}

		ComPtr<IWICBitmapDecoder> decoder;
		ComPtr<IWICBitmapFrameDecode> frame;
		ComPtr<IWICFormatConverter> converter;
		ComPtr<IWICBitmap> bitmap;
		if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))
			|| FAILED(decoder->GetFrame(0, &frame))
			|| FAILED(wic->CreateFormatConverter(&converter))
			|| FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))
			|| FAILED(wic->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &bitmap)))
		{
			EG_CORE_WARN("[WindowsImage] Failed to load an image: {}", path);
			return {};
		}
		return bitmap;
	}

	glm::uvec2 GetImageSize(IWICBitmapSource* source)
	{
		UINT width = 0, height = 0;
		if (source)
			source->GetSize(&width, &height);
		return glm::uvec2(width, height);
	}

	bool CopyScaledPixels(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height, uint32_t* outPixels)
	{
		ComPtr<IWICBitmapSource> scaled = Scale(wic, source, width, height);
		if (!scaled || !outPixels)
			return false;

		const UINT stride = width * 4u;
		return SUCCEEDED(scaled->CopyPixels(nullptr, stride, stride * height, (BYTE*)outPixels));
	}

	bool CopyScaledPixelsStraightAlpha(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height, uint32_t* outPixels)
	{
		// Scaled while premultiplied, so that transparent pixels don't bleed their color into the edges
		if (!CopyScaledPixels(wic, source, width, height, outPixels))
			return false;
		Unpremultiply(outPixels, size_t(width) * size_t(height));
		return true;
	}

	HICON CreateIcon(IWICImagingFactory* wic, IWICBitmapSource* source, int size)
	{
		if (!wic || !source || size <= 0)
			return nullptr;

		BITMAPINFO info{};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = size;
		info.bmiHeader.biHeight = -size; // Top-down
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;

		void* bits = nullptr;
		HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!color)
			return nullptr;

		HICON icon = nullptr;
		if (CopyScaledPixelsStraightAlpha(wic, source, uint32_t(size), uint32_t(size), (uint32_t*)bits))
		{
			// The mask isn't used by 32-bit icons, but it's required
			std::vector<uint8_t> maskBits(size_t((size + 15) / 16 * 2) * size_t(size), 0);
			HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());

			ICONINFO iconInfo{};
			iconInfo.fIcon = TRUE;
			iconInfo.hbmMask = mask;
			iconInfo.hbmColor = color;
			icon = CreateIconIndirect(&iconInfo);
			DeleteObject(mask);
		}
		DeleteObject(color);
		return icon;
	}

	bool EncodeScaledPng(IWICImagingFactory* wic, IWICBitmapSource* source, uint32_t width, uint32_t height, std::vector<uint8_t>& outPng)
	{
		outPng.clear();
		std::vector<uint32_t> pixels(size_t(width) * size_t(height));
		if (!CopyScaledPixelsStraightAlpha(wic, source, width, height, pixels.data()))
			return false;

		const UINT stride = width * 4u;
		ComPtr<IWICBitmap> bitmap;
		ComPtr<IStream> stream;
		ComPtr<IWICBitmapEncoder> encoder;
		ComPtr<IWICBitmapFrameEncode> frame;
		if (FAILED(wic->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA, stride, stride * height, (BYTE*)pixels.data(), &bitmap))
			|| FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))
			|| FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))
			|| FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))
			|| FAILED(encoder->CreateNewFrame(&frame, nullptr))
			|| FAILED(frame->Initialize(nullptr)))
			return false;

		WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
		if (FAILED(frame->SetSize(width, height))
			|| FAILED(frame->SetPixelFormat(&format))
			|| format != GUID_WICPixelFormat32bppBGRA // The encoder must take it as is, so that the alpha is kept
			|| FAILED(frame->WriteSource(bitmap.Get(), nullptr))
			|| FAILED(frame->Commit())
			|| FAILED(encoder->Commit()))
			return false;

		STATSTG stat{};
		if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || stat.cbSize.HighPart != 0)
			return false;

		outPng.resize(stat.cbSize.LowPart);
		const LARGE_INTEGER start{};
		ULONG bytesRead = 0;
		if (FAILED(stream->Seek(start, STREAM_SEEK_SET, nullptr))
			|| FAILED(stream->Read(outPng.data(), ULONG(outPng.size()), &bytesRead))
			|| bytesRead != outPng.size())
		{
			outPng.clear();
			return false;
		}
		return true;
	}
}
