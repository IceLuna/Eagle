#include "egpch.h"
#include "RenderDecalsTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static glm::mat4 CalculateDecalVP(const DecalComponent* decalComp)
	{
		constexpr float m_projector_size = 0.5f;
		const auto& worldTr = decalComp->GetWorldTransform();
		const glm::vec3 normal = glm::rotate(worldTr.Rotation.GetQuat(), glm::vec3(0, 0, -1));
		const glm::vec3 projectorPos = worldTr.Location + normal;
		const glm::vec3 projectorDir = -normal;

		const glm::mat4 rotate = glm::rotate(glm::mat4(1.f), 0.f, projectorDir);
		const glm::vec3 default_up = glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 rotated_axis = rotate * glm::vec4(default_up, 0.0f);

		// Scaling along Z. (far - near) * 2 = ScaleZ; => far = ScaleZ / 2 + near
		const float nearZ = 1.f - (worldTr.Scale3D.z * 0.5f);
		const float farZ = worldTr.Scale3D.z * 0.5f + nearZ;

		const glm::mat4 view = glm::lookAt(projectorPos, worldTr.Location, glm::normalize(rotated_axis));
		glm::mat4 proj = Math::Ortho(-m_projector_size, m_projector_size, -m_projector_size, m_projector_size, nearZ, farZ);
		proj = proj * glm::scale(glm::mat4(1.f), 1.f / glm::vec3(worldTr.Scale3D.x, worldTr.Scale3D.y, 1.f)); // Z-scaling is applied above.
		// Flipping for Vulkan
		proj[1][1] *= -1.f;

		return proj * view;
	}

	RenderDecalsTask::RenderDecalsTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications specs{};
		specs.Size = sizeof(glm::mat4) * 100u;
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_TransformsBuffer = Buffer::Create(specs, "Decals_Transforms");

		specs.Size = sizeof(DecalData) * 100u;
		specs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;
		m_InstanceBuffer = Buffer::Create(specs, "Decals_Instances");
	}

	void RenderDecalsTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Decals.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Decals");
		EG_CPU_TIMING_SCOPED("Decals");
		Upload(cmd);
		Render(cmd);
	}

	void RenderDecalsTask::Upload(const Ref<CommandBuffer>& cmd)
	{
		if (!bUpload && !bUploadTransforms)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Decals. Upload");
		EG_CPU_TIMING_SCOPED("Decals. Upload");

		if (bUploadTransforms)
		{
			const size_t currentTransformsSize = m_Transforms.size() * sizeof(glm::mat4);
			if (currentTransformsSize > m_TransformsBuffer->GetSize())
			{
				size_t newSize = (currentTransformsSize * 3) / 2;
				m_TransformsBuffer->Resize(newSize);
			}
			cmd->Write(m_TransformsBuffer, m_Transforms.data(), m_Transforms.size() * sizeof(glm::mat4), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		}

		if (bUpload)
		{
			const size_t currentInstancesSize = m_Decals.size() * sizeof(DecalData);
			if (currentInstancesSize > m_InstanceBuffer->GetSize())
			{
				size_t newSize = (currentInstancesSize * 3) / 2;
				m_InstanceBuffer->Resize(newSize);
			}
			cmd->Write(m_InstanceBuffer, m_Decals.data(), m_Decals.size() * sizeof(DecalData), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		}

		bUpload = false;
		bUploadTransforms = false;
	}

	void RenderDecalsTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Decals. Render");
		EG_CPU_TIMING_SCOPED("Decals. Render");
		
		auto& gbuffer = m_Renderer.GetGBuffer();
		auto& depth = gbuffer.Depth;
		const auto oldLayout = depth->GetLayout();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
		m_Pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_Pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_Pipeline->SetBuffer(m_TransformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_Pipeline->SetImageSampler(depth, Sampler::PointSampler, 3, 0);
		m_Pipeline->SetImageSampler(gbuffer.Flags, Sampler::PointSampler, 3, 1);

		const glm::mat4& vp = m_Renderer.GetViewProjection();
		const glm::mat4& invVP = m_Renderer.GetInverseViewProjection();
		const uint32_t instanceCount = (uint32_t)m_Decals.size();

		cmd->TransitionLayout(depth, oldLayout, ImageReadAccess::PixelShaderRead);

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&vp, &invVP);
		cmd->DrawInstanced(m_InstanceBuffer, 36, instanceCount, 0, 0);
		cmd->EndGraphics();

		cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, oldLayout);
	}

	void RenderDecalsTask::SetDecals(const std::vector<const DecalComponent*>& decals, bool bDirty)
	{
		if (!bDirty)
			return;

		struct UpdateData
		{
			DecalData Data;
			uint32_t SortPriority;
		};

		std::unordered_map<uint32_t, uint64_t> decalsTransformsMapping; // key - entity ID; value - index into m_Transforms
		std::vector<UpdateData> decalsData;
		std::vector<glm::mat4> decalTransforms;
		decalsData.reserve(decals.size());
		decalTransforms.reserve(decals.size());

		for (auto& decalComp : decals)
		{
			const auto& materialAsset = decalComp->GetMaterialAsset();
			if (!materialAsset)
				continue;

			const uint32_t entityID = decalComp->Parent.GetID();
			decalTransforms.emplace_back() = CalculateDecalVP(decalComp);
			const uint32_t transformIndex = uint32_t(decalTransforms.size() - 1);
			decalsTransformsMapping[entityID] = transformIndex;

			// Decal data
			{
				const auto& material = materialAsset->GetMaterial();
				const auto& albedo = material->GetAlbedoAsset();

				auto& updateData = decalsData.emplace_back();
				updateData.SortPriority = decalComp->GetSortPriority();
				auto& data = updateData.Data;
				data.MaterialIndex = MaterialSystem::GetMaterialIndex(material);
				data.TransformIndex = transformIndex;
				data.AspectRatio = glm::vec2(1.f);
				data.EntityID = entityID;
				if (decalComp->IsAdjustAspectRatioEnabled() && albedo)
				{
					const auto& size = albedo->GetTexture()->GetSize();
					if (size.x > size.y)
					{
						data.AspectRatio.x = 1.0f;
						data.AspectRatio.y = float(size.x) / float(size.y);
					}
					else
					{
						data.AspectRatio.x = float(size.y) / float(size.x);
						data.AspectRatio.y = 1.0f;
					}
				}
			}
		}

		std::sort(decalsData.begin(), decalsData.end(), [](const UpdateData& a, const UpdateData& b)
		{
			return a.SortPriority < b.SortPriority; // We want lower priorities to be first
		});

		RenderManager::Submit([task = shared_from_this(), decals = std::move(decalsData), transforms = std::move(decalTransforms),
			transformsMapping = std::move(decalsTransformsMapping)](Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<RenderDecalsTask>(task);
			thisRef->m_Decals.clear();
			for (const auto& decal : decals)
				thisRef->m_Decals.push_back(decal.Data);
			thisRef->m_Transforms = std::move(transforms);
			thisRef->m_TransformsMapping = std::move(transformsMapping);
			thisRef->bUpload = true;
			thisRef->bUploadTransforms = true;
		});
	}

	void RenderDecalsTask::SetTransforms(const std::unordered_set<const DecalComponent*>& decals)
	{
		if (decals.empty())
			return;

		struct Data
		{
			glm::mat4 VP;
			uint32_t ID;
		};

		std::vector<Data> updateData;
		updateData.reserve(decals.size());

		for (auto& decal : decals)
			updateData.push_back({ CalculateDecalVP(decal), decal->Parent.GetID() });

		RenderManager::Submit([task = shared_from_this(), data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<RenderDecalsTask>(task);
			for (auto& sprite : data)
			{
				auto it = thisRef->m_TransformsMapping.find(sprite.ID);
				if (it != thisRef->m_TransformsMapping.end())
				{
					thisRef->m_Transforms[it->second] = sprite.VP;
					thisRef->bUploadTransforms = true;
				}
			}
		});
	}

	void RenderDecalsTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;
		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.ClearOperation = ClearOperation::Load;
		emissiveAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.bBlendEnabled = true;
		emissiveAttachment.BlendingState.BlendOp = BlendOperation::Add;
		emissiveAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		emissiveAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;
		emissiveAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		emissiveAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		emissiveAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment materialAttachment;
		materialAttachment.ClearOperation = ClearOperation::Load;
		materialAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.bBlendEnabled = true;
		materialAttachment.BlendingState.BlendOp = BlendOperation::Add;
		materialAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		materialAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;
		materialAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		materialAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		materialAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.ClearOperation = ClearOperation::Load;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.Image = gbuffer.ObjectID;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("decals.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("decals.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.CullMode = CullMode::None;
		state.PerInstanceAttribs = PerInstanceAttribs;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
