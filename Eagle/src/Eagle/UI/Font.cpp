#include "egpch.h"
#include "Font.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"

namespace Eagle
{
	std::unordered_map<Path, Ref<Font>> FontLibrary::s_Fonts;

	Ref<Font> Font::Create(const Path& path)
	{
		class LocalFont : public Font
		{
		public:
			LocalFont(const Path& path) : Font(path) {}
		};

		Ref<Font> result = MakeRef<LocalFont>(path);
		FontLibrary::Add(result);
		return result;
	}

	Font::Font(const Path& path)
		: m_Path(path)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		if (!ft)
		{
			EG_CORE_ERROR("Failed to init FreeType");
			return;
		}

		msdfgen::FontHandle* font = msdfgen::loadFont(ft, path.u8string().c_str());
		if (!font)
		{
			EG_CORE_ERROR("Failed to load font: {}", path);
			msdfgen::deinitializeFreetype(ft);
			return;
		}

		m_FontGeometry = MakeScope<msdf_atlas::FontGeometry>(&m_Glyphs);
		msdf_atlas::Charset charset;

		// From ImGui
		static const uint32_t charsetRanges[] =
		{
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
			0x2DE0, 0x2DFF, // Cyrillic Extended-A
			0xA640, 0xA69F, // Cyrillic Extended-B
			0,
		};

		for (uint32_t range = 0; range < 8; range += 2)
		{
			for (uint32_t c = charsetRanges[range]; c <= charsetRanges[range + 1]; c++)
				charset.add(c);
		}

		m_FontGeometry->loadCharset(font, 1.f, charset);

		const double maxCornerAngle = 3.0;
		for (auto& glyph : m_Glyphs)
			glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);

		// TightAtlasPacker class computes the layout of the atlas.
		msdf_atlas::TightAtlasPacker packer;
		// Set atlas parameters:
		// setDimensions or setDimensionsConstraint to find the best value
		packer.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
		// setScale for a fixed size or setMinimumScale to use the largest that fits
		// setPixelRange or setUnitRange
		packer.setPixelRange(2.0);
		packer.setMiterLimit(1.0);
		packer.setScale(40.0);
		// Compute atlas layout - pack glyphs
		int remaining = packer.pack(m_Glyphs.data(), (int)m_Glyphs.size());
		if (remaining != 0)
		{
			EG_CORE_ERROR("Couldn't fit {} glyphs into the atlas texture", remaining);
		}

		// Get final atlas dimensions
		int width = 0, height = 0;
		packer.getDimensions(width, height);

		// Generating atlas
		msdf_atlas::GeneratorAttributes attributes;
		attributes.scanlinePass = true;
		msdf_atlas::ImmediateAtlasGenerator<float, 4, msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<float, 4>> generator(width, height);
		generator.setAttributes(attributes);
		generator.setThreadCount(std::thread::hardware_concurrency());
		generator.generate(m_Glyphs.data(), (int)m_Glyphs.size());

		const auto& bitmap = (msdfgen::BitmapConstRef<float, 4>)generator.atlasStorage();
		m_Atlas = Texture2D::Create(ImageFormat::R32G32B32A32_Float, glm::uvec2(bitmap.width, bitmap.height), bitmap.pixels, {}, false, path.stem().string());

		// Cleanup
		msdfgen::destroyFont(font);
		msdfgen::deinitializeFreetype(ft);
	}
	
	bool FontLibrary::Get(const Path& path, Ref<Font>* outFont)
	{
		auto it = s_Fonts.find(path);
		if (it != s_Fonts.end())
		{
			*outFont = it->second;
			return true;
		}

		return false;
	}
	
	bool FontLibrary::Get(const GUID& guid, Ref<Font>* outFont)
	{
		for (const auto& data : s_Fonts)
		{
			const auto& font = data.second;
			if (guid == font->GetGUID())
			{
				*outFont = font;
				return true;
			}
		}

		return false;
	}
}
