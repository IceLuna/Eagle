#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/GUID.h"

namespace msdf_atlas
{
	class FontGeometry;
	class GlyphGeometry;
}

namespace Eagle
{
	class Texture2D;

	enum class TextAlignment
	{
		Left, Center, Right
	};

	// 0 - left, 0.5 - center, 1 - right
	inline float GetTextAlignmentFactor(TextAlignment alignment)
	{
		return (alignment == TextAlignment::Center) ? 0.5f : (alignment == TextAlignment::Right) ? 1.f : 0.f;
	}

	// The origin is the left end of the first line's baseline, Y points up
	struct TextLayoutSettings
	{
		float LineSpacing = 0.f;   // Added to the font's line height
		float LetterSpacing = 0.f; // Added to the advance of each character
		float MaxWidth = 0.f;      // Lines are wrapped at spaces & tabs to fit into it
		float BoxWidth = 0.f;      // If > 0, the layout box has this width instead of the widest line's
		TextAlignment Alignment = TextAlignment::Left; // Alignment of lines within the layout box

		bool operator==(const TextLayoutSettings& other) const
		{
			return LineSpacing == other.LineSpacing && LetterSpacing == other.LetterSpacing && MaxWidth == other.MaxWidth &&
				BoxWidth == other.BoxWidth && Alignment == other.Alignment;
		}

		bool operator!=(const TextLayoutSettings& other) const { return !(*this == other); }
	};

	struct TextLayoutGlyph
	{
		glm::vec2 PlaneMin;
		glm::vec2 PlaneMax;
		glm::vec2 AtlasUVMin;
		glm::vec2 AtlasUVMax;

		// Quad corners in this order: bottom-left, bottom-right, top-right, top-left
		glm::vec2 GetPosition(uint32_t corner) const { return GetCorner(PlaneMin, PlaneMax, corner); }
		glm::vec2 GetAtlasUV(uint32_t corner) const { return GetCorner(AtlasUVMin, AtlasUVMax, corner); }

	private:
		static glm::vec2 GetCorner(glm::vec2 min, glm::vec2 max, uint32_t corner)
		{
			EG_CORE_ASSERT(corner < 4);
			return { (corner == 1 || corner == 2) ? max.x : min.x, (corner >= 2) ? max.y : min.y };
		}
	};

	struct TextLayoutLine
	{
		uint32_t FirstGlyph = 0;
		uint32_t GlyphsCount = 0;
		float BaselineY = 0.f;
		float Width = 0.f; // Right edge of its glyphs
	};

	struct TextLayout
	{
		std::vector<TextLayoutGlyph> Glyphs;
		std::vector<TextLayoutLine> Lines; // Always has at least one line
		glm::vec2 BoundsMin = glm::vec2(0.f); // Bounds of all glyph quads. Zero if there are no glyphs
		glm::vec2 BoundsMax = glm::vec2(0.f);

		// Layout box. X: from 0 to `BoxWidth` (or the widest line). Y: from the last line's descender to the first line's ascender.
		// Unlike the bounds, it doesn't depend on which glyphs are used, so it's suitable for pivots
		glm::vec2 BoxMin = glm::vec2(0.f);
		glm::vec2 BoxMax = glm::vec2(0.f);

		float Ascender = 0.f;  // Font's ascender & descender relative to a baseline
		float Descender = 0.f;

		// Point of the layout box. (0, 0) - top-left corner, (1, 1) - bottom-right corner
		glm::vec2 GetBoxPoint(glm::vec2 point) const
		{
			return glm::vec2(BoxMin.x + (BoxMax.x - BoxMin.x) * point.x, BoxMax.y - (BoxMax.y - BoxMin.y) * point.y);
		}

		// Point of the first baseline at the left edge, center or right edge of the box. It's the origin of 3D text
		glm::vec2 GetAlignmentOrigin(TextAlignment alignment) const
		{
			return glm::vec2(BoxMin.x + (BoxMax.x - BoxMin.x) * GetTextAlignmentFactor(alignment), 0.f);
		}
	};

	class Font
	{
	public:
		// Stored in font assets. If an asset's atlas has a different version, the atlas is regenerated from the font data.
		// Increase it when the atlas generation changes and it invalidates old assets.
		static constexpr uint32_t AtlasVersion = 2;

		const Ref<Texture2D>& GetAtlas() const { return m_Atlas; }
		const Scope<msdf_atlas::FontGeometry>& GetFontGeometry() const { return m_FontGeometry; }
		const ScopedDataBuffer& GetAtlasData() const { return m_AtlasData; }
		GUID GetGUID() const { return m_GUID; }
		
		// Builds the font atlas. Might take some time.
		static Ref<Font> Create(const DataBuffer& fontBuffer, const std::string& name = "Font");
		// Instead of building the atlas, uses the provided one
		static Ref<Font> Create(const DataBuffer& atlasData, glm::uvec2 size, const DataBuffer& fontBuffer, const std::string& name = "Font");

		TextLayout Layout(const std::u32string& text, const TextLayoutSettings& settings) const;

	protected:
		Font(const DataBuffer& fontBuffer, const std::string& name);
		Font(const DataBuffer& atlasData, glm::uvec2 size, const DataBuffer& fontBuffer, const std::string& name);

	private:
		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs; // Storage for glyph geometry and their coordinates in the atlas
		ScopedDataBuffer m_AtlasData;

		// FontGeometry is a helper class that loads a set of glyphs from a single font.
		// It can also be used to get additional font metrics, kerning information, etc.
		Scope<msdf_atlas::FontGeometry> m_FontGeometry;

		Ref<Texture2D> m_Atlas;
		GUID m_GUID;
	};

	// Keeps the last layout, and lays the text out again only when the font, the text or the settings change.
	// The layout is immutable, so it can be shared with the render thread
	class TextLayoutCache
	{
	public:
		// Null if there's no font
		const Ref<const TextLayout>& Get(const Ref<Font>& font, const std::string& text, const TextLayoutSettings& settings);

	private:
		Ref<const TextLayout> m_Layout;
		Ref<Font> m_Font;
		std::string m_Text;
		TextLayoutSettings m_Settings;
	};
}
