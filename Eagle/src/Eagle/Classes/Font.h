#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/GUID.h"

namespace msdf_atlas
{
	class FontGeometry;
	class GlyphGeometry;
}

namespace msdfgen
{
	struct FontMetrics;
}

namespace Eagle
{
	class Texture2D;

	class Font
	{
	public:
		const Ref<Texture2D>& GetAtlas() const { return m_Atlas; }
		const Scope<msdf_atlas::FontGeometry>& GetFontGeometry() const { return m_FontGeometry; }
		GUID GetGUID() const { return m_GUID; }
		
		static Ref<Font> Create(const DataBuffer& buffer, const std::string& name = "Font");

		static bool NextLine(int index, const std::vector<int>& lines);
		static std::vector<int> GetNextLines(const msdfgen::FontMetrics& metrics, const Scope<msdf_atlas::FontGeometry>& fontGeometry, const std::u32string& text, const double spaceAdvance,
			float lineHeightOffset, float kerningOffset, float maxWidth);

	protected:
		Font(const DataBuffer& buffer, const std::string& name);

	private:
		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs; // Storage for glyph geometry and their coordinates in the atlas

		// FontGeometry is a helper class that loads a set of glyphs from a single font.
		// It can also be used to get additional font metrics, kerning information, etc.
		Scope<msdf_atlas::FontGeometry> m_FontGeometry;

		Ref<Texture2D> m_Atlas;
		GUID m_GUID;
	};
}
