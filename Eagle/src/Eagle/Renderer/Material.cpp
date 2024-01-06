#include "egpch.h"
#include "Material.h"
#include "TextureSystem.h"
#include "MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

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
		: m_AlbedoAsset(other->m_AlbedoAsset)
		, m_NormalAsset(other->m_NormalAsset)
		, m_MetallnessAsset(other->m_MetallnessAsset)
		, m_RoughnessAsset(other->m_RoughnessAsset)
		, m_AOAsset(other->m_AOAsset)
		, m_EmissiveAsset(other->m_EmissiveAsset)
		, m_OpacityAsset(other->m_OpacityAsset)
		, m_OpacityMaskAsset(other->m_OpacityMaskAsset)
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

	const uint32_t albedoTextureIndex = material->GetAlbedoAsset() ? TextureSystem::AddTexture(material->GetAlbedoAsset()->GetTexture()) : 0u;
	const uint32_t metallnessTextureIndex = material->GetMetallnessAsset() ? TextureSystem::AddTexture(material->GetMetallnessAsset()->GetTexture()) : 0u;
	const uint32_t normalTextureIndex = material->GetNormalAsset() ? TextureSystem::AddTexture(material->GetNormalAsset()->GetTexture()) : 0u;
	const uint32_t roughnessTextureIndex = material->GetRoughnessAsset() ? TextureSystem::AddTexture(material->GetRoughnessAsset()->GetTexture()) : 0u;
	const uint32_t aoTextureIndex = material->GetAOAsset() ? TextureSystem::AddTexture(material->GetAOAsset()->GetTexture()) : 0u;
	const uint32_t emissiveTextureIndex = material->GetEmissiveAsset() ? TextureSystem::AddTexture(material->GetEmissiveAsset()->GetTexture()) : 0u;
	const uint32_t opacityTextureIndex = material->GetOpacityAsset() ? TextureSystem::AddTexture(material->GetOpacityAsset()->GetTexture()) : 0u;
	const uint32_t opacityMaskTextureIndex = material->GetOpacityMaskAsset() ? TextureSystem::AddTexture(material->GetOpacityMaskAsset()->GetTexture()) : 0u;

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

	const uint32_t albedoTextureIndex = material->GetAlbedoAsset() ? TextureSystem::AddTexture(material->GetAlbedoAsset()->GetTexture()) : 0u;
	const uint32_t metallnessTextureIndex = material->GetMetallnessAsset() ? TextureSystem::AddTexture(material->GetMetallnessAsset()->GetTexture()) : 0u;
	const uint32_t normalTextureIndex = material->GetNormalAsset() ? TextureSystem::AddTexture(material->GetNormalAsset()->GetTexture()) : 0u;
	const uint32_t roughnessTextureIndex = material->GetRoughnessAsset() ? TextureSystem::AddTexture(material->GetRoughnessAsset()->GetTexture()) : 0u;
	const uint32_t aoTextureIndex = material->GetAOAsset() ? TextureSystem::AddTexture(material->GetAOAsset()->GetTexture()) : 0u;
	const uint32_t emissiveTextureIndex = material->GetEmissiveAsset() ? TextureSystem::AddTexture(material->GetEmissiveAsset()->GetTexture()) : 0u;
	const uint32_t opacityTextureIndex = material->GetOpacityAsset() ? TextureSystem::AddTexture(material->GetOpacityAsset()->GetTexture()) : 0u;
	const uint32_t opacityMaskTextureIndex = material->GetOpacityMaskAsset() ? TextureSystem::AddTexture(material->GetOpacityMaskAsset()->GetTexture()) : 0u;

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
