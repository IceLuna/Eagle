#pragma once

#include "Material.h"

namespace Eagle
{
	class Buffer;
	class CommandBuffer;

	class MaterialSystem
	{
	public:
		static void Init();
		static void Shutdown();

		static void AddMaterial(const Ref<Material>& material);
		static void RemoveMaterial(const Ref<Material>& material);
		static void Update(const Ref<CommandBuffer>& cmd);

		static uint32_t GetMaterialIndex(const Ref<Material>& material);
		static bool HasChanged() { return s_Changed; }

		static const Ref<Buffer>& GetMaterialsBuffer() { return s_MaterialsBuffer; }

	private:
		static void OnMaterialChanged(const Ref<Material>& material);
		static void SetDirty()
		{
			s_Dirty = s_Changed = true;
		}

	private:
		static std::vector<Ref<Material>> s_Materials;
		static Ref<Buffer> s_MaterialsBuffer; // GPU buffer
		static std::unordered_map<Ref<Material>, uint32_t> s_UsedMaterialsMap; // uint32_t = index to s_Materials

		// If true, new materials were added
		static bool s_Dirty;
		// If true, new materials were added. The difference is that this flag is reset at the end of the frame. Basically every other system can see that materials have changed.
		static bool s_Changed;

		friend class Material;
	};
}
