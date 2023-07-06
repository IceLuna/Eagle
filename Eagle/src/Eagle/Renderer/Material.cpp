#include "egpch.h"
#include "Material.h"
#include "TextureSystem.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
    Material::Material(const Ref<Material>& other)
		: m_AlbedoTexture(other->m_AlbedoTexture)
		, m_NormalTexture(other->m_NormalTexture)
		, m_MetallnessTexture(other->m_MetallnessTexture)
		, m_RoughnessTexture(other->m_RoughnessTexture)
		, m_AOTexture(other->m_AOTexture)
		, m_EmissiveTexture(other->m_EmissiveTexture)
		, TintColor(other->TintColor)
		, TilingFactor(other->TilingFactor)
		, BlendMode(other->BlendMode)
	{}
}

CPUMaterial::CPUMaterial(const Eagle::Ref<Eagle::Material>& material)
	: TintColor(material->TintColor), EmissiveIntensity(material->EmissiveIntensity), TilingFactor(material->TilingFactor)
{
	using namespace Eagle;

	const uint32_t albedoTextureIndex = TextureSystem::AddTexture(material->GetAlbedoTexture());
	const uint32_t metallnessTextureIndex = TextureSystem::AddTexture(material->GetMetallnessTexture());
	const uint32_t normalTextureIndex = TextureSystem::AddTexture(material->GetNormalTexture());
	const uint32_t roughnessTextureIndex = TextureSystem::AddTexture(material->GetRoughnessTexture());
	const uint32_t aoTextureIndex = TextureSystem::AddTexture(material->GetAOTexture());
	const uint32_t emissiveTextureIndex = TextureSystem::AddTexture(material->GetEmissiveTexture());
	const uint32_t opacityTextureIndex = TextureSystem::AddTexture(material->GetOpacityTexture());

	PackedTextureIndices = PackedTextureIndices2 = PackedTextureIndices3 = 0;
	PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
	PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
	PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

	PackedTextureIndices2 |= (emissiveTextureIndex << EmissiveTextureOffset);
	PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
	PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

	PackedTextureIndices3 |= (opacityTextureIndex & OpacityTextureMask);
}

CPUMaterial& CPUMaterial::operator=(const std::shared_ptr<Eagle::Texture2D>& texture)
{
	uint32_t albedoTextureIndex = Eagle::TextureSystem::AddTexture(texture);
	PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

	return *this;
}

CPUMaterial& CPUMaterial::operator=(const std::shared_ptr<Eagle::Material>& material)
{
	using namespace Eagle;

	TintColor = material->TintColor;
	EmissiveIntensity = material->EmissiveIntensity;
	TilingFactor = material->TilingFactor;

	const uint32_t albedoTextureIndex = TextureSystem::AddTexture(material->GetAlbedoTexture());
	const uint32_t metallnessTextureIndex = TextureSystem::AddTexture(material->GetMetallnessTexture());
	const uint32_t normalTextureIndex = TextureSystem::AddTexture(material->GetNormalTexture());
	const uint32_t roughnessTextureIndex = TextureSystem::AddTexture(material->GetRoughnessTexture());
	const uint32_t aoTextureIndex = TextureSystem::AddTexture(material->GetAOTexture());
	const uint32_t emissiveTextureIndex = TextureSystem::AddTexture(material->GetEmissiveTexture());
	const uint32_t opacityTextureIndex = TextureSystem::AddTexture(material->GetOpacityTexture());

	PackedTextureIndices = PackedTextureIndices2 = PackedTextureIndices3 = 0u;
	PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
	PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
	PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

	PackedTextureIndices2 |= (emissiveTextureIndex << EmissiveTextureOffset);
	PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
	PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);
	
	PackedTextureIndices3 |= (opacityTextureIndex & OpacityTextureMask);

	return *this;
}
