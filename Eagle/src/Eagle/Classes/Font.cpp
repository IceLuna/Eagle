#include "egpch.h"
#include "Font.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

#include "msdf-atlas-gen.h"

namespace Eagle
{
	Ref<Font> Font::Create(const DataBuffer& fontBuffer, const std::string& name)
	{
		class LocalFont : public Font
		{
		public:
			LocalFont(const DataBuffer& fontBuffer, const std::string& name) : Font(fontBuffer, name) {}
		};

		return MakeRef<LocalFont>(fontBuffer, name);
	}

	Ref<Font> Font::Create(const DataBuffer& atlasData, glm::uvec2 size, const DataBuffer& fontBuffer, const std::string& name)
	{
		class LocalFont : public Font
		{
		public:
			LocalFont(const DataBuffer& atlasData, glm::uvec2 size, const DataBuffer& fontBuffer, const std::string& name) : Font(atlasData, size, fontBuffer, name) {}
		};

		return MakeRef<LocalFont>(atlasData, size, fontBuffer, name);
	}

	Font::Font(const DataBuffer& buffer, const std::string& name)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		if (!ft)
		{
			EG_CORE_ERROR("Failed to init FreeType");
			return;
		}

		msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (const msdfgen::byte*)buffer.Data, (int)buffer.Size);
		if (!font)
		{
			EG_CORE_ERROR("Failed to load font");
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
		packer.setPixelRange(EG_MSDF_PIXEL_RANGE);
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
		msdf_atlas::ImmediateAtlasGenerator<float, 4, msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<msdfgen::byte, 4>> generator(width, height);
		generator.setAttributes(attributes);
		generator.setThreadCount(std::thread::hardware_concurrency());
		generator.generate(m_Glyphs.data(), (int)m_Glyphs.size());

		constexpr ImageFormat format = ImageFormat::R8G8B8A8_UNorm;
		const auto& bitmap = (msdfgen::BitmapConstRef<msdfgen::byte, 4>)generator.atlasStorage();
		const glm::uvec2 size = glm::uvec2(bitmap.width, bitmap.height);

		const size_t atlasMemSize = CalculateImageMemorySize(format, size);
		m_AtlasData.Allocate(atlasMemSize);
		m_AtlasData.Write(bitmap.pixels, atlasMemSize);
		m_Atlas = Texture2D::Create(name, format, size, m_AtlasData.Data());

		// Cleanup
		msdfgen::destroyFont(font);
		msdfgen::deinitializeFreetype(ft);
	}

	Font::Font(const DataBuffer& atlasData, glm::uvec2 size, const DataBuffer& fontBuffer, const std::string& name)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		if (!ft)
		{
			EG_CORE_ERROR("Failed to init FreeType");
			return;
		}

		msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (const msdfgen::byte*)fontBuffer.Data, (int)fontBuffer.Size);
		if (!font)
		{
			EG_CORE_ERROR("Failed to load font");
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
		packer.setPixelRange(EG_MSDF_PIXEL_RANGE);
		packer.setMiterLimit(1.0);
		packer.setScale(40.0);
		// Compute atlas layout - pack glyphs
		int remaining = packer.pack(m_Glyphs.data(), (int)m_Glyphs.size());
		if (remaining != 0)
		{
			EG_CORE_ERROR("Couldn't fit {} glyphs into the atlas texture", remaining);
		}

		m_AtlasData = ScopedDataBuffer::Copy(atlasData);
		m_Atlas = Texture2D::Create(name, ImageFormat::R8G8B8A8_UNorm, size, m_AtlasData.Data());

		// Cleanup
		msdfgen::destroyFont(font);
		msdfgen::deinitializeFreetype(ft);
	}

	TextLayout Font::Layout(const std::u32string& text, const TextLayoutSettings& settings) const
	{
		TextLayout layout;
		layout.Lines.emplace_back();
		if (!m_FontGeometry || !m_Atlas)
			return layout;

		const auto& fontGeometry = *m_FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		const double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		const double lineAdvance = fsScale * metrics.lineHeight + settings.LineSpacing;
		const double texelWidth = 1.0 / m_Atlas->GetWidth();
		const double texelHeight = 1.0 / m_Atlas->GetHeight();

		const auto* spaceGlyph = fontGeometry.getGlyph(' ');
		const double spaceAdvance = spaceGlyph ? spaceGlyph->getAdvance() : 0.0;

		layout.Ascender = float(fsScale * metrics.ascenderY);
		layout.Descender = float(fsScale * metrics.descenderY);
		layout.Glyphs.reserve(text.size());

		double x = 0.0;
		double y = 0.0;
		int lastSpace = -1; // Last space or tab of the current line. If the line gets too wide, it's broken there
		size_t glyphsCountAtLastSpace = 0;

		auto startNewLine = [&]()
		{
			x = 0.0;
			y -= lineAdvance;
			lastSpace = -1;

			auto& line = layout.Lines.emplace_back();
			line.FirstGlyph = (uint32_t)layout.Glyphs.size();
			line.BaselineY = (float)y;
		};

		const int textSize = (int)text.size();
		for (int i = 0; i < textSize; ++i)
		{
			const char32_t character = text[i];
			if (character == '\n')
			{
				startNewLine();
				continue;
			}

			// A tab is 4 spaces
			const bool bIsTab = character == '\t';
			if (character == ' ' || bIsTab)
			{
				lastSpace = i;
				glyphsCountAtLastSpace = layout.Glyphs.size();

				double advance = spaceAdvance;
				if (i + 1 < textSize)
				{
					const char32_t nextCharacter = (text[i + 1] == '\t') ? U' ' : text[i + 1];
					fontGeometry.getAdvance(advance, ' ', nextCharacter);
				}

				x += (fsScale * advance + settings.LetterSpacing) * (bIsTab ? 4.0 : 1.0);
				continue;
			}

			const auto* glyph = fontGeometry.getGlyph(character);
			if (!glyph)
				glyph = fontGeometry.getGlyph('?');
			if (!glyph)
				continue;

			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);
			pl = pl * fsScale + x;
			pr = pr * fsScale + x;
			pb = pb * fsScale + y;
			pt = pt * fsScale + y;

			if (pr > settings.MaxWidth && lastSpace != -1)
			{
				// Too wide. Remove the glyphs after the last space and lay them out again on a new line
				layout.Glyphs.resize(glyphsCountAtLastSpace);
				i = lastSpace; // `++i` moves to the first character after the space
				startNewLine();
				continue;
			}

			double l, b, r, t;
			glyph->getQuadAtlasBounds(l, b, r, t);

			auto& layoutGlyph = layout.Glyphs.emplace_back();
			layoutGlyph.PlaneMin = glm::vec2(pl, pb);
			layoutGlyph.PlaneMax = glm::vec2(pr, pt);
			layoutGlyph.AtlasUVMin = glm::vec2(l * texelWidth, b * texelHeight);
			layoutGlyph.AtlasUVMax = glm::vec2(r * texelWidth, t * texelHeight);

			if (i + 1 < textSize)
			{
				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, text[i + 1]);
				x += fsScale * advance + settings.LetterSpacing;
			}
		}

		// Glyph counts & widths
		const uint32_t glyphsCount = (uint32_t)layout.Glyphs.size();
		float boxWidth = 0.f;
		for (size_t i = 0; i < layout.Lines.size(); ++i)
		{
			auto& line = layout.Lines[i];
			const uint32_t end = (i + 1 < layout.Lines.size()) ? layout.Lines[i + 1].FirstGlyph : glyphsCount;
			line.GlyphsCount = end - line.FirstGlyph;
			for (uint32_t g = line.FirstGlyph; g < end; ++g)
				line.Width = glm::max(line.Width, layout.Glyphs[g].PlaneMax.x);
			boxWidth = glm::max(boxWidth, line.Width);
		}
		if (settings.BoxWidth > 0.f)
			boxWidth = settings.BoxWidth;

		// Alignment
		if (settings.Alignment != TextAlignment::Left)
		{
			const float alignment = GetTextAlignmentFactor(settings.Alignment);
			for (const auto& line : layout.Lines)
			{
				const float shift = (boxWidth - line.Width) * alignment;
				for (uint32_t g = line.FirstGlyph; g < line.FirstGlyph + line.GlyphsCount; ++g)
				{
					layout.Glyphs[g].PlaneMin.x += shift;
					layout.Glyphs[g].PlaneMax.x += shift;
				}
			}
		}

		layout.BoxMin = glm::vec2(0.f, layout.Lines.back().BaselineY + layout.Descender);
		layout.BoxMax = glm::vec2(boxWidth, layout.Ascender);

		if (glyphsCount > 0)
		{
			layout.BoundsMin = layout.Glyphs[0].PlaneMin;
			layout.BoundsMax = layout.Glyphs[0].PlaneMax;
			for (const auto& glyph : layout.Glyphs)
			{
				layout.BoundsMin = glm::min(layout.BoundsMin, glyph.PlaneMin);
				layout.BoundsMax = glm::max(layout.BoundsMax, glyph.PlaneMax);
			}
		}

		return layout;
	}

	const Ref<const TextLayout>& TextLayoutCache::Get(const Ref<Font>& font, const std::string& text, const TextLayoutSettings& settings)
	{
		if (!font)
		{
			m_Layout.reset();
			m_Font.reset();
			return m_Layout;
		}

		if (!m_Layout || font != m_Font || settings != m_Settings || text != m_Text)
		{
			m_Layout = MakeRef<const TextLayout>(font->Layout(Utils::ToUTF32(text), settings));
			m_Font = font;
			m_Text = text;
			m_Settings = settings;
		}
		return m_Layout;
	}
}
