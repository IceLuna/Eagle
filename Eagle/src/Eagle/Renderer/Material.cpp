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

CPUMaterial CPUMaterial::Convert(const Eagle::Ref<Eagle::Material>& material, std::vector<float>& rawValues)
{
	using namespace Eagle;
	CPUMaterial result;
	result.TintColor = material->GetTintColor();
	result.EmissiveIntensity = material->GetEmissiveIntensity();
	result.TilingFactor = material->GetTilingFactor();

	uint32_t albedoIndex = 0u;
	if (material->IsRawAlbedoUsed())
	{
		albedoIndex = (uint32_t)rawValues.size();
		const glm::vec3 albedo = material->GetAlbedo();
		for (uint32_t i = 0; i < 3; ++i)
			rawValues.push_back(albedo[i]);
	}
	else if (const auto& asset = material->GetAlbedoAsset())
		albedoIndex = TextureSystem::AddTexture(asset->GetTexture());

	uint32_t metalnessIndex = 0u;
	if (material->IsRawMetalnessUsed())
	{
		metalnessIndex = (uint32_t)rawValues.size();
		rawValues.push_back(material->GetMetalness());
	}
	else if (const auto& asset = material->GetMetalnessAsset())
		metalnessIndex = TextureSystem::AddTexture(asset->GetTexture());

	const uint32_t normalIndex = material->GetNormalAsset() ? TextureSystem::AddTexture(material->GetNormalAsset()->GetTexture()) : 0u;

	uint32_t roughnessIndex = 0u;
	if (material->IsRawRoughnessUsed())
	{
		roughnessIndex = (uint32_t)rawValues.size();
		rawValues.push_back(material->GetRoughness());
	}
	else if (const auto& asset = material->GetRoughnessAsset())
		roughnessIndex = TextureSystem::AddTexture(asset->GetTexture());

	uint32_t aoIndex = 0u;
	if (material->IsRawAOUsed())
	{
		aoIndex = (uint32_t)rawValues.size();
		rawValues.push_back(material->GetAO());
	}
	else if (const auto& asset = material->GetAOAsset())
		aoIndex = TextureSystem::AddTexture(asset->GetTexture());

	uint32_t emissiveIndex = 0u;
	if (material->IsRawEmissiveUsed())
	{
		emissiveIndex = (uint32_t)rawValues.size();
		const glm::vec3 emissive = material->GetEmissive();
		for (uint32_t i = 0; i < 3; ++i)
			rawValues.push_back(emissive[i]);
	}
	else if (const auto& asset = material->GetEmissiveAsset())
		emissiveIndex = TextureSystem::AddTexture(asset->GetTexture());

	uint32_t opacityIndex = 0u;
	if (material->IsRawOpacityUsed())
	{
		opacityIndex = (uint32_t)rawValues.size();
		rawValues.push_back(material->GetOpacity());
	}
	else if (const auto& asset = material->GetOpacityAsset())
		opacityIndex = TextureSystem::AddTexture(asset->GetTexture());

	uint32_t opacityMaskIndex = 0u;
	if (material->IsRawOpacityMaskUsed())
	{
		opacityMaskIndex = (uint32_t)rawValues.size();
		rawValues.push_back(material->GetOpacityMask());
	}
	else if (const auto& asset = material->GetOpacityMaskAsset())
		opacityMaskIndex = TextureSystem::AddTexture(asset->GetTexture());

	result.PackedIndices = result.PackedIndices2 = result.PackedIndices3 = result.PackedIndices4 = 0u;
	
	// Pack 1
	result.PackedIndices   = ( albedoIndex    & MaterialIndexMask) | (material->IsRawAlbedoUsed()    ? IsRawValueMask : 0u);
	result.PackedIndices  |= ((metalnessIndex & MaterialIndexMask) | (material->IsRawMetalnessUsed() ? IsRawValueMask : 0u)) << MetalnessIndexOffset;

	// Pack 2
	result.PackedIndices2  = ( normalIndex    & MaterialIndexMask);
	result.PackedIndices2 |= ((roughnessIndex & MaterialIndexMask) | (material->IsRawRoughnessUsed() ? IsRawValueMask : 0u)) << RoughnessIndexOffset;

	// Pack 3
	result.PackedIndices3  = ( aoIndex        & MaterialIndexMask) | (material->IsRawAOUsed()        ? IsRawValueMask : 0u);
	result.PackedIndices3 |= ((emissiveIndex  & MaterialIndexMask) | (material->IsRawEmissiveUsed()  ? IsRawValueMask : 0u)) << EmissiveIndexOffset;

	// Pack 4
	result.PackedIndices4  = ( opacityIndex     & MaterialIndexMask) | (material->IsRawOpacityUsed()     ? IsRawValueMask : 0u);
	result.PackedIndices4 |= ((opacityMaskIndex & MaterialIndexMask) | (material->IsRawOpacityMaskUsed() ? IsRawValueMask : 0u)) << OpacityMaskIndexOffset;

	return result;
}
