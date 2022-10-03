#pragma once

#include "Eagle/Core/GUID.h"
#include "Image.h"
#include "Sampler.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class Texture : virtual public std::enable_shared_from_this<Texture>
	{
	protected:
		Texture(const Path& path)
			: m_Path(path) {}
		Texture(ImageFormat format, glm::uvec3 size, const void* data)
			: m_Path(), m_Format(format), m_Size(size) {}

	public:
		virtual ~Texture() = default;

		virtual void SetSRGB(bool bSRGB) = 0;
		virtual bool IsSRGB() const = 0;
		virtual bool IsLoaded() const = 0;

		Ref<Image>& GetImage() { return m_Image; }
		const Ref<Image>& GetImage() const { return m_Image; }
		Ref<Sampler>& GetSampler() { return m_Sampler; }
		const Ref<Sampler>& GetSampler() const { return m_Sampler; }

		ImageFormat GetFormat() const { return m_Format; }
		const glm::uvec3& GetSize() const { return m_Size; }
		uint32_t GetWidth() const { return m_Size.x; }
		uint32_t GetHeight() const { return m_Size.y; }
		uint32_t GetDepth() const { return m_Size.z; }

		const Path& GetPath() const { return m_Path; }
		const GUID& GetGUID() const { return m_GUID; }

	protected:
		Path m_Path;
		Ref<Image> m_Image = nullptr;
		Ref<Sampler> m_Sampler = nullptr;
		GUID m_GUID;
		ImageFormat m_Format = ImageFormat::Unknown;
		glm::uvec3 m_Size = glm::uvec3(0, 0, 0);

		friend class Renderer;
	};

	struct Texture2DSpecifications
	{
		FilterMode FilterMode = FilterMode::Bilinear;
		AddressMode AddressMode = AddressMode::Wrap;
		SamplesCount SamplesCount = SamplesCount::Samples1;
		float MaxAnisotropy = 1.f;
		bool bGenerateMips = false;
		bool bSRGB = true;
	};

	struct TextureProps
	{
		glm::vec4 TintColor = glm::vec4{ 1.0 };
		float Opacity = 1.f;
		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};

	class Texture2D : public Texture
	{
	protected:
		Texture2D(const Path& path, const Texture2DSpecifications& specs = {}) : Texture(path), m_Specs(specs) {}
		Texture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs)
			: Texture(format, { size, 1u }, data)
			, m_Specs(specs) {}

	public:
		bool IsSRGB() const override { return m_Specs.bSRGB; }

	public:
		static Ref<Texture2D> Create(const Path& path, const Texture2DSpecifications& specs = {});
		static Ref<Texture2D> Create(ImageFormat format, glm::uvec2 size, const void* data = nullptr, const Texture2DSpecifications& specs = {});

		//TODO: Remove
		static Ref<Texture2D> WhiteTexture;
		static Ref<Texture2D> BlackTexture;
		static Ref<Texture2D> NoneTexture;
		static Ref<Texture2D> MeshIconTexture;
		static Ref<Texture2D> TextureIconTexture;
		static Ref<Texture2D> SceneIconTexture;
		static Ref<Texture2D> SoundIconTexture;
		static Ref<Texture2D> FolderIconTexture;
		static Ref<Texture2D> UnknownIconTexture;
		static Ref<Texture2D> PlayButtonTexture;
		static Ref<Texture2D> StopButtonTexture;

	protected:
		Texture2DSpecifications m_Specs;
	};

	class TextureLibrary
	{
	public:
		static void Add(const Ref<Texture>& texture) { m_Textures.push_back(texture); }
		static bool Get(const Path& path, Ref<Texture>* outTexture);
		static bool Get(const GUID& guid, Ref<Texture>* outTexture);
		static bool Exist(const Path& path);
		static bool Exist(const GUID& guid);

		static const std::vector<Ref<Texture>>& GetTextures() { return m_Textures; }

	private:
		TextureLibrary() = default;
		TextureLibrary(const TextureLibrary&) = default;

		//TODO: Move to AssetManager::Shutdown()
		static void Clear() { m_Textures.clear(); }
		friend class Renderer;
		
		static std::vector<Ref<Texture>> m_Textures;
	};
}
