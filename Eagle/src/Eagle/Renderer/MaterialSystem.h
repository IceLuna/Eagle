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
		static bool HasBlendModeChanged() { return s_BlendModeChanged; }

		static const Ref<Buffer>& GetMaterialsBuffer() { return s_MaterialsBuffer; }
		static const Ref<Buffer>& GetMaterialsRawBuffer() { return s_MaterialsRawBuffer; }

		static void SetDirty();

		// Indices to `0.0` and `1.0` values inside s_MaterialsRawBuffer
		static const uint32_t ZeroRawIndex = 0u;
		static const uint32_t OneRawIndex = 1u;

	private:
		static void OnMaterialChanged(const Ref<Material>& material, bool bBlendModeChanged = false);
		static void SetDirty_Internal()
		{
			s_Dirty = s_Changed = true;
		}

	private:
		static std::vector<Ref<Material>> s_Materials;
		static Ref<Buffer> s_MaterialsBuffer; // GPU buffer
		static Ref<Buffer> s_MaterialsRawBuffer; // GPU buffer of raw values
		static std::unordered_map<Ref<Material>, uint32_t> s_UsedMaterialsMap; // uint32_t = index to s_Materials

		// If true, materials were changed or new ones were added
		static bool s_Dirty;
		// If true, materials were changed or new ones were added.
		// The difference is that this flag is reset at the beginning of the next frame. So that every other system can see that materials have changed.
		static bool s_Changed;
		static bool s_BlendModeChanged;

		friend class Material;
	};
}
