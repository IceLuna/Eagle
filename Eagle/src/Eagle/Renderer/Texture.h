#pragma once

#include <glm/glm.hpp>
#include <filesystem>

namespace Eagle
{
	class Texture
	{
	public:
		Texture(const std::filesystem::path& path = "", bool bLoadAsSRGB = true) : m_Path(path), m_IsSRGB(bLoadAsSRGB) {}
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual glm::vec2 GetSize() const = 0;
		virtual uint32_t GetRendererID() const = 0;
		//If Texture was loaded as SRGB, it will appear dark in UI. So Use this function to display sRGB textures in UI.
		virtual uint32_t GetNonSRGBRendererID() const = 0;
		virtual uint32_t GetDataFormat() const = 0;
		virtual uint32_t GetInternalFormat() const = 0;
		virtual const void* GetData() const = 0;
		virtual uint32_t GetChannels() const = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void SetData(const void* data) = 0;
		virtual void SetSRGB(bool bSRGB) = 0;

		virtual bool operator== (const Texture& other) const = 0;

		const std::filesystem::path& GetPath() const { return m_Path; };
		bool IsSRGB() const { return m_IsSRGB; };

	protected:
		std::filesystem::path m_Path;
		bool m_IsSRGB;
		friend class Renderer;
	};

	class Texture2D : public Texture
	{
	public:
		Texture2D(const std::filesystem::path& path = "", bool bLoadAsSRGB = true) : Texture(path, bLoadAsSRGB) {}

		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, const void* data);
		static Ref<Texture2D> Create(const std::filesystem::path& path, bool bLoadAsSRGB = true, bool bAddToLibrary = true);

		static Ref<Texture2D> WhiteTexture;
		static Ref<Texture2D> BlackTexture;
		static Ref<Texture2D> NoneTexture;
		static Ref<Texture2D> MeshIconTexture;
		static Ref<Texture2D> TextureIconTexture;
		static Ref<Texture2D> SceneIconTexture;
		static Ref<Texture2D> FolderIconTexture;
		static Ref<Texture2D> UnknownIconTexture;
		static Ref<Texture2D> PlayButtonTexture;
		static Ref<Texture2D> StopButtonTexture;
	};

	struct TextureProps
	{
		glm::vec4 TintColor = glm::vec4{1.0};
		float Opacity = 1.f;
		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};

	class TextureLibrary
	{
	public:
		static void Add(const Ref<Texture>& texture) { m_Textures.push_back(texture); }
		static bool Get(const std::filesystem::path& path, Ref<Texture>* texture);
		static bool Exist(const std::filesystem::path& path);

		static const std::vector<Ref<Texture>>& GetTextures() { return m_Textures; }

	private:
		TextureLibrary() = default;
		TextureLibrary(const TextureLibrary&) = default;
		
		static std::vector<Ref<Texture>> m_Textures;
	};
}
