#include "egpch.h"
#include "Material.h"
#include "TextureSystem.h"
#include "MaterialSystem.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
	class PrivateMaterial : public Material
	{
	public:
		PrivateMaterial() = default;
		PrivateMaterial(const Ref<Material>& other) : Material(other) {}
	};

    Material::Material(const Ref<Material>& other)
		: m_AlbedoTexture(other->m_AlbedoTexture)
		, m_NormalTexture(other->m_NormalTexture)
		, m_MetallnessTexture(other->m_MetallnessTexture)
		, m_RoughnessTexture(other->m_RoughnessTexture)
		, m_AOTexture(other->m_AOTexture)
		, m_EmissiveTexture(other->m_EmissiveTexture)
		, m_OpacityTexture(other->m_OpacityTexture)
		, m_OpacityMaskTexture(other->m_OpacityMaskTexture)
		, m_TintColor(other->m_TintColor)
		, m_EmissiveIntensity(other->m_EmissiveIntensity)
		, m_TilingFactor(other->m_TilingFactor)
		, m_BlendMode(other->m_BlendMode)
	{}
	
	Ref<Material> Material::Create()
	{
		Ref<Material> material = MakeRef<PrivateMaterial>();
		MaterialSystem::AddMaterial(material);
		return material;
	}

	Ref<Material> Material::Create(const Ref<Material>& other)
	{
		Ref<Material> material = MakeRef<PrivateMaterial>(other);
		MaterialSystem::AddMaterial(material);
		return material;
	}

	void Material::OnMaterialChanged()
	{
		auto thisMaterial = shared_from_this();
		MaterialSystem::OnMaterialChanged(thisMaterial);
	}
}

CPUMaterial::CPUMaterial(const Eagle::Ref<Eagle::Material>& material)
	: TintColor(material->GetTintColor()), EmissiveIntensity(material->GetEmissiveIntensity()), TilingFactor(material->GetTilingFactor())
{
	using namespace Eagle;

	const uint32_t albedoTextureIndex = material->GetAlbedoTexture() ? TextureSystem::AddTexture(material->GetAlbedoTexture()->GetTexture()) : 0u;
	const uint32_t metallnessTextureIndex = material->GetMetallnessTexture() ? TextureSystem::AddTexture(material->GetMetallnessTexture()->GetTexture()) : 0u;
	const uint32_t normalTextureIndex = material->GetNormalTexture() ? TextureSystem::AddTexture(material->GetNormalTexture()->GetTexture()) : 0u;
	const uint32_t roughnessTextureIndex = material->GetRoughnessTexture() ? TextureSystem::AddTexture(material->GetRoughnessTexture()->GetTexture()) : 0u;
	const uint32_t aoTextureIndex = material->GetAOTexture() ? TextureSystem::AddTexture(material->GetAOTexture()->GetTexture()) : 0u;
	const uint32_t emissiveTextureIndex = material->GetEmissiveTexture() ? TextureSystem::AddTexture(material->GetEmissiveTexture()->GetTexture()) : 0u;
	const uint32_t opacityTextureIndex = material->GetOpacityTexture() ? TextureSystem::AddTexture(material->GetOpacityTexture()->GetTexture()) : 0u;
	const uint32_t opacityMaskTextureIndex = material->GetOpacityMaskTexture() ? TextureSystem::AddTexture(material->GetOpacityMaskTexture()->GetTexture()) : 0u;

	PackedTextureIndices = PackedTextureIndices2 = PackedTextureIndices3 = 0;
	PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
	PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
	PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

	PackedTextureIndices2 |= (emissiveTextureIndex << EmissiveTextureOffset);
	PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
	PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

	PackedTextureIndices3 |= (opacityMaskTextureIndex << OpacityTextureOffset);
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

	TintColor = material->GetTintColor();
	EmissiveIntensity = material->GetEmissiveIntensity();
	TilingFactor = material->GetTilingFactor();

	const uint32_t albedoTextureIndex = material->GetAlbedoTexture() ? TextureSystem::AddTexture(material->GetAlbedoTexture()->GetTexture()) : 0u;
	const uint32_t metallnessTextureIndex = material->GetMetallnessTexture() ? TextureSystem::AddTexture(material->GetMetallnessTexture()->GetTexture()) : 0u;
	const uint32_t normalTextureIndex = material->GetNormalTexture() ? TextureSystem::AddTexture(material->GetNormalTexture()->GetTexture()) : 0u;
	const uint32_t roughnessTextureIndex = material->GetRoughnessTexture() ? TextureSystem::AddTexture(material->GetRoughnessTexture()->GetTexture()) : 0u;
	const uint32_t aoTextureIndex = material->GetAOTexture() ? TextureSystem::AddTexture(material->GetAOTexture()->GetTexture()) : 0u;
	const uint32_t emissiveTextureIndex = material->GetEmissiveTexture() ? TextureSystem::AddTexture(material->GetEmissiveTexture()->GetTexture()) : 0u;
	const uint32_t opacityTextureIndex = material->GetOpacityTexture() ? TextureSystem::AddTexture(material->GetOpacityTexture()->GetTexture()) : 0u;
	const uint32_t opacityMaskTextureIndex = material->GetOpacityMaskTexture() ? TextureSystem::AddTexture(material->GetOpacityMaskTexture()->GetTexture()) : 0u;

	PackedTextureIndices = PackedTextureIndices2 = PackedTextureIndices3 = 0u;
	PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
	PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
	PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

	PackedTextureIndices2 |= (emissiveTextureIndex << EmissiveTextureOffset);
	PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
	PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);
	
	PackedTextureIndices3 |= (opacityMaskTextureIndex << OpacityTextureOffset);
	PackedTextureIndices3 |= (opacityTextureIndex & OpacityTextureMask);

	return *this;
}
