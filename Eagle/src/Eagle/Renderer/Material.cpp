#include "egpch.h"
#include "Material.h"

#include "Renderer.h"
#include "PipelineGraphics.h"

namespace Eagle
{
    Material::Material(const Ref<Material>& other)
		: m_AlbedoTexture(other->m_AlbedoTexture)
		, m_NormalTexture(other->m_NormalTexture)
		, m_MetallnessTexture(other->m_MetallnessTexture)
		, m_RoughnessTexture(other->m_RoughnessTexture)
		, TintColor(other->TintColor)
		, TilingFactor(other->TilingFactor)
		, Shininess(other->Shininess)
	{
	}
}
