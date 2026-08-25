#include "egpch.h"
#include "MaterialSystem.h"
#include "Material.h"

#include "VidWrappers/Buffer.h"
#include "VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
	std::vector<Ref<Material>> MaterialSystem::s_Materials;
	Ref<Buffer> MaterialSystem::s_MaterialsBuffer;
	Ref<Buffer> MaterialSystem::s_MaterialsRawBuffer;
	ankerl::unordered_dense::map<Ref<Material>, uint32_t> MaterialSystem::s_UsedMaterialsMap;
	std::vector<size_t> MaterialSystem::s_FreeIndices; // Free slots inside `s_Materials`
	bool MaterialSystem::s_Dirty = true;
	bool MaterialSystem::s_RenderingModeChanged = true;

	static std::vector<CPUMaterial> s_CPUMaterials;
	static std::vector<float> s_CPURawMaterials;

	static constexpr size_t s_BaseMaterialsBuffer = 100ull * sizeof(CPUMaterial);
	static constexpr size_t s_BaseMaterialsRawBuffer = 1024ull * sizeof(float);
	static constexpr uint32_t s_DummyMaterialIndex = 0u;
	static constexpr uint32_t s_DummyMaterialOffset = s_DummyMaterialIndex + 1u;

	static std::mutex s_Mutex;

	void MaterialSystem::Init()
	{
		BufferSpecifications specs;
		specs.Usage = BufferUsage::TransferDst | BufferUsage::StorageBuffer;
		specs.Size = s_BaseMaterialsBuffer;
		s_MaterialsBuffer = Buffer::Create(specs, "Materials");

		specs.Size = s_BaseMaterialsRawBuffer;
		s_MaterialsRawBuffer = Buffer::Create(specs, "RawMaterials");
	}

	void MaterialSystem::Shutdown()
	{
		Reset();
		s_MaterialsBuffer.reset();
		s_MaterialsRawBuffer.reset();
	}

	void MaterialSystem::Reset()
	{
		s_CPUMaterials.clear();
		s_CPURawMaterials.clear();
		s_Materials.clear();
		s_UsedMaterialsMap.clear();
		s_FreeIndices.clear();
		SetDirty_Internal();
	}
	
	void MaterialSystem::AddMaterial(const Ref<Material>& material)
	{
		if (!material)
			return;

		std::scoped_lock lock(s_Mutex);

		auto it = s_UsedMaterialsMap.find(material);
		if (it == s_UsedMaterialsMap.end())
		{
			uint32_t index = 0;
			if (s_FreeIndices.empty())
			{
				index = (uint32_t)s_Materials.size();
				s_Materials.push_back(material);
			}
			else
			{
				index = (uint32_t)s_FreeIndices.back();
				s_Materials[index] = material;
				s_FreeIndices.pop_back();
			}
			s_UsedMaterialsMap[material] = index;
			SetDirty_Internal();
		}
	}

	void MaterialSystem::RemoveMaterial(const Ref<Material>& material)
	{
		std::scoped_lock lock(s_Mutex);
		
		auto it = s_UsedMaterialsMap.find(material);
		if (it != s_UsedMaterialsMap.end())
		{
			s_Materials[it->second] = s_Materials[s_DummyMaterialIndex];
			s_FreeIndices.push_back(it->second);
			s_UsedMaterialsMap.erase(it);
			SetDirty_Internal();
		}
	}

	void MaterialSystem::Update(const Ref<CommandBuffer>& cmd)
	{
		std::scoped_lock lock(s_Mutex);
		
		if (!s_Dirty)
		{
			s_RenderingModeChanged = false;
			return;
		}

		EG_GPU_TIMING_SCOPED(cmd, "Material system. Update");
		EG_CPU_TIMING_SCOPED("Material system. Update");

		// Remove unused materials
		{
			const size_t size = s_Materials.size();
			for (size_t i = s_DummyMaterialOffset; i < size; ++i)
			{
				auto& material = s_Materials[i];

				// Why 2? Because material system itself stores two Ref<Material>
				// So if `use_count == 2`, that means that material is not used
				if (material.use_count() <= 2)
				{
					s_UsedMaterialsMap.erase(material);
					material = s_Materials[s_DummyMaterialIndex];
					s_FreeIndices.push_back(i);
				}
			}
		}

		s_CPUMaterials.clear();
		s_CPUMaterials.reserve(s_Materials.size() + 1); // +1 because [0] is always the dummy material
		s_CPUMaterials.emplace_back();

		s_CPURawMaterials.clear();
		s_CPURawMaterials.reserve(s_MaterialsRawBuffer->GetSize() / sizeof(float));
		s_CPURawMaterials.push_back(0.f); // Dummy shader-fallback value
		s_CPURawMaterials.push_back(1.f); // Dummy shader-fallback value

		for (auto& material : s_Materials)
			s_CPUMaterials.emplace_back(CPUMaterial::Convert(material, s_CPURawMaterials));

		const size_t materialBufferSize = s_MaterialsBuffer->GetSize();
		const size_t materialRawBufferSize = s_MaterialsRawBuffer->GetSize();
		const size_t materialDataSize = s_CPUMaterials.size() * sizeof(CPUMaterial);
		const size_t materialRawDataSize = s_CPURawMaterials.size() * sizeof(float);

		if (materialDataSize > materialBufferSize)
			s_MaterialsBuffer->Resize((materialDataSize * 3) / 2);
		if (materialRawDataSize > materialRawBufferSize)
			s_MaterialsRawBuffer->Resize((materialRawDataSize * 3) / 2);

		cmd->Write(s_MaterialsBuffer, s_CPUMaterials.data(), materialDataSize, 0, s_MaterialsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		cmd->Write(s_MaterialsRawBuffer, s_CPURawMaterials.data(), materialRawDataSize, 0, s_MaterialsRawBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

		s_Dirty = false;
	}
	
	uint32_t MaterialSystem::GetMaterialIndex(const Ref<Material>& material)
	{
		if (!material)
			return s_DummyMaterialIndex;

		std::scoped_lock lock(s_Mutex);
		
		auto it = s_UsedMaterialsMap.find(material);
		if (it == s_UsedMaterialsMap.end())
		{
			EG_CORE_ERROR("Internal error: didn't find the material");
			return s_DummyMaterialIndex;
		}
		return it->second + s_DummyMaterialOffset;
	}
	
	void MaterialSystem::SetDirty()
	{
		std::scoped_lock lock(s_Mutex);
		
		s_Dirty = s_RenderingModeChanged = true;
	}

	void MaterialSystem::OnMaterialChanged(const Ref<Material>& material, bool bRenderingModeChanged)
	{
		if (!material)
			return;

		std::scoped_lock lock(s_Mutex);

		auto it = s_UsedMaterialsMap.find(material);
		if (it != s_UsedMaterialsMap.end())
		{
			SetDirty_Internal();
			s_RenderingModeChanged |= bRenderingModeChanged;
		}
	}

	void MaterialSystem::SetDirty_Internal()
	{
		s_Dirty = true;
	}
}
