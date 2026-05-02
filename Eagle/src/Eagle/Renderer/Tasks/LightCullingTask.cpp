#include "egpch.h"
#include "LightCullingTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"
#include "../../Eagle-Editor/assets/shaders/common_structures.h"
#include "../../Eagle-Editor/assets/shaders/light_culling/utils.h"

namespace Eagle
{
	LightCullingTask::LightCullingTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		constexpr size_t reserveCount = 10;

		BufferSpecifications specs{};
		specs.Layout = BufferLayoutType::StorageBuffer;
		specs.Size = reserveCount * sizeof(LightsManagerTask::PointLight);
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_CulledPointLightsBuffer = Buffer::Create(specs, "LightCulling_PointLights_CameraView");

		specs.Size = reserveCount * sizeof(LightsManagerTask::SpotLight);
		m_CulledSpotLightsBuffer = Buffer::Create(specs, "LightCulling_SpotLights_CameraView");

		specs.Layout = BufferReadAccess::Uniform;
		specs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
		specs.Size = 2 * sizeof(uint32_t);
		m_CountersBuffer = Buffer::Create(specs, "LightCulling_Counters");

		InitPipelines();
		OnResize(m_Renderer.GetViewportSize());
	}

	void LightCullingTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Light Culling");
		EG_GPU_TIMING_SCOPED(cmd, "Light Culling");

		const auto& origPLBuffer = m_Renderer.GetPointLightsBuffer();
		const auto& origSLBuffer = m_Renderer.GetSpotLightsBuffer();

		if (const size_t reqSize = origPLBuffer->GetSize(); reqSize > m_CulledPointLightsBuffer->GetSize())
		{
			m_CulledPointLightsBuffer->Resize(reqSize);
		}
		if (const size_t reqSize = origSLBuffer->GetSize(); reqSize > m_CulledSpotLightsBuffer->GetSize())
		{
			m_CulledSpotLightsBuffer->Resize(reqSize);
		}

		CullForCameraView(cmd);
		Cull(cmd);
	}
	
	// Culling is perfomed on the CPU to be able to avoid rendering shadow maps for culled lights.
	// There's a shader that does it on the GPU (cull_for_camera_view.comp), but then how do we NOT render
	// shadow maps for culled lights?
	void LightCullingTask::CullForCameraView(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Light Culling. Culling for camera frustum");

		const glm::vec2 size = m_Renderer.GetViewportSize();
		const glm::vec2 rcpScreenSize = vec2(1) / size;
		const float maxShadowDistance2 = m_Renderer.GetShadowMaxDistance() * m_Renderer.GetShadowMaxDistance();

		const float fMinDepth = 1.f;
		const float fMaxDepth = 0.f;

		const auto& frustumData = m_Renderer.GetCullingFrustumData();
		const glm::mat4& invProj = frustumData.InvProj;
		const glm::mat4& view = frustumData.View;
		const glm::vec3& cameraPos = frustumData.Position;

		::Frustum frustum;
		::AABB aabb;
		CalculateFrustumAndAABB(invProj, rcpScreenSize, vec2(0), size, fMinDepth, fMaxDepth, frustum, aabb);

		const float maxDepthVS = ScreenToView(invProj, vec4(0, 0, fMaxDepth, 1), rcpScreenSize).z;
		const float nearClipVS = ScreenToView(invProj, vec4(0, 0, 1, 1), rcpScreenSize).z;

		m_CulledPointLights.clear();
		m_CulledSpotLights.clear();

		// Point lights
		{
			uint32_t shadowMapIndex = 0;
			const auto& lights = m_Renderer.GetPointLights();
			for (const auto& light : lights)
			{
				const ::Sphere sphere = SphereFromPointLight((::PointLight&)light, view);
				if (SphereInsideFrustum(sphere, frustum, nearClipVS, maxDepthVS))
				{
					if (SphereIntersectsAABB(sphere, aabb))
					{
						auto& emplaced = m_CulledPointLights.emplace_back(light);
						if (emplaced.DoesCastShadows())
						{
							const float distance2 = glm::distance2(cameraPos, light.Position);
							const bool bInShadowRange = distance2 <= maxShadowDistance2;
							emplaced.SetCastsShadows(bInShadowRange);
							if (bInShadowRange)
							{
								emplaced.ShadowMapIndex = shadowMapIndex++;
							}
						}
					}
				}
			}
		}

		// Spot lights
		{
			uint32_t shadowMapIndex = 0;
			const auto& lights = m_Renderer.GetSpotLights();
			for (const auto& light : lights)
			{
				const ::Cone cone = ConeFromSpotLight((::SpotLight&)light, view);
				if (ConeInsideFrustum(cone, frustum, nearClipVS, maxDepthVS))
				{
					auto& emplaced = m_CulledSpotLights.emplace_back(light);
					if (emplaced.bCastsShadows != 0)
					{
						const float distance2 = glm::distance2(cameraPos, light.Position);
						const bool bInShadowRange = distance2 <= maxShadowDistance2;
						emplaced.bCastsShadows = bInShadowRange ? 1u : 0u;
						if (bInShadowRange)
						{
							emplaced.ShadowMapIndex = shadowMapIndex++;
						}
					}
				}
			}
		}

		if (!m_CulledPointLights.empty())
			cmd->Write(m_CulledPointLightsBuffer, m_CulledPointLights.data(), sizeof(PointLight) * m_CulledPointLights.size(), 0, m_CulledPointLightsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		if (!m_CulledSpotLights.empty())
			cmd->Write(m_CulledSpotLightsBuffer, m_CulledSpotLights.data(), sizeof(SpotLight) * m_CulledSpotLights.size(), 0, m_CulledSpotLightsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

		const uint32_t counters[2] = {
			(uint32_t)m_CulledPointLights.size(),
			(uint32_t)m_CulledSpotLights.size()
		};
		cmd->Write(m_CountersBuffer, counters, sizeof(uint32_t) * 2, 0, m_CountersBuffer->GetLayout(), BufferReadAccess::Uniform);
	}

	void LightCullingTask::Cull(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Light Culling. Tile Culling");
		EG_GPU_TIMING_SCOPED(cmd, "Light Culling. Tile Culling");

		struct PushData
		{
			glm::uvec2 Size;
			uint32_t TilesBufferWidth;
		} pushData;
		pushData.Size = m_Renderer.GetViewportSize();
		pushData.TilesBufferWidth = m_TilesBufferWidth;

		const auto& depth = m_Renderer.GetGBuffer().Depth;
		const auto& output = m_Renderer.GetHDROutput();

		const ImageLayout outputLayout = output->GetLayout();
		const ImageLayout depthLayout = depth->GetLayout();
		cmd->TransitionLayout(depth, depthLayout, ImageReadAccess::NonPixelShaderRead);

		if (bVisualizeTiles)
		{
			cmd->TransitionLayout(output, outputLayout, ImageLayoutType::StorageImage);
			m_TileCulling->SetImage(output, 0, 5);
		}

		m_TileCulling->SetBuffer(m_CulledPointLightsBuffer, 0, 0);
		m_TileCulling->SetBuffer(m_CulledSpotLightsBuffer, 0, 1);
		m_TileCulling->SetBuffer(m_CountersBuffer, 0, 2);
		m_TileCulling->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 3);
		m_TileCulling->SetImageSampler(depth, Sampler::PointSampler, 0, 4);

		m_TileCulling->SetBuffer(m_Tiles_Opaque_PL, 1, 0);
		m_TileCulling->SetBuffer(m_Tiles_Opaque_SL, 1, 1);
		m_TileCulling->SetBuffer(m_Tiles_Translucent_PL, 1, 2);
		m_TileCulling->SetBuffer(m_Tiles_Translucent_SL, 1, 3);

		constexpr uint32_t tileSize = EG_LIGHT_CULLING_TILE_SIZE;
		const glm::uvec3 groupSize = glm::uvec3(tileSize, tileSize, 1);
		const glm::uvec2 numGroups = CalcNumGroups(pushData.Size, groupSize);
		cmd->Dispatch(m_TileCulling, numGroups, &pushData);

		cmd->Barrier(m_Tiles_Opaque_PL);
		cmd->Barrier(m_Tiles_Opaque_SL);
		cmd->Barrier(m_Tiles_Translucent_PL);
		cmd->Barrier(m_Tiles_Translucent_SL);
		cmd->TransitionLayout(depth, ImageReadAccess::NonPixelShaderRead, depthLayout);
		if (bVisualizeTiles)
		{
			cmd->TransitionLayout(output, ImageLayoutType::StorageImage, outputLayout);
		}
	}

	void LightCullingTask::OnResize(const glm::uvec2 size)
	{
		const glm::uvec2 numTiles = CalcNumGroups(size, EG_LIGHT_CULLING_TILE_SIZE);
		m_TilesBufferWidth = numTiles.x;

		const size_t totalSize = numTiles.x * numTiles.y * EG_LIGHTS_BUCKET_COUNT * sizeof(uint32_t);

		if (!m_Tiles_Opaque_PL)
		{
			BufferSpecifications specs{};
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Usage = BufferUsage::StorageBuffer;
			specs.Size = totalSize;

			m_Tiles_Opaque_PL = Buffer::Create(specs, "LightTiles_Opaque_PL");
			m_Tiles_Opaque_SL = Buffer::Create(specs, "LightTiles_Opaque_SL");
			m_Tiles_Translucent_PL = Buffer::Create(specs, "LightTiles_Translucent_PL");
			m_Tiles_Translucent_SL = Buffer::Create(specs, "LightTiles_Translucent_SL");
			return;
		}

		m_Tiles_Opaque_PL->Resize(totalSize);
		m_Tiles_Opaque_SL->Resize(totalSize);
		m_Tiles_Translucent_PL->Resize(totalSize);
		m_Tiles_Translucent_SL->Resize(totalSize);
	}

	void LightCullingTask::InitPipelines()
	{
		ShaderDefines defines{};
		if (bVisualizeTiles)
		{
			defines["EG_DEBUG_LIGHT_CULLING"] = {};
		}

		PipelineComputeState state{};
		state.ComputeShader = Shader::Create("light_culling/culling.comp", ShaderType::Compute, defines);
		m_TileCulling = PipelineCompute::Create(state);
	}
}
