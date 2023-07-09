#include "egpch.h"
#include "MaterialSystem.h"

#include "VidWrappers/Buffer.h"
#include "VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
	std::vector<Ref<Material>> MaterialSystem::s_Materials;
	Ref<Buffer> MaterialSystem::s_MaterialsBuffer;
	std::unordered_map<Ref<Material>, uint32_t> MaterialSystem::s_UsedMaterialsMap;
	bool MaterialSystem::s_Dirty = true;
	bool MaterialSystem::s_Changed = true;

	static constexpr size_t s_BaseMaterialsBuffer = 100ull * sizeof(CPUMaterial);
	static constexpr uint32_t s_DummyMaterialIndex = 0u;

	void MaterialSystem::Init()
	{
		BufferSpecifications specs;
		specs.Usage = BufferUsage::TransferDst | BufferUsage::StorageBuffer;
		specs.Size = s_BaseMaterialsBuffer;
		s_MaterialsBuffer = Buffer::Create(specs, "Materials");
	}

	void MaterialSystem::Shutdown()
	{
		s_Materials.clear();
		s_MaterialsBuffer.reset();
		s_UsedMaterialsMap.clear();
		SetDirty();
	}
	
	void MaterialSystem::AddMaterial(const Ref<Material>& material)
	{
		if (!material)
			return;

		auto it = s_UsedMaterialsMap.find(material);
		if (it == s_UsedMaterialsMap.end())
		{
			const uint32_t index = (uint32_t)s_Materials.size();
			s_Materials.push_back(material);
			s_UsedMaterialsMap[material] = index;
			SetDirty();
		}
	}

	void MaterialSystem::RemoveMaterial(const Ref<Material>& material)
	{
		auto it = s_UsedMaterialsMap.find(material);
		if (it != s_UsedMaterialsMap.end())
		{
			s_Materials.erase(s_Materials.begin() + it->second);
			s_UsedMaterialsMap.erase(it);
			SetDirty();
		}
	}

	void MaterialSystem::Update(const Ref<CommandBuffer>& cmd)
	{
		if (!s_Dirty)
		{
			// This way the value is saved till the end of the frame.
			// So if materials were changed, `s_Changed` won't reset until the next frame
			s_Changed = false;
			return;
		}

		EG_GPU_TIMING_SCOPED(cmd, "Material system. Update");
		EG_CPU_TIMING_SCOPED("Material system. Update");

		{
			// Remove unused materials
			bool bChanged = false;
			std::vector<Ref<Material>> tempMaterials;
			tempMaterials.reserve(s_Materials.size());
			for (auto& material : s_Materials)
			{
				// Why 2? Because material system itself stores two Ref<Material>
				// So if `use_count == 2`, that means that material is not used
				if (material.use_count() > 2)
					tempMaterials.emplace_back(std::move(material));
				else
					bChanged = true;
			}
			s_Materials = std::move(tempMaterials);
			if (bChanged)
			{
				s_UsedMaterialsMap.clear();
				uint32_t index = 0;
				for (auto& material : s_Materials)
					s_UsedMaterialsMap[material] = index++;
			}
		}

		std::vector<CPUMaterial> materials;
		materials.reserve(s_Materials.size() + 1); // +1 because [0] is always the dummy material
		materials.emplace_back();

		for (auto& material : s_Materials)
		{
			if (materials.emplace_back(material).TilingFactor == 0.f)
				EG_DEBUGBREAK();
		}

		const size_t materialBufferSize = s_MaterialsBuffer->GetSize();
		const size_t materialDataSize = materials.size() * sizeof(CPUMaterial);

		if (materialDataSize > materialBufferSize)
			s_MaterialsBuffer->Resize((materialDataSize * 3) / 2);

		cmd->Write(s_MaterialsBuffer, materials.data(), materialDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(s_MaterialsBuffer);

		s_Dirty = false;
	}
	
	uint32_t MaterialSystem::GetMaterialIndex(const Ref<Material>& material)
	{
		auto it = s_UsedMaterialsMap.find(material);
		if (it == s_UsedMaterialsMap.end())
		{
			EG_CORE_WARN("Didn't find the material");
			return s_DummyMaterialIndex;
		}
		return it->second + 1u; // +1 because [0] is always the dummy material
	}
	
	void MaterialSystem::OnMaterialChanged(const Ref<Material>& material)
	{
		if (!material)
			return;

		auto it = s_UsedMaterialsMap.find(material);
		if (it != s_UsedMaterialsMap.end())
		{
			const uint32_t index = it->second;
			s_Materials[index] = material;
			SetDirty();
		}
	}
}
