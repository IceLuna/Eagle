#pragma once

namespace Eagle
{
	class Buffer;
	class CommandBuffer;
	class Material;

	class MaterialSystem
	{
	public:
		static void Init();
		static void Shutdown();
		static void Reset(); // Doesn't release the GPU buffer

		static void AddMaterial(const Ref<Material>& material);
		static void RemoveMaterial(const Ref<Material>& material);
		static void Update(const Ref<CommandBuffer>& cmd);

		static uint32_t GetMaterialIndex(const Ref<Material>& material);
		static bool HasRenderingModeChanged() { return s_RenderingModeChanged; }

		static const Ref<Buffer>& GetMaterialsBuffer() { return s_MaterialsBuffer; }
		static const Ref<Buffer>& GetMaterialsRawBuffer() { return s_MaterialsRawBuffer; }

		static void SetDirty();

		// Indices to `0.0` and `1.0` values inside s_MaterialsRawBuffer
		static const uint32_t ZeroRawIndex = 0u;
		static const uint32_t OneRawIndex = 1u;

	private:
		static void OnMaterialChanged(const Ref<Material>& material, bool bRenderingModeChanged = false);
		static void SetDirty_Internal();

	private:
		static std::vector<Ref<Material>> s_Materials;
		static Ref<Buffer> s_MaterialsBuffer; // GPU buffer
		static Ref<Buffer> s_MaterialsRawBuffer; // GPU buffer of raw values
		static ankerl::unordered_dense::map<Ref<Material>, uint32_t> s_UsedMaterialsMap; // uint32_t = index to s_Materials
		static std::vector<size_t> s_FreeIndices; // Free slots inside `s_Materials`

		// If true, materials were changed or new ones were added
		static bool s_Dirty;
		static bool s_RenderingModeChanged; // Either blend mode or double-sided state have changed

		friend class Material;
	};
}
