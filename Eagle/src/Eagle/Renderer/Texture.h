#pragma once

#include <glm/glm.hpp>

namespace Eagle
{

	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void SetData(const void* data) = 0;

		virtual bool operator== (const Texture& other) const = 0;

		virtual const std::string& GetPath() const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, const void* data);
		static Ref<Texture2D> Create(const std::string& path);

		static Ref<Texture> WhiteTexture;
		static Ref<Texture> BlackTexture;
	};

	struct TextureProps
	{
		float Opacity = 1.f;
		float TilingFactor = 1.f;
	};

	class TextureLibrary
	{
	public:
		static void Add(const Ref<Texture>& texture) { m_Textures.push_back(texture); }
		static bool Get(const std::string& path, Ref<Texture>* texture);

		static const std::vector<Ref<Texture>>& GetTextures() { return m_Textures; }

	private:
		TextureLibrary() = default;
		TextureLibrary(const TextureLibrary&) = default;
		
		static std::vector<Ref<Texture>> m_Textures;
	};
}
