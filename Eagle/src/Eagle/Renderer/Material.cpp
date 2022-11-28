#include "egpch.h"
#include "Material.h"

#include "Renderer.h"
#include "PipelineGraphics.h"

namespace Eagle
{
    Material::Material(const Ref<Material>& other)
		: m_DiffuseTexture(other->m_DiffuseTexture)
		, m_SpecularTexture(other->m_SpecularTexture)
		, m_NormalTexture(other->m_NormalTexture)
		, TintColor(other->TintColor)
		, TilingFactor(other->TilingFactor)
		, Shininess(other->Shininess)
	{
	}
}
