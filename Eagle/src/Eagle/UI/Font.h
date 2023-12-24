#pragma once

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
		const Path& GetPath() const { return m_Path; }
		GUID GetGUID() const { return m_GUID; }
		
		static Ref<Font> Create(const Path& path);

		static bool NextLine(int index, const std::vector<int>& lines);
		static std::vector<int> GetNextLines(const msdfgen::FontMetrics& metrics, const Scope<msdf_atlas::FontGeometry>& fontGeometry, const std::u32string& text, const double spaceAdvance,
			float lineHeightOffset, float kerningOffset, float maxWidth);

	protected:
		Font(const Path& path);

	private:
		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs; // Storage for glyph geometry and their coordinates in the atlas

		// FontGeometry is a helper class that loads a set of glyphs from a single font.
		// It can also be used to get additional font metrics, kerning information, etc.
		Scope<msdf_atlas::FontGeometry> m_FontGeometry;

		Ref<Texture2D> m_Atlas;
		Path m_Path;
		GUID m_GUID;
	};

	class FontLibrary
	{
	public:
		static void Add(const Ref<Font>& font)
		{
			s_Fonts.emplace(font->GetPath(), font);
		}

		static bool Get(const Path& path, Ref<Font>* outFont);
		static bool Get(const GUID& guid, Ref<Font>* outFont);

		static const std::unordered_map<Path, Ref<Font>>& GetFonts() { return s_Fonts; }

		//TODO: Move to AssetManager::Shutdown()
		static void Clear() { s_Fonts.clear(); }

	private:
		FontLibrary() = default;
		FontLibrary(const FontLibrary&) = default;

		static std::unordered_map<Path, Ref<Font>> s_Fonts;
	};
}
