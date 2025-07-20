#include "egpch.h"
#include "ParticleEmitter.h"

#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	bool ParticleEmitter::IsSkeletalMeshUsed() const
	{
		return EmissionShape == ParticleEmitter::EmissionShapeType::Mesh && MeshAsset && MeshAsset->GetAssetType() == AssetType::SkeletalMesh;
	}
}
