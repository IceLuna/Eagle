#include "egpch.h"
#include "RenderMeshesTask.h"

#include "Eagle/Classes/StaticMesh.h"
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

	RenderMeshesTask::RenderMeshesTask(SceneRenderer& renderer, bool bClearImages)
		: RendererTask(renderer), m_ClearImages(bClearImages)
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
		for (auto& mesh : m_Meshes)
		{
			uint32_t albedoTextureIndex = TextureSystem::AddTexture(mesh.Material->GetAlbedoTexture());
			uint32_t metallnessTextureIndex = TextureSystem::AddTexture(mesh.Material->GetMetallnessTexture());
			uint32_t normalTextureIndex = TextureSystem::AddTexture(mesh.Material->GetNormalTexture());
			uint32_t roughnessTextureIndex = TextureSystem::AddTexture(mesh.Material->GetRoughnessTexture());
			uint32_t aoTextureIndex = TextureSystem::AddTexture(mesh.Material->GetAOTexture());

			CPUMaterial material;
			material.TintColor = mesh.Material->TintColor;
			material.TilingFactor = mesh.Material->TilingFactor;

			material.PackedTextureIndices = material.PackedTextureIndices2 = 0;

			material.PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
			material.PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
			material.PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

			material.PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
			material.PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

			m_Materials.push_back(material);
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
		EG_GPU_TIMING_SCOPED(cmd, "Upload Vertex & Index buffers");
		EG_CPU_TIMING_SCOPED("Upload Vertex & Index buffers");

		if (!bUploadMeshes)
			return;

		bUploadMeshes = false;

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = 0;
		size_t currentIndexSize = 0;
		for (auto& mesh : m_Meshes)
		{
			currentVertexSize += mesh.Mesh->GetVerticesCount() * sizeof(Vertex);
			currentIndexSize += mesh.Mesh->GetIndecesCount() * sizeof(Index);
		}

		if (currentVertexSize > vb->GetSize())
		{
			currentVertexSize = (currentVertexSize * 3) / 2;
			vb->Resize(currentVertexSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			currentIndexSize = (currentIndexSize * 3) / 2;
			ib->Resize(currentIndexSize);
		}

		static std::vector<Vertex> vertices;
		static std::vector<Index> indices;
		vertices.clear(); vertices.reserve(currentVertexSize);
		indices.clear(); indices.reserve(currentIndexSize);

		for (auto& mesh : m_Meshes)
		{
			const auto& meshVertices = mesh.Mesh->GetVertices();
			const auto& meshIndices = mesh.Mesh->GetIndeces();
			vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
			indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());
		}

		cmd->Write(vb, vertices.data(), vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ib, indices.data(), indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
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

		std::vector<MeshData> tempMeshes;
		std::vector<glm::mat4> tempMeshTransforms;
		tempMeshes.reserve(meshes.size());
		tempMeshTransforms.reserve(meshes.size());

		for (auto& comp : meshes)
		{
			const Ref<Eagle::StaticMesh>& staticMesh = comp->GetStaticMesh();
			if (!staticMesh->IsValid())
				continue;

			tempMeshes.push_back({ staticMesh, comp->Material, comp->Parent.GetID() });
			tempMeshTransforms.push_back(Math::ToTransformMatrix(comp->GetWorldTransform()));
		}

		RenderManager::Submit([this, meshes = std::move(tempMeshes), transforms = std::move(tempMeshTransforms)](Ref<CommandBuffer>&) mutable
		{
			m_Meshes = std::move(meshes);
			m_MeshTransforms = std::move(transforms);
			const size_t meshesCount = m_Meshes.size();

			m_MeshTransformIndices.clear();
			m_MeshTransformIndices.reserve(meshesCount);

			for (size_t i = 0; i < meshesCount; ++i)
			{
				const uint32_t id = m_Meshes[i].ID;
				m_MeshTransformIndices[id] = i;
			}

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
		colorAttachment.bClearEnabled = m_ClearImages;

		ColorAttachment geometryNormalAttachment;
		geometryNormalAttachment.Image = gbuffer.GeometryNormal;
		geometryNormalAttachment.InitialLayout = ImageLayoutType::Unknown;
		geometryNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.bClearEnabled = m_ClearImages;

		ColorAttachment shadingNormalAttachment;
		shadingNormalAttachment.Image = gbuffer.ShadingNormal;
		shadingNormalAttachment.InitialLayout = ImageLayoutType::Unknown;
		shadingNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.bClearEnabled = m_ClearImages;

		ColorAttachment materialAttachment;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.InitialLayout = ImageLayoutType::Unknown;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.bClearEnabled = m_ClearImages;

		constexpr int objectIDClearColorUint = -1;
		const float objectIDClearColor = *(float*)(&objectIDClearColorUint);
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::Unknown;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.bClearEnabled = m_ClearImages;
		objectIDAttachment.ClearColor = glm::vec4{ objectIDClearColor };

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.bClearEnabled = m_ClearImages;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometryNormalAttachment);
		state.ColorAttachments.push_back(shadingNormalAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
	
	void RenderMeshesTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render meshes");
		EG_CPU_TIMING_SCOPED("Render meshes");

		struct VertexPushData
		{
			glm::mat4 ViewProj;
			uint32_t TransformIndex = 0;
			uint32_t MaterialIndex = 0;
		} pushData;

		pushData.ViewProj = m_Renderer.GetViewProjection();

		uint32_t firstIndex = 0;
		uint32_t vertexOffset = 0;
		uint32_t meshIndex = 0;

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrame = texturesChangedFrame + 1;
		}

		m_Pipeline->SetBuffer(m_MaterialBuffer, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_Pipeline->SetBuffer(m_MeshTransformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		
		auto& meshes = m_Meshes;
		const size_t meshesCount = meshes.size();
		cmd->BeginGraphics(m_Pipeline);
		for (size_t i = 0; i < meshesCount; ++i)
		{
			const auto& mesh = meshes[i];
			const auto& vertices = mesh.Mesh->GetVertices();
			const auto& indices = mesh.Mesh->GetIndeces();
			const size_t vertexSize = vertices.size() * sizeof(Vertex);
			const size_t indexSize = indices.size() * sizeof(Index);

			pushData.TransformIndex = uint32_t(i);
			pushData.MaterialIndex = meshIndex;

			cmd->SetGraphicsRootConstants(&pushData, &mesh.ID);
			cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, (uint32_t)indices.size(), firstIndex, vertexOffset);
			firstIndex += (uint32_t)indices.size();
			vertexOffset += (uint32_t)vertices.size();
			meshIndex++;
		}
		cmd->EndGraphics();
	}
}
