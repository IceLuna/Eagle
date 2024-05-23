#pragma once

namespace Eagle
{
	class StaticMesh;
	class SkeletalMesh;
	class AssetMaterial;
	struct SkeletalMeshAnimation;
}

namespace Eagle::Utils
{
	StaticMeshImportData ImportStaticMesh(const Path& path);

	SkeletalMeshImportData ImportSkeletalMesh(const Path& path);

	std::vector<SkeletalMeshAnimation> ImportAnimations(const Path& path, const Ref<SkeletalMesh>& skeletal, bool bRootMotion);

	// Imports materials from a 3D model file
	std::vector<Ref<AssetMaterial>> ImportMaterials(const Path& path, const Path& saveTo);

}
