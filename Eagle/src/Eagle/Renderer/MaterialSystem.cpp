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
	std::unordered_map<Ref<Material>, uint32_t> MaterialSystem::s_UsedMaterialsMap;
	bool MaterialSystem::s_Dirty = true;
	bool MaterialSystem::s_Changed = true;
	bool MaterialSystem::s_RenderingModeChanged = true;

	static std::vector<CPUMaterial> s_CPUMaterials;
	static std::vector<float> s_CPURawMaterials;

	static constexpr size_t s_BaseMaterialsBuffer = 100ull * sizeof(CPUMaterial);
	static constexpr size_t s_BaseMaterialsRawBuffer = 1024ull * sizeof(float);
	static constexpr uint32_t s_DummyMaterialIndex = 0u;

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
			const uint32_t index = (uint32_t)s_Materials.size();
			s_Materials.push_back(material);
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
			s_Materials.erase(s_Materials.begin() + it->second);
			s_UsedMaterialsMap.erase(it);
			SetDirty_Internal();
		}
	}

	void MaterialSystem::Update(const Ref<CommandBuffer>& cmd)
	{
		std::scoped_lock lock(s_Mutex);
		
		if (!s_Dirty)
		{
			// This way the value is saved till the end of the frame.
			// So if materials were changed, `s_Changed` won't reset until the next frame
			s_Changed = false;
			s_RenderingModeChanged = false;
			return;
		}

		EG_GPU_TIMING_SCOPED(cmd, "Material system. Update");
		EG_CPU_TIMING_SCOPED("Material system. Update");

		// Remove unused materials
		{
			bool bChanged = false;
			std::vector<Ref<Material>> materialsInUse;
			materialsInUse.reserve(s_Materials.size());
			for (auto& material : s_Materials)
			{
				// Why 2? Because material system itself stores two Ref<Material>
				// So if `use_count == 2`, that means that material is not used
				if (material.use_count() > 2)
					materialsInUse.emplace_back(std::move(material));
				else
					bChanged = true;
			}
			s_Materials = std::move(materialsInUse);
			if (bChanged)
			{
				s_UsedMaterialsMap.clear();
				uint32_t index = 0;
				for (auto& material : s_Materials)
					s_UsedMaterialsMap[material] = index++;
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
		return it->second + 1u; // +1 because [0] is always the dummy material
	}
	
	void MaterialSystem::SetDirty()
	{
		std::scoped_lock lock(s_Mutex);
		
		s_Dirty = s_Changed = s_RenderingModeChanged = true;
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
}
