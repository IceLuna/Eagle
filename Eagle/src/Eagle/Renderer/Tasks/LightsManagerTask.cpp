#include "egpch.h"
#include "LightsManagerTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static constexpr glm::vec3 s_Directions[6] = { glm::vec3(1.0, 0.0, 0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
											       glm::vec3(0.0,-1.0, 0.0), glm::vec3(+0.0, 0.0, 1.0), glm::vec3(0.0, 0.0,-1.0) };

	static constexpr glm::vec3 s_UpVectors[6] = { glm::vec3(0.0, -1.0, +0.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, +0.0, 1.0),
										          glm::vec3(0.0, +0.0, -1.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, -1.0, 0.0) };

	static const glm::mat4 s_PointLightPerspectiveProjection = glm::perspective(glm::radians(90.f), 1.f, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR);

	// TODO: Expose to editor
	static constexpr uint32_t s_CSMSizes[EG_CASCADES_COUNT] =
	{
		RendererConfig::DirLightShadowMapSize * 2,
		RendererConfig::DirLightShadowMapSize,
		RendererConfig::DirLightShadowMapSize,
		RendererConfig::DirLightShadowMapSize
	};

	LightsManagerTask::LightsManagerTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		BufferSpecifications pointLightsBufferSpecs;
		pointLightsBufferSpecs.Size = s_BasePointLightsBufferSize;
		pointLightsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		pointLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications spotLightsBufferSpecs;
		spotLightsBufferSpecs.Size = s_BaseSpotLightsBufferSize;
		spotLightsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		spotLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications directionalLightBufferSpecs;
		directionalLightBufferSpecs.Size = sizeof(DirectionalLight);
		directionalLightBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		directionalLightBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		m_PointLightsBuffer = Buffer::Create(pointLightsBufferSpecs, "PointLightsBuffer");
		m_SpotLightsBuffer = Buffer::Create(spotLightsBufferSpecs, "SpotLightsBuffer");
		m_DirectionalLightBuffer = Buffer::Create(directionalLightBufferSpecs, "DirectionalLightBuffer");
	}

	void LightsManagerTask::SetPointLights(const std::vector<const PointLightComponent*>& pointLights, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<PointLight> tempData;
		tempData.reserve(pointLights.size());
		for (auto& pointLight : pointLights)
		{
			const bool bCastsShadows = pointLight->DoesCastShadows();
			const bool bVolumetric = pointLight->IsVolumetricLight();

			auto& light = tempData.emplace_back();
			light.Position = pointLight->GetWorldTransform().Location;
			const float radius = pointLight->GetRadius();
			light.Radius2 = radius * radius;
			light.LightColor = pointLight->GetLightColor() * pointLight->GetIntensity();
			light.VolumetricFogIntensity = glm::max(pointLight->GetVolumetricFogIntensity(), 0.0f);

			uint32_t* intensity = (uint32_t*)&light.VolumetricFogIntensity;
			*intensity = (*intensity) | (bVolumetric ? 0x80000000 : 0u);

			uint32_t* radius2 = (uint32_t*)&light.Radius2;
			*radius2 = (*radius2) | (bCastsShadows ? 0x80000000 : 0u);
		}

		RenderManager::Submit([this, pointLights = std::move(tempData)](Ref<CommandBuffer>& cmd) mutable
		{
			m_PointLights = std::move(pointLights);

			for (auto& light : m_PointLights)
			{
				for (int i = 0; i < 6; ++i)
					light.ViewProj[i] = s_PointLightPerspectiveProjection * glm::lookAt(light.Position, light.Position + s_Directions[i], s_UpVectors[i]);
			}
			bPointLightsDirty = true;
		});
	}

	void LightsManagerTask::SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<SpotLight> tempData;
		tempData.reserve(spotLights.size());
		for (auto& spotLight : spotLights)
		{
			auto& light = tempData.emplace_back();

			const float innerAngle = glm::clamp(spotLight->GetInnerCutOffAngle(), 1.f, 80.f);
			const float outerAngle = glm::clamp(spotLight->GetOuterCutOffAngle(), 1.f, 80.f);

			light.Position = spotLight->GetWorldTransform().Location;
			light.LightColor = spotLight->GetLightColor() * spotLight->GetIntensity();
			light.Direction = spotLight->GetForwardVector();
			light.InnerCutOffRadians = glm::radians(innerAngle);
			light.OuterCutOffRadians = glm::radians(outerAngle);
			light.VolumetricFogIntensity = glm::max(spotLight->GetVolumetricFogIntensity(), 0.0f);
			const float distance = spotLight->GetDistance();
			light.Distance2  = distance * distance;
			light.ViewProj[0] = glm::vec4(spotLight->GetUpVector(), 0.f); // Temporary storing up vector
			light.bCastsShadows = uint32_t(spotLight->DoesCastShadows());
			light.bVolumetricLight = uint32_t(spotLight->IsVolumetricLight());
		}

		RenderManager::Submit([this, spotLights = std::move(tempData)](Ref<CommandBuffer>& cmd) mutable
		{
			m_SpotLights = std::move(spotLights);

			for (auto& light : m_SpotLights)
			{
				const float cutoff = light.OuterCutOffRadians * 2.f;
				glm::mat4 spotLightPerspectiveProjection = glm::perspective(cutoff, 1.f, 0.01f, 50.f);
				spotLightPerspectiveProjection[1][1] *= -1.f;
				const glm::vec3 upVector = light.ViewProj[0];
				light.ViewProj = spotLightPerspectiveProjection * glm::lookAt(light.Position, light.Position + light.Direction, upVector);
			}
			bSpotLightsDirty = true;
		});
	}

	void LightsManagerTask::SetDirectionalLight(const DirectionalLightComponent* directionalLightComponent)
	{
		if (directionalLightComponent != nullptr)
		{
			RenderManager::Submit([this,
				forward = directionalLightComponent->GetForwardVector(),
				lightColor = directionalLightComponent->GetLightColor() * directionalLightComponent->GetIntensity(),
				ambient = directionalLightComponent->Ambient,
				volumetricFogIntensity = directionalLightComponent->GetVolumetricFogIntensity(),
				bVolumetric = directionalLightComponent->IsVolumetricLight(),
			    bCastsShadows = directionalLightComponent->DoesCastShadows()](Ref<CommandBuffer>& cmd)
			{
				bHasDirectionalLight = true;
				const auto& cascadeProjections = m_Renderer.GetCascadeProjections();
				const auto& cascadeFarPlanes = m_Renderer.GetCascadeFarPlanes();

				auto& directionalLight = m_DirectionalLight;
				directionalLight.Direction = forward;
				directionalLight.LightColor = lightColor;
				directionalLight.Ambient = ambient;
				directionalLight.VolumetricFogIntensity = glm::max(volumetricFogIntensity, 0.f);
				directionalLight.bCastsShadows = uint32_t(bCastsShadows);
				directionalLight.bVolumetricLight = uint32_t(bVolumetric);

				for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
					directionalLight.CascadePlaneDistances[i] = cascadeFarPlanes[i];

				// https://alextardif.com/shadowmapping.html is used as a ref for CSM
				// Creating base lookAt using light dir
				constexpr glm::vec3 upDir = glm::vec3(0.f, 1.f, 0.f);
				const glm::vec3 baseLookAt = -directionalLight.Direction;
				const glm::mat4 defaultLookAt = glm::lookAt(glm::vec3(0), baseLookAt, upDir);

				const auto& viewMatrix = m_Renderer.GetViewMatrix();
				for (uint32_t index = 0; index < EG_CASCADES_COUNT; ++index)
				{
					const glm::mat4& cascadeProj = cascadeProjections[index];
					const std::array frustumCorners = GetFrustumCornersWorldSpace(viewMatrix, cascadeProj);
					glm::vec3 frustumCenter = GetFrustumCenter(frustumCorners);

					float radius = 0.0f;
					for (uint32_t i = 0; i < 8; i++)
					{
						float distance = glm::length(frustumCorners[i] - frustumCenter);
						radius = glm::max(radius, distance);
					}
					radius = std::ceil(radius * 16.0f) / 16.0f;

					glm::vec3 maxExtents = glm::vec3(radius);
					glm::vec3 minExtents = -maxExtents;

					float CascadeFarPlaneOffset = 50.0f, CascadeNearPlaneOffset = -50.0f;

					glm::vec3 lightDir = directionalLight.Direction;
					glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));
					glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f + CascadeNearPlaneOffset, maxExtents.z - minExtents.z + CascadeFarPlaneOffset);

					// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
					glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
					const float ShadowMapResolution = float(s_CSMSizes[index]);
					glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
					glm::vec4 roundedOrigin = glm::round(shadowOrigin);
					glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
					roundOffset = roundOffset * 2.0f / ShadowMapResolution;
					roundOffset.z = 0.0f;
					roundOffset.w = 0.0f;

					lightOrthoMatrix[3] += roundOffset;

					directionalLight.ViewProj[index] = lightOrthoMatrix * lightViewMatrix;
				}
			});
		}
		else
		{
			RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
			{
				bHasDirectionalLight = false;
			});
		}
	}

	void LightsManagerTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		UploadLightBuffers(cmd);
	}

	void LightsManagerTask::UploadLightBuffers(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Upload Light Buffers");
		EG_GPU_TIMING_SCOPED(cmd, "Upload Light Buffers");

		if (bPointLightsDirty)
		{
			const size_t pointLightsDataSize = m_PointLights.size() * sizeof(PointLight);
			if (pointLightsDataSize > m_PointLightsBuffer->GetSize())
				m_PointLightsBuffer->Resize((pointLightsDataSize * 3) / 2);

			if (pointLightsDataSize)
			{
				cmd->Write(m_PointLightsBuffer, m_PointLights.data(), pointLightsDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
				cmd->StorageBufferBarrier(m_PointLightsBuffer);
			}
			bPointLightsDirty = false;
		}

		if (bSpotLightsDirty)
		{
			const size_t spotLightsDataSize = m_SpotLights.size() * sizeof(SpotLight);
			if (spotLightsDataSize > m_SpotLightsBuffer->GetSize())
				m_SpotLightsBuffer->Resize((spotLightsDataSize * 3) / 2);

			if (spotLightsDataSize)
			{
				cmd->Write(m_SpotLightsBuffer, m_SpotLights.data(), spotLightsDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
				cmd->StorageBufferBarrier(m_SpotLightsBuffer);
			}
			bSpotLightsDirty = false;
		}

		cmd->Write(m_DirectionalLightBuffer, &m_DirectionalLight, sizeof(DirectionalLight), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(m_DirectionalLightBuffer);
	}
}
