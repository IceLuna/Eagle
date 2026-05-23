#include "egpch.h"
#include "LightsManagerTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Math/Math.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static constexpr glm::vec3 s_Directions[6] = { glm::vec3(1.0, 0.0, 0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
											       glm::vec3(0.0,-1.0, 0.0), glm::vec3(+0.0, 0.0, 1.0), glm::vec3(0.0, 0.0,-1.0) };

	static constexpr glm::vec3 s_UpVectors[6] = { glm::vec3(0.0, -1.0, +0.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, +0.0, 1.0),
										          glm::vec3(0.0, +0.0, -1.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, -1.0, 0.0) };

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

		BufferSpecifications matricesBufferSpecs;
		matricesBufferSpecs.Size = 100 * sizeof(glm::mat4);
		matricesBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		matricesBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		m_PointLightsBuffer = Buffer::Create(pointLightsBufferSpecs, "Point Lights Buffer");
		m_SpotLightsBuffer = Buffer::Create(spotLightsBufferSpecs, "Spot Lights Buffer");
		m_DirectionalLightBuffer = Buffer::Create(directionalLightBufferSpecs, "Directional Light Buffer");
		m_MatricesBuffer = Buffer::Create(matricesBufferSpecs, "Light Matrices Buffer");
	}

	void LightsManagerTask::SetPointLights(const std::vector<const PointLightComponent*>& pointLights)
	{
		std::vector<PointLight> tempData;
		std::vector<glm::mat4> matrices;
		tempData.reserve(pointLights.size());
		matrices.reserve(pointLights.size() * 6); // For each face

		for (auto& pointLight : pointLights)
		{
			const bool bCastsShadows = pointLight->DoesCastShadows();
			const bool bVolumetric = pointLight->IsVolumetricLight();
			const float radius = pointLight->GetRadius();

			auto& light = tempData.emplace_back();
			light.Position = pointLight->GetWorldTransform().Location;
			light.Radius = radius;
			light.Radius2 = radius * radius;
			light.LightColor = pointLight->GetLightColor() * pointLight->GetIntensity();
			light.VolumetricFogIntensity = glm::max(pointLight->GetVolumetricFogIntensity(), 0.0f);
			light.ViewProjOffset = (uint32_t)matrices.size();
			light.SetCastsShadows(bCastsShadows);

			const glm::mat4 projection = Math::Perspective(glm::radians(90.f), 1.f, EG_POINT_LIGHT_NEAR, radius);

			for (int i = 0; i < 6; ++i)
				matrices.emplace_back() = projection * glm::lookAt(light.Position, light.Position + s_Directions[i], s_UpVectors[i]);

			uint32_t* intensity = (uint32_t*)&light.VolumetricFogIntensity;
			*intensity = (*intensity) | (bVolumetric ? 0x80000000 : 0u);

			EG_CORE_ASSERT(bCastsShadows == light.DoesCastShadows());
		}

		RenderManager::Submit([task = shared_from_this(), pointLights = std::move(tempData), lightMatrices = std::move(matrices)](const Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<LightsManagerTask>(task);
			thisRef->m_PointLights = std::move(pointLights);
			thisRef->m_PointLightMatrices = std::move(lightMatrices);
			thisRef->bPointLightsDirty = true;
		});
	}

	void LightsManagerTask::SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights)
	{
		std::vector<SpotLight> tempData;
		std::vector<glm::mat4> matrices;
		tempData.reserve(spotLights.size());
		matrices.reserve(spotLights.size());

		for (auto& spotLight : spotLights)
		{
			constexpr float nearPlane = EG_POINT_LIGHT_NEAR;
			constexpr float aspectRatio = 1.f;

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
			light.Distance = distance;
			light.bCastsShadows = uint32_t(spotLight->DoesCastShadows());
			light.ViewProjOffset = uint32_t(matrices.size());

			uint32_t* intensity = (uint32_t*)&light.VolumetricFogIntensity;
			*intensity = (*intensity) | (spotLight->IsVolumetricLight() ? 0x80000000 : 0u);

			const float fovY = light.OuterCutOffRadians * 2.f;
			const glm::mat4 view = glm::lookAt(light.Position, light.Position + light.Direction, spotLight->GetUpVector());
			matrices.emplace_back() = Math::Perspective(fovY, aspectRatio, nearPlane, distance) * view;
		}

		RenderManager::Submit([task = shared_from_this(), spotLights = std::move(tempData), lightMatrices = std::move(matrices)](const Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<LightsManagerTask>(task);
			thisRef->m_SpotLights = std::move(spotLights);
			thisRef->m_SpotLightMatrices = std::move(lightMatrices);
			thisRef->bSpotLightsDirty = true;
		});
	}

	void LightsManagerTask::SetDirectionalLight(const DirectionalLightComponent* directionalLightComponent)
	{
		if (directionalLightComponent != nullptr)
		{
			RenderManager::Submit([task = shared_from_this(),
				forward = directionalLightComponent->GetForwardVector(),
				lightColor = directionalLightComponent->GetLightColor() * directionalLightComponent->GetIntensity(),
				ambient = directionalLightComponent->GetAmbientColor(),
				volumetricFogIntensity = directionalLightComponent->GetVolumetricFogIntensity(),
				bVolumetric = directionalLightComponent->IsVolumetricLight(),
			    bCastsShadows = directionalLightComponent->DoesCastShadows()](const Ref<CommandBuffer>& cmd)
			{
				auto thisRef = Cast<LightsManagerTask>(task);

				thisRef->bHasDirectionalLight = true;
				const auto& cascadeProjections = thisRef->m_Renderer.GetCascadeProjections();
				const auto& cascadeFarPlanes = thisRef->m_Renderer.GetCascadeFarPlanes();

				auto& directionalLight = thisRef->m_DirectionalLight;
				directionalLight.Direction = forward;
				directionalLight.LightColor = lightColor;
				directionalLight.Ambient = ambient;
				directionalLight.VolumetricFogIntensity = glm::max(volumetricFogIntensity, 0.f);
				directionalLight.bCastsShadows = uint32_t(bCastsShadows);
				directionalLight.ViewProjOffset = 0u;

				uint32_t* intensity = (uint32_t*)&directionalLight.VolumetricFogIntensity;
				*intensity = (*intensity) | (bVolumetric ? 0x80000000 : 0u);

				for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
					directionalLight.CascadePlaneDistances[i] = cascadeFarPlanes[i];

				const auto& csmSizes = thisRef->m_Renderer.GetOptions_RT().ShadowsSettings.DirLightShadowMapSizes;
				const auto& viewMatrix = thisRef->m_Renderer.GetViewMatrix();
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
					radius = std::ceil(radius);

					glm::vec3 maxExtents = glm::vec3(radius);
					glm::vec3 minExtents = -maxExtents;

					float CascadeFarPlaneOffset = 50.0f, CascadeNearPlaneOffset = -50.0f;

					glm::vec3 lightDir = directionalLight.Direction;
					glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
					glm::mat4 lightOrthoMatrix = Math::Ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f + CascadeNearPlaneOffset, maxExtents.z - minExtents.z + CascadeFarPlaneOffset);

					// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
					glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
					const float ShadowMapResolution = float(csmSizes[index]);
					glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
					glm::vec4 roundedOrigin = glm::round(shadowOrigin);
					glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
					roundOffset = roundOffset * 2.0f / ShadowMapResolution;
					roundOffset.z = 0.0f;
					roundOffset.w = 0.0f;

					lightOrthoMatrix[3] += roundOffset;

					thisRef->m_DirLightMatrices[index] = lightOrthoMatrix * lightViewMatrix;
				}
			});
		}
		else
		{
			RenderManager::Submit([task = shared_from_this()](const Ref<CommandBuffer>& cmd)
			{
				auto thisRef = Cast<LightsManagerTask>(task);
				thisRef->bHasDirectionalLight = false;
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

		// Matrices buffer layout:
		//	 Spot lights go first
		//	 Dir lights go second
		// Note: Point light matrices aren't uploaded since they're not used by the shaders currently
		// If this is ever changed, revisit how ShadowPass task uses/collects them

		m_LightMatrices.clear();

		const uint32_t spotLightsOffset = (uint32_t)m_LightMatrices.size();
		m_LightMatrices.insert(m_LightMatrices.end(), m_SpotLightMatrices.begin(), m_SpotLightMatrices.end());

		if (bHasDirectionalLight)
		{
			m_DirectionalLight.ViewProjOffset = (uint32_t)m_LightMatrices.size();
			for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
			{
				m_LightMatrices.push_back(m_DirLightMatrices[i]);
			}
		}

		if (bPointLightsDirty)
		{
			const size_t pointLightsDataSize = m_PointLights.size() * sizeof(PointLight);
			if (pointLightsDataSize > m_PointLightsBuffer->GetSize())
				m_PointLightsBuffer->Resize((pointLightsDataSize * 3) / 2);

			if (pointLightsDataSize)
			{
				cmd->Write(m_PointLightsBuffer, m_PointLights.data(), pointLightsDataSize, 0, m_PointLightsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
			}
			bPointLightsDirty = false;
		}

		if (bSpotLightsDirty)
		{
			if (spotLightsOffset > 0)
			{
				for (auto& light : m_SpotLights)
				{
					light.ViewProjOffset += spotLightsOffset;
				}
			}

			const size_t spotLightsDataSize = m_SpotLights.size() * sizeof(SpotLight);
			if (spotLightsDataSize > m_SpotLightsBuffer->GetSize())
				m_SpotLightsBuffer->Resize((spotLightsDataSize * 3) / 2);

			if (spotLightsDataSize)
			{
				cmd->Write(m_SpotLightsBuffer, m_SpotLights.data(), spotLightsDataSize, 0, m_SpotLightsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
			}
			bSpotLightsDirty = false;
		}

		if (bHasDirectionalLight)
			cmd->Write(m_DirectionalLightBuffer, &m_DirectionalLight, sizeof(DirectionalLight), 0, m_DirectionalLightBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

		// Upload matrices
		{
			const size_t matricesDataSize = m_LightMatrices.size() * sizeof(glm::mat4);
			if (matricesDataSize > m_MatricesBuffer->GetSize())
				m_MatricesBuffer->Resize((matricesDataSize * 3) / 2);

			if (matricesDataSize)
			{
				cmd->Write(m_MatricesBuffer, m_LightMatrices.data(), matricesDataSize, 0, m_MatricesBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
			}
		}
	}
}
