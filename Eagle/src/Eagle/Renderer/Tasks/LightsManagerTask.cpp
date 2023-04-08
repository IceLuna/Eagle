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
		pointLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications spotLightsBufferSpecs;
		spotLightsBufferSpecs.Size = s_BaseSpotLightsBufferSize;
		spotLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications directionalLightBufferSpecs;
		directionalLightBufferSpecs.Size = sizeof(DirectionalLight);
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
			auto& light = tempData.emplace_back();
			light.Position = pointLight->GetWorldTransform().Location;
			light.LightColor = pointLight->GetLightColor();
			light.Intensity = glm::max(pointLight->GetIntensity(), 0.0f);
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
			light.LightColor = spotLight->GetLightColor();
			light.Direction = spotLight->GetForwardVector();
			light.InnerCutOffRadians = glm::radians(innerAngle);
			light.OuterCutOffRadians = glm::radians(outerAngle);
			light.Intensity = glm::max(spotLight->GetIntensity(), 0.0f);
			light.ViewProj[0] = glm::vec4(spotLight->GetUpVector(), 0.f); // Temporary storing up vector
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
			    lightColor = directionalLightComponent->LightColor,
			    intensity = directionalLightComponent->Intensity](Ref<CommandBuffer>& cmd)
			{
				bHasDirectionalLight = true;
				const auto& cascadeProjections = m_Renderer.GetCascadeProjections();
				const auto& cascadeFarPlanes = m_Renderer.GetCascadeFarPlanes();

				auto& directionalLight = m_DirectionalLight;
				directionalLight.Direction = forward;
				directionalLight.LightColor = lightColor;
				directionalLight.Intensity = glm::max(intensity, 0.f);
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
					// Take the farthest corners, subtract, get length
					const float diameter = glm::length(frustumCorners[0] - frustumCorners[6]);
					const float radius = diameter * 0.5f;
					const float texelsPerUnit = float(s_CSMSizes[index]) / diameter;

					const glm::mat4 scaling = glm::scale(glm::mat4(1.f), glm::vec3(texelsPerUnit));

					glm::mat4 lookAt = defaultLookAt * scaling;
					const glm::mat4 invLookAt = glm::inverse(lookAt);

					// Move our frustum center in texel-sized increments, then get it back into its original space
					frustumCenter = lookAt * glm::vec4(frustumCenter, 1.f);
					frustumCenter.x = glm::floor(frustumCenter.x);
					frustumCenter.y = glm::floor(frustumCenter.y);
					frustumCenter = invLookAt * glm::vec4(frustumCenter, 1.f);

					// Creating our new eye by moving towards the opposite direction of the light by the diameter
					const glm::vec3 frustumCenter3 = glm::vec3(frustumCenter);
					glm::vec3 eye = frustumCenter3 - (directionalLight.Direction * diameter);

					// Final light view matrix
					const glm::mat4 lightView = glm::lookAt(eye, frustumCenter3, upDir);

					// Final light proj matrix that keeps a consistent size. Multiplying by 6 is not perfect.
					// Near and far should be calculating using scene bounds, but for now it'll be like that.
					const glm::mat4 lightProj = glm::ortho(-radius, radius * m_Renderer.GetAspectRatio(), -radius, radius, -radius * 4.f, radius * 4.f);

					directionalLight.ViewProj[index] = lightProj * lightView;
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
