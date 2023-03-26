#include "egpch.h"
#include "Material.h"

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
	{
	}
}
