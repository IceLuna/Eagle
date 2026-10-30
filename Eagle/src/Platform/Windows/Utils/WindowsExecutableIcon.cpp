#include "egpch.h"
#include "WindowsImage.h"

#include "Eagle/Utils/PlatformUtils.h"

#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

// How it works:
// The icon that Windows shows for an executable (Explorer, shortcuts, a pinned taskbar button) is its first RT_GROUP_ICON resource.
// A group lists the images of the icon (one per size); each image is a separate RT_ICON resource.
// The resources of an executable that isn't running can be replaced with BeginUpdateResource / UpdateResource / EndUpdateResource.
// The new images are written with the IDs 1..N: each size is a 32-bit DIB, except for 256x256 which is a PNG.
// Every existing group is replaced with the new one (same name & language), so whichever group comes first, it's the new icon.
// The new resources take the language of the existing icons, so that they replace them instead of being added next to them.
// The rest of the existing icon resources are removed.

namespace Eagle
{
	namespace
	{
		// The wide versions of RT_ICON & RT_GROUP_ICON (those depend on whether UNICODE is defined)
		const LPCWSTR s_IconType = MAKEINTRESOURCEW(3);
		const LPCWSTR s_GroupIconType = MAKEINTRESOURCEW(14);

		constexpr WORD s_IconResourceLanguage = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
		constexpr WORD s_IconGroupID = 1;
		constexpr uint32_t s_IconSizes[] = { 16, 20, 24, 32, 40, 48, 64, 256 }; // 20 & 40 are the small & large icons at 125% scaling
		constexpr uint32_t s_MinPngIconSize = 256;

#pragma pack(push, 2)
		// The layout of RT_GROUP_ICON resources
		struct GroupIconDirectory
		{
			WORD Reserved = 0;
			WORD Type = 1; // 1 - icon
			WORD Count = 0;
		};

		struct GroupIconDirectoryEntry
		{
			BYTE Width = 0;  // 0 means 256
			BYTE Height = 0; // 0 means 256
			BYTE ColorCount = 0;
			BYTE Reserved = 0;
			WORD Planes = 1;
			WORD BitCount = 32;
			DWORD BytesInResource = 0;
			WORD ID = 0; // ID of the RT_ICON resource
		};
#pragma pack(pop)

		// An existing icon resource, to be removed
		struct ResourceID
		{
			LPCWSTR Type = nullptr;
			std::wstring Name; // Used if `bIntegerName` is false
			WORD IntegerName = 0;
			bool bIntegerName = false;
			WORD Language = 0;

			LPCWSTR GetName() const { return bIntegerName ? MAKEINTRESOURCEW(IntegerName) : Name.c_str(); }
		};

		struct EnumContext
		{
			LPCWSTR Type = nullptr;
			std::vector<ResourceID>* Resources = nullptr;
		};

		BOOL CALLBACK EnumLanguagesCallback(HMODULE, LPCWSTR, LPCWSTR name, WORD language, LONG_PTR param)
		{
			EnumContext& context = *(EnumContext*)param;
			ResourceID& id = context.Resources->emplace_back();
			id.Type = context.Type;
			id.Language = language;
			id.bIntegerName = IS_INTRESOURCE(name);
			if (id.bIntegerName)
				id.IntegerName = WORD(ULONG_PTR(name));
			else
				id.Name = name;
			return TRUE;
		}

		BOOL CALLBACK EnumNamesCallback(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR param)
		{
			EnumResourceLanguagesW(module, type, name, &EnumLanguagesCallback, param);
			return TRUE;
		}

		// Executables built without icons simply have none
		std::vector<ResourceID> FindIconResources(const Path& exePath)
		{
			std::vector<ResourceID> resources;
			HMODULE module = LoadLibraryExW(exePath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
			if (!module)
				return resources;

			for (LPCWSTR type : { s_GroupIconType, s_IconType })
			{
				EnumContext context{ type, &resources };
				EnumResourceNamesW(module, type, &EnumNamesCallback, (LONG_PTR)&context);
			}
			FreeLibrary(module);
			return resources;
		}

		// RT_ICON in the DIB format: a header, the color pixels (bottom-up), then a 1-bit AND mask (bottom-up, rows aligned to 4 bytes).
		// The header's height is doubled, since it covers both the colors & the mask
		std::vector<uint8_t> CreateDibIcon(const uint32_t* pixels, uint32_t size)
		{
			const size_t maskStride = ((size + 31u) / 32u) * 4u;
			const size_t colorBytes = size_t(size) * size_t(size) * 4u;
			const size_t maskBytes = maskStride * size;

			std::vector<uint8_t> data(sizeof(BITMAPINFOHEADER) + colorBytes + maskBytes, 0);
			BITMAPINFOHEADER& header = *(BITMAPINFOHEADER*)data.data();
			header.biSize = sizeof(BITMAPINFOHEADER);
			header.biWidth = LONG(size);
			header.biHeight = LONG(size * 2u);
			header.biPlanes = 1;
			header.biBitCount = 32;
			header.biCompression = BI_RGB;
			header.biSizeImage = DWORD(colorBytes + maskBytes);

			uint8_t* color = data.data() + sizeof(BITMAPINFOHEADER);
			uint8_t* mask = color + colorBytes;
			for (uint32_t y = 0; y < size; ++y)
			{
				const uint32_t* sourceRow = pixels + size_t(size - 1u - y) * size; // Bottom-up
				std::memcpy(color + size_t(y) * size * 4u, sourceRow, size_t(size) * 4u);

				// Fully transparent pixels are masked out, for anything that ignores the alpha channel
				uint8_t* maskRow = mask + size_t(y) * maskStride;
				for (uint32_t x = 0; x < size; ++x)
				{
					if ((sourceRow[x] >> 24) == 0)
						maskRow[x / 8u] |= uint8_t(0x80u >> (x % 8u));
				}
			}
			return data;
		}
	}

	namespace Utils
	{
		bool SetExecutableIcon(const Path& exePath, const Path& imagePath)
		{
			WindowsImage::WICContext wic;
			if (!wic)
				return false;

			WindowsImage::ComPtr<IWICBitmapSource> image = WindowsImage::LoadImageFile(wic.GetFactory(), imagePath);
			if (!image)
			{
				EG_CORE_ERROR("Failed to set the icon of `{}`. Couldn't load the image: {}", exePath, imagePath);
				return false;
			}

			// Encode all sizes first, so that nothing is changed if it fails
			std::vector<std::vector<uint8_t>> icons;
			icons.reserve(std::size(s_IconSizes));
			for (const uint32_t size : s_IconSizes)
			{
				std::vector<uint8_t>& icon = icons.emplace_back();
				bool bSuccess = false;
				if (size >= s_MinPngIconSize)
				{
					bSuccess = WindowsImage::EncodeScaledPng(wic.GetFactory(), image.Get(), size, size, icon);
				}
				else
				{
					std::vector<uint32_t> pixels(size_t(size) * size_t(size));
					bSuccess = WindowsImage::CopyScaledPixelsStraightAlpha(wic.GetFactory(), image.Get(), size, size, pixels.data());
					if (bSuccess)
						icon = CreateDibIcon(pixels.data(), size);
				}

				if (!bSuccess || icon.empty())
				{
					EG_CORE_ERROR("Failed to set the icon of `{}`. Couldn't create the {}x{} image", exePath, size, size);
					return false;
				}
			}

			// The group lists the images
			std::vector<uint8_t> group(sizeof(GroupIconDirectory) + sizeof(GroupIconDirectoryEntry) * icons.size());
			GroupIconDirectory& directory = *new (group.data()) GroupIconDirectory();
			directory.Count = WORD(icons.size());
			for (size_t i = 0; i < icons.size(); ++i)
			{
				GroupIconDirectoryEntry& entry = *new (group.data() + sizeof(GroupIconDirectory) + sizeof(GroupIconDirectoryEntry) * i) GroupIconDirectoryEntry();
				const uint32_t size = s_IconSizes[i];
				entry.Width = size >= 256u ? 0 : BYTE(size);
				entry.Height = entry.Width;
				entry.BytesInResource = DWORD(icons[i].size());
				entry.ID = WORD(i + 1);
			}

			// Must be found before the update starts, the file can't be loaded while it's being updated
			const std::vector<ResourceID> existingIcons = FindIconResources(exePath);

			HANDLE update = BeginUpdateResourceW(exePath.c_str(), FALSE);
			if (!update)
			{
				EG_CORE_ERROR("Failed to set the icon of `{}`. Couldn't open it for update (error {}). Is it running?", exePath, GetLastError());
				return false;
			}

			// The language of the existing icons, if there are any
			WORD language = s_IconResourceLanguage;
			for (const ResourceID& resource : existingIcons)
			{
				if (resource.Type == s_GroupIconType)
				{
					language = resource.Language;
					break;
				}
			}

			auto isNewImage = [&](const ResourceID& resource)
			{
				return resource.Type == s_IconType && resource.bIntegerName && resource.IntegerName >= 1 && resource.IntegerName <= icons.size() && resource.Language == language;
			};

			bool bSuccess = true;
			for (size_t i = 0; i < icons.size() && bSuccess; ++i)
				bSuccess = UpdateResourceW(update, s_IconType, MAKEINTRESOURCEW(i + 1), language, icons[i].data(), DWORD(icons[i].size())) != FALSE;

			bool bReplacedGroup = false;
			for (const ResourceID& resource : existingIcons)
			{
				if (!bSuccess)
					break;

				if (resource.Type == s_GroupIconType)
				{
					bSuccess = UpdateResourceW(update, s_GroupIconType, resource.GetName(), resource.Language, group.data(), DWORD(group.size())) != FALSE;
					bReplacedGroup = true;
				}
				else if (!isNewImage(resource))
				{
					// Not referenced anymore. It's not an error if it can't be removed
					UpdateResourceW(update, resource.Type, resource.GetName(), resource.Language, nullptr, 0);
				}
			}

			if (bSuccess && !bReplacedGroup)
				bSuccess = UpdateResourceW(update, s_GroupIconType, MAKEINTRESOURCEW(s_IconGroupID), language, group.data(), DWORD(group.size())) != FALSE;

			if (!bSuccess)
			{
				EG_CORE_ERROR("Failed to set the icon of `{}`. Couldn't update its resources (error {})", exePath, GetLastError());
				EndUpdateResourceW(update, TRUE); // Discards the changes
				return false;
			}
			if (!EndUpdateResourceW(update, FALSE))
			{
				EG_CORE_ERROR("Failed to set the icon of `{}`. Couldn't write its resources (error {})", exePath, GetLastError());
				return false;
			}

			// Explorer caches icons, let it know that this one changed
			SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, exePath.c_str(), nullptr);
			return true;
		}
	}
}
