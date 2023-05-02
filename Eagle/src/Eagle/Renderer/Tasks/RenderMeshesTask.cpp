#include "egpch.h"
#include "RenderMeshesTask.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static constexpr size_t s_BaseVertexBufferSize = 10 * 1024 * 1024; // 10 MB
	static constexpr size_t s_BaseIndexBufferSize = 5 * 1024 * 1024; // 5 MB
	static constexpr size_t s_BaseMaterialBufferSize = 1024 * 1024; // 1 MB

	RenderMeshesTask::RenderMeshesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		// Create buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = s_BaseVertexBufferSize;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = s_BaseIndexBufferSize;
			indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

			BufferSpecifications materialsBufferSpecs;
			materialsBufferSpecs.Size = s_BaseMaterialBufferSize;
			materialsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

			BufferSpecifications tramsformsBufferSpecs;
			tramsformsBufferSpecs.Size = sizeof(glm::mat4) * 100; // 100 transforms
			tramsformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

			m_VertexBuffer = Buffer::Create(vertexSpecs, "Meshes_VertexBuffer");
			m_InstanceVertexBuffer = Buffer::Create(vertexSpecs, "Meshes_InstanceVertexBuffer");
			m_IndexBuffer = Buffer::Create(indexSpecs, "Meshes_IndexBuffer");
			m_MaterialBuffer = Buffer::Create(materialsBufferSpecs, "Meshes_MaterialsBuffer");
			m_MeshTransformsBuffer = Buffer::Create(tramsformsBufferSpecs, "Meshes_TransformsBuffer");
		}
	}

	void RenderMeshesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Meshes.empty())
		{
			// Just to clear images & transition layouts
			cmd->BeginGraphics(m_Pipeline);
			cmd->EndGraphics();
			return;
		}

		UpdateMaterials(); // No caching of materials yet, so need to update anyway
		UploadMaterials(cmd);
		UploadMeshes(cmd);
		UploadTransforms(cmd);
		Render(cmd);
	}

	void RenderMeshesTask::UpdateMaterials()
	{
		// Update CPU buffers
		m_Materials.clear();
		m_Materials.resize(m_MeshesCount);

		for (auto& [mesh, datas] : m_Meshes)
		{
			for (auto& data : datas)
			{
				const uint32_t albedoTextureIndex = TextureSystem::AddTexture(data.Material->GetAlbedoTexture());
				const uint32_t metallnessTextureIndex = TextureSystem::AddTexture(data.Material->GetMetallnessTexture());
				const uint32_t normalTextureIndex = TextureSystem::AddTexture(data.Material->GetNormalTexture());
				const uint32_t roughnessTextureIndex = TextureSystem::AddTexture(data.Material->GetRoughnessTexture());
				const uint32_t aoTextureIndex = TextureSystem::AddTexture(data.Material->GetAOTexture());
				const uint32_t emissiveTextureIndex = TextureSystem::AddTexture(data.Material->GetEmissiveTexture());

				CPUMaterial material;
				material.TintColor = data.Material->TintColor;
				material.EmissiveIntensity = data.Material->EmissiveIntensity;
				material.TilingFactor = data.Material->TilingFactor;

				material.PackedTextureIndices = material.PackedTextureIndices2 = 0;

				material.PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
				material.PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
				material.PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

				material.PackedTextureIndices2 |= (emissiveTextureIndex << EmissiveTextureOffset);
				material.PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
				material.PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

				m_Materials[data.InstanceData.MaterialIndex] = material;
			}
		}
	}

	void RenderMeshesTask::UploadMaterials(const Ref<CommandBuffer>& cmd)
	{
		if (m_Materials.empty())
			return;

		// Update GPU buffer
		const size_t materilBufferSize = m_MaterialBuffer->GetSize();
		const size_t materialDataSize = m_Materials.size() * sizeof(CPUMaterial);

		if (materialDataSize > materilBufferSize)
			m_MaterialBuffer->Resize((materilBufferSize * 3) / 2);

		cmd->Write(m_MaterialBuffer, m_Materials.data(), materialDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(m_MaterialBuffer);
	}

	void RenderMeshesTask::UploadMeshes(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Upload Vertex & Index mesh buffers");
		EG_CPU_TIMING_SCOPED("Upload Vertex & Index mesh buffers");

		if (!bUploadMeshes)
			return;

		bUploadMeshes = false;

		auto& vb = m_VertexBuffer;
		auto& ivb = m_InstanceVertexBuffer;
		auto& ib = m_IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = 0;
		size_t currentIndexSize = 0;
		const size_t currentInstanceVertexSize = m_MeshesCount * sizeof(PerInstanceData);
		for (auto& [meshKey, data] : m_Meshes)
		{
			currentVertexSize += meshKey.Mesh->GetVerticesCount() * sizeof(Vertex);
			currentIndexSize += meshKey.Mesh->GetIndecesCount() * sizeof(Index);
		}

		if (currentVertexSize > vb->GetSize())
		{
			currentVertexSize = (currentVertexSize * 3) / 2;
			vb->Resize(currentVertexSize);
		}
		if (currentInstanceVertexSize > ivb->GetSize())
			vb->Resize((currentInstanceVertexSize * 3) / 2);
		if (currentIndexSize > ib->GetSize())
		{
			currentIndexSize = (currentIndexSize * 3) / 2;
			ib->Resize(currentIndexSize);
		}

		m_Vertices.clear();
		m_Indices.clear();
		m_InstanceVertices.clear();
		m_Vertices.reserve(currentVertexSize);
		m_InstanceVertices.reserve(currentInstanceVertexSize);
		m_Indices.reserve(currentIndexSize);

		for (auto& [meshKey, datas] : m_Meshes)
		{
			const auto& meshVertices = meshKey.Mesh->GetVertices();
			const auto& meshIndices = meshKey.Mesh->GetIndeces();
			m_Vertices.insert(m_Vertices.end(), meshVertices.begin(), meshVertices.end());
			m_Indices.insert(m_Indices.end(), meshIndices.begin(), meshIndices.end());

			for (auto& data : datas)
				m_InstanceVertices.push_back(data.InstanceData);
		}

		cmd->Write(vb, m_Vertices.data(), m_Vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ivb, m_InstanceVertices.data(), currentInstanceVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ib, m_Indices.data(), m_Indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
		cmd->TransitionLayout(ivb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
		cmd->TransitionLayout(ib, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderMeshesTask::UploadTransforms(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Upload Transforms buffer");
		EG_CPU_TIMING_SCOPED("Upload Transforms buffer");

		if (!bUploadMeshTransforms)
			return;

		bUploadMeshTransforms = false;

		const auto& transforms = m_MeshTransforms;
		auto& gpuBuffer = m_MeshTransformsBuffer;

		const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
		if (currentBufferSize > gpuBuffer->GetSize())
			gpuBuffer->Resize((currentBufferSize * 3) / 2);

		cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(gpuBuffer);
	}
	
	void RenderMeshesTask::SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty)
	{
		if (!bDirty)
			return;

		std::unordered_map<MeshKey, std::vector<MeshData>> tempMeshes;
		std::unordered_map<uint32_t, uint64_t> meshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)
		std::vector<glm::mat4> tempMeshTransforms;

		tempMeshes.reserve(meshes.size());
		tempMeshTransforms.reserve(meshes.size());
		meshTransformIndices.reserve(meshes.size());

		uint32_t meshIndex = 0;
		for (auto& comp : meshes)
		{
			const Ref<Eagle::StaticMesh>& staticMesh = comp->GetStaticMesh();
			if (!staticMesh || !staticMesh->IsValid())
				continue;

			const uint32_t meshID = comp->Parent.GetID();
			auto& instanceData = tempMeshes[{staticMesh, staticMesh->GetGUID(), comp->DoesCastShadows()}];
			auto& meshData = instanceData.emplace_back();
			meshData.Material = comp->Material;
			meshData.InstanceData.TransformIndex = meshIndex;
			meshData.InstanceData.MaterialIndex = meshIndex;
			meshData.InstanceData.ObjectID = meshID;

			tempMeshTransforms.push_back(Math::ToTransformMatrix(comp->GetWorldTransform()));
			meshTransformIndices.emplace(meshID, meshIndex);
			++meshIndex;
		}

		RenderManager::Submit([this, meshes = std::move(tempMeshes),
									 transforms = std::move(tempMeshTransforms),
									 transformIndices = std::move(meshTransformIndices),
									 meshesCount = meshIndex](Ref<CommandBuffer>&) mutable
		{
			m_Meshes = std::move(meshes);
			m_MeshTransforms = std::move(transforms);
			m_MeshTransformIndices = std::move(transformIndices);
			m_MeshesCount = meshesCount;

			bUploadMeshes = true;
			bUploadMeshTransforms = true;
		});
	}

	void RenderMeshesTask::UpdateMeshesTransforms(const std::vector<const StaticMeshComponent*>& meshes)
	{
		if (meshes.empty())
			return;

		struct Data
		{
			glm::mat4 TransformMatrix;
			uint32_t ID;
		};

		std::vector<Data> updateData;
		updateData.reserve(meshes.size());

		for (auto& mesh : meshes)
			updateData.push_back({ Math::ToTransformMatrix(mesh->GetWorldTransform()), mesh->Parent.GetID() });

		RenderManager::Submit([this, data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			for (auto& mesh : data)
			{
				auto it = m_MeshTransformIndices.find(mesh.ID);
				if (it != m_MeshTransformIndices.end())
				{
					m_MeshTransforms[it->second] = mesh.TransformMatrix;
					bUploadMeshTransforms = true;
				}
			}
		});
	}

	void RenderMeshesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment geometryNormalAttachment;
		geometryNormalAttachment.Image = gbuffer.GeometryNormal;
		geometryNormalAttachment.InitialLayout = ImageLayoutType::Unknown;
		geometryNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment shadingNormalAttachment;
		shadingNormalAttachment.Image = gbuffer.ShadingNormal;
		shadingNormalAttachment.InitialLayout = ImageLayoutType::Unknown;
		shadingNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.InitialLayout = ImageLayoutType::Unknown;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment materialAttachment;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.InitialLayout = ImageLayoutType::Unknown;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.ClearOperation = ClearOperation::Clear;

		constexpr int objectIDClearColorUint = -1;
		const float objectIDClearColor = *(float*)(&objectIDClearColorUint);
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::Unknown;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Clear;
		objectIDAttachment.ClearColor = glm::vec4{ objectIDClearColor };

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Clear;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometryNormalAttachment);
		state.ColorAttachments.push_back(shadingNormalAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.PerInstanceAttribs = PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
	
	void RenderMeshesTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render meshes");
		EG_CPU_TIMING_SCOPED("Render meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrame = texturesChangedFrame + 1;
		}

		m_Pipeline->SetBuffer(m_MaterialBuffer, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_Pipeline->SetBuffer(m_MeshTransformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);

		const auto& viewProj = m_Renderer.GetViewProjection();
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&viewProj, nullptr);

		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		for (auto& [meshKey, datas] : m_Meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t indicesCount  = (uint32_t)meshKey.Mesh->GetIndeces().size();
			const uint32_t instanceCount = (uint32_t)datas.size();

			cmd->DrawIndexedInstanced(m_VertexBuffer, m_IndexBuffer, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, m_InstanceVertexBuffer);

			firstIndex += indicesCount;
			vertexOffset += verticesCount;
			firstInstance += instanceCount;
		}

		cmd->EndGraphics();
	}
}
