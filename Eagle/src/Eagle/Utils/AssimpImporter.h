#pragma once

namespace Eagle
{
	class StaticMesh;
	class SkeletalMesh;
	class AssetMaterial;
	struct SkeletalMeshAnimation;
	enum class RootMotionMode;
}

namespace Eagle::Utils
{
	// @bCombineMeshes. If true, all meshes found in the file are merged into a single asset.
	//		If false, every mesh in the file is returned as its own separate entry so it can be imported as an independent asset.
	// @bResetLocation. Only has an effect when `bCombineMeshes == false`. If true, each returned mesh's
	//		own translation (its position within the file) is stripped, so it ends up centered at (0, 0, 0)
	//		instead of keeping its original place in the scene. Rotation & scale are kept.
	std::vector<StaticMeshImportData> ImportStaticMesh(const Path& path, bool bCombineMeshes = true, bool bResetLocation = false);

	// @bCombineMeshes. Same as above, but for skeletal meshes. Note: regardless of this flag,
	//		all returned meshes share the same skeleton (bones & root bone), since they come from the same armature.
	// @bResetLocation. Same as above. Doesn't affect the skeleton/bone hierarchy or skinning - only the mesh's own rest-pose location.
	std::vector<SkeletalMeshImportData> ImportSkeletalMesh(const Path& path, bool bCombineMeshes = true, bool bResetLocation = false);

	std::vector<SkeletalMeshAnimation> ImportAnimations(const Path& path, const Ref<SkeletalMesh>& skeletal, const RootMotionMode& rootMotionMode);

	// Imports materials from a 3D model file
	std::vector<Ref<AssetMaterial>> ImportMaterials(const Path& path, const Path& saveTo);

}
