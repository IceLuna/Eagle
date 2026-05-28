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
	static glm::mat4 CalculateDecalInvTransform(const DecalComponent* decalComp)
	{
		Transform transform = decalComp->GetWorldTransform();
		transform.Scale3D *= 0.5f;
		return glm::inverse(Math::ToTransformMatrix(transform)); // Inversing here to avoid doing it in the fragment shader
	}

	static glm::vec2 GetAspectRatio(const Ref<Material>& material)
	{
		glm::vec2 aspectRatio = glm::vec2(1);

		if (const auto& albedo = material->GetAlbedoAsset())
		{
			const auto& size = albedo->GetTexture()->GetSize();
			if (size.x > size.y)
			{
				aspectRatio.x = 1.0f;
				aspectRatio.y = float(size.x) / float(size.y);
			}
			else
			{
				aspectRatio.x = float(size.y) / float(size.x);
				aspectRatio.y = 1.0f;
			}
		}

		return aspectRatio;
	}

	RenderDecalsTask::RenderDecalsTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bGeometricSpecularAA = renderer.GetOptions().bGeometricSpecularAA;
		InitPipeline();

		BufferSpecifications specs{};
		specs.Size = sizeof(glm::mat4) * 100u;
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_TransformsBuffer = Buffer::Create(specs, "Decals_Transforms");

		specs.Size = sizeof(DecalData) * 100u;
		specs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;
		m_InstanceBuffer = Buffer::Create(specs, "Decals_Instances");
	}

	RenderDecalsTask::~RenderDecalsTask()
	{
		for (auto& [_, material] : m_Materials)
		{
			material->RemoveOnModifiedCallback(m_CallbackID);
		}
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
			cmd->Write(m_TransformsBuffer, m_Transforms.data(), m_Transforms.size() * sizeof(glm::mat4), 0, m_TransformsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		}

		if (bUpload)
		{
			m_NoNormalsDecalsCount = 0;
			m_WithNormalsDecalsCount = 0;

			for (auto& decal : m_Decals)
			{
				const auto& material = m_Materials.at(decal.MaterialIndex);
				decal.AspectRatio = GetAspectRatio(material);
				if (material->GetNormalAsset())
					m_WithNormalsDecalsCount++;
				else
					m_NoNormalsDecalsCount++;
			}

			// Decals without normals come first
			std::sort(m_Decals.begin(), m_Decals.end(), [this](const DecalData& a, const DecalData& b)
			{
				const auto& material1 = m_Materials.at(a.MaterialIndex);
				const auto& material2 = m_Materials.at(b.MaterialIndex);

				const bool bHasNormals1 = material1->GetNormalAsset().operator bool();
				const bool bHasNormals2 = material2->GetNormalAsset().operator bool();

				return bHasNormals1 < bHasNormals2;
			});

			const size_t currentInstancesSize = m_Decals.size() * sizeof(DecalData);
			if (currentInstancesSize > m_InstanceBuffer->GetSize())
			{
				size_t newSize = (currentInstancesSize * 3) / 2;
				m_InstanceBuffer->Resize(newSize);
			}
			cmd->Write(m_InstanceBuffer, m_Decals.data(), m_Decals.size() * sizeof(DecalData), 0, m_InstanceBuffer->GetLayout(), BufferReadAccess::Vertex);
		}

		bUpload = false;
		bUploadTransforms = false;
	}

	void RenderDecalsTask::BindDescriptors(const Ref<PipelineGraphics>& pipeline, const GBuffer& gbuffer)
	{
		pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		pipeline->SetBuffer(m_TransformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSampler, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		pipeline->SetImageSampler(gbuffer.Flags, Sampler::PointSampler, EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
	}

	void RenderDecalsTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Decals. Render");
		EG_CPU_TIMING_SCOPED("Decals. Render");
		
		const auto& gbuffer = m_Renderer.GetGBuffer();
		const auto& depth = gbuffer.Depth;
		const auto& flags= gbuffer.Flags;

		const auto depthLayout = depth->GetLayout();
		const auto flagsLayout = flags->GetLayout();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			const auto& images = TextureSystem::GetImages();
			const auto& samplers = TextureSystem::GetSamplers();
			m_Pipeline->SetImageSamplerArray(images, samplers, EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_WithNormalsPipeline->SetImageSamplerArray(images, samplers, EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
		BindDescriptors(m_Pipeline, gbuffer);
		BindDescriptors(m_WithNormalsPipeline, gbuffer);

		const glm::mat4& vp = m_Renderer.GetViewProjection();
		const glm::mat4& invVP = m_Renderer.GetInverseViewProjection();

		cmd->TransitionLayout(depth, depthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(flags, flagsLayout, ImageReadAccess::PixelShaderRead);
		auto& stats = m_Renderer.GetStats();

		// Without normals
		if (const uint32_t instanceCount = m_NoNormalsDecalsCount)
		{
			cmd->BeginGraphics(m_Pipeline);
			cmd->SetGraphicsRootConstants(&vp, &invVP);
			cmd->DrawInstanced(m_InstanceBuffer, 36, instanceCount, 0, 0);
			cmd->EndGraphics();
			++stats.DrawCalls;
		}

		// With normals
		if (const uint32_t instanceCount = m_WithNormalsDecalsCount)
		{
			const uint32_t instanceOffset = m_NoNormalsDecalsCount;
			cmd->BeginGraphics(m_WithNormalsPipeline);
			cmd->SetGraphicsRootConstants(&vp, &invVP);
			cmd->DrawInstanced(m_InstanceBuffer, 36, instanceCount, 0, instanceOffset);
			cmd->EndGraphics();
			++stats.DrawCalls;
		}

		cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, depthLayout);
		cmd->TransitionLayout(flags, ImageReadAccess::PixelShaderRead, flagsLayout);
	}

	void RenderDecalsTask::AddMaterialCallbacks()
	{
		for (auto& [_, material] : m_Materials)
		{
			material->AddOnModifiedCallback(m_CallbackID, [this]()
			{
				bUpload = true;
			});
		}
	}

	void RenderDecalsTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (settings.bGeometricSpecularAA == bGeometricSpecularAA)
			return;

		bGeometricSpecularAA = settings.bGeometricSpecularAA;
		InitPipeline();
	}

	void RenderDecalsTask::SetDecals(const std::vector<const DecalComponent*>& decals)
	{
		struct UpdateData
		{
			DecalData Data;
			uint32_t SortPriority;
		};

		std::unordered_map<uint32_t, uint64_t> decalsTransformsMapping; // key - entity ID; value - index into m_Transforms
		std::vector<UpdateData> decalsData;
		std::vector<glm::mat4> decalTransforms;
		std::map<uint32_t, Ref<Material>> decalMaterials; // Key - Material index
		decalsData.reserve(decals.size());
		decalTransforms.reserve(decals.size());

		for (auto& decalComp : decals)
		{
			const auto& materialAsset = decalComp->GetMaterialAsset();
			if (!materialAsset)
				continue;

			const uint32_t entityID = decalComp->Parent.GetID();
			decalTransforms.emplace_back() = CalculateDecalInvTransform(decalComp);
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
				if (decalComp->IsAdjustAspectRatioEnabled())
				{
					data.AspectRatio = GetAspectRatio(material);
				}
				decalMaterials[data.MaterialIndex] = material;
			}
		}

		std::sort(decalsData.begin(), decalsData.end(), [](const UpdateData& a, const UpdateData& b)
		{
			return a.SortPriority < b.SortPriority; // We want lower priorities to be first
		});

		RenderManager::Submit([task = shared_from_this(), decals = std::move(decalsData), transforms = std::move(decalTransforms),
			transformsMapping = std::move(decalsTransformsMapping), materials = std::move(decalMaterials)](const Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<RenderDecalsTask>(task);
			thisRef->m_Decals.clear();
			for (const auto& decal : decals)
				thisRef->m_Decals.push_back(decal.Data);
			thisRef->m_Transforms = std::move(transforms);
			thisRef->m_TransformsMapping = std::move(transformsMapping);
			thisRef->bUpload = true;
			thisRef->bUploadTransforms = true;

			for (auto& [_, material] : thisRef->m_Materials)
			{
				material->RemoveOnModifiedCallback(thisRef->m_CallbackID);
			}
			thisRef->m_Materials = std::move(materials);
			thisRef->AddMaterialCallbacks();
		});
	}

	void RenderDecalsTask::SetTransforms(const std::vector<const DecalComponent*>& decals)
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
			updateData.push_back({ CalculateDecalInvTransform(decal), decal->Parent.GetID() });

		RenderManager::Submit([task = shared_from_this(), data = std::move(updateData)](const Ref<CommandBuffer>&)
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
		ShaderDefines defines;
		if (bGeometricSpecularAA)
		{
			defines["EG_GEOMETRIC_SPECULAR_AA"] = "";
		}

		const auto& gbuffer = m_Renderer.GetGBuffer();

		BlendState alphaBlendingState{};
		alphaBlendingState.BlendOp = BlendOperation::Add;
		alphaBlendingState.BlendSrc = BlendFactor::SrcAlpha;
		alphaBlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;
		alphaBlendingState.BlendOpAlpha = BlendOperation::Add;
		alphaBlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		alphaBlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState = alphaBlendingState;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.ClearOperation = ClearOperation::Load;
		emissiveAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		emissiveAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.bBlendEnabled = true;
		emissiveAttachment.BlendingState = alphaBlendingState;

		ColorAttachment materialAttachment;
		materialAttachment.ClearOperation = ClearOperation::Load;
		materialAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		materialAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.bBlendEnabled = true;
		materialAttachment.BlendingState = alphaBlendingState;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.ClearOperation = ClearOperation::Load;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.Image = gbuffer.ObjectID;

		ColorAttachment normalsAttachment;
		normalsAttachment.ClearOperation = ClearOperation::Load;
		normalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.Image = gbuffer.Normals;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("decals.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("decals.frag", ShaderType::Fragment, defines);
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

		defines["DECAL_NORMALS"] = "";

		state.FragmentShader = Shader::Create("decals.frag", ShaderType::Fragment, defines);
		state.ColorAttachments.push_back(normalsAttachment);
		if (m_WithNormalsPipeline)
			m_WithNormalsPipeline->SetState(state);
		else
			m_WithNormalsPipeline = PipelineGraphics::Create(state);
	}
}
