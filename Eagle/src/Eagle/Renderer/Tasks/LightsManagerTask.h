#pragma once

#include "RendererTask.h"
#include "../../Eagle-Editor/assets/shaders/defines.h"

namespace Eagle
{
	class PointLightComponent;
	class SpotLightComponent;
	class DirectionalLightComponent;
	class Buffer;
	
	class LightsManagerTask : public RendererTask
	{
	public:
		struct PointLight
		{
			glm::vec3 Position;
			float Radius2; // Sign bit is used as a flag for `bCastsShadows`

			glm::vec3 LightColor;
			float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`

			uint32_t ShadowMapIndex = EG_INVALID_SHADOW_MAP;
			uint32_t ViewProjOffset; // Note: it's invalid to use it on shader side because point light transforms aren't uploaded. Currently, used to fetch it on the CPU side
			uint32_t Padding0;
			uint32_t Padding1;

			bool DoesCastShadows() const { return (*((uint32_t*)(&Radius2)) & 0x80000000) != 0; }
		};

		struct DirectionalLight
		{
			float CascadePlaneDistances[EG_CASCADES_COUNT];

			glm::vec3 Direction;
			uint32_t ViewProjOffset; // Offset into the transforms buffer

			glm::vec3 LightColor;
			uint32_t bCastsShadows;

			glm::vec3 Ambient;
			float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`
		};

		struct SpotLight
		{
			glm::vec3 Position;
			float InnerCutOffRadians;

			glm::vec3 Direction;
			float OuterCutOffRadians;

			glm::vec3 LightColor;
			uint32_t ViewProjOffset; // Offset into the transforms buffer

			float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`
			float Distance2;
			uint32_t bCastsShadows;
			uint32_t ShadowMapIndex = EG_INVALID_SHADOW_MAP;
		};

	public:
		LightsManagerTask(SceneRenderer& renderer);

		void SetPointLights(const std::vector<const PointLightComponent*>& pointLights, bool bDirty);
		void SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights, bool bDirty);
		void SetDirectionalLight(const DirectionalLightComponent* directionalLightComponent);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override {}

		const std::vector<PointLight>& GetPointLights() const { return m_PointLights; }
		const std::vector<SpotLight>& GetSpotLights() const { return m_SpotLights; }
		const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }
		const std::vector<glm::mat4>& GetLightMatrices() const { return m_LightMatrices; }
		const std::vector<glm::mat4>& GetPointLightMatrices() const { return m_PointLightMatrices; }
		bool HasDirectionalLight() const { return bHasDirectionalLight; }

		const Ref<Buffer>& GetPointLightsBuffer() const { return m_PointLightsBuffer; }
		const Ref<Buffer>& GetSpotLightsBuffer() const { return m_SpotLightsBuffer; }
		const Ref<Buffer>& GetDirectionalLightBuffer() const { return m_DirectionalLightBuffer; }
		const Ref<Buffer>& GetLightMatricesBuffer() const { return m_MatricesBuffer; }

	private:
		void UploadLightBuffers(const Ref<CommandBuffer>& cmd);

	private:
		std::vector<PointLight> m_PointLights;
		std::vector<SpotLight> m_SpotLights;
		DirectionalLight m_DirectionalLight{};

		std::vector<glm::mat4> m_LightMatrices;
		std::vector<glm::mat4> m_PointLightMatrices;
		std::vector<glm::mat4> m_SpotLightMatrices;
		std::array<glm::mat4, EG_CASCADES_COUNT> m_DirLightMatrices;

		Ref<Buffer> m_PointLightsBuffer;
		Ref<Buffer> m_SpotLightsBuffer;
		Ref<Buffer> m_DirectionalLightBuffer;
		Ref<Buffer> m_MatricesBuffer;

		bool bPointLightsDirty = true;
		bool bSpotLightsDirty = true;
		bool bHasDirectionalLight = false;

		static constexpr size_t s_BaseLightsCount = 10;
		static constexpr size_t s_BasePointLightsBufferSize = s_BaseLightsCount * sizeof(PointLight);
		static constexpr size_t s_BaseSpotLightsBufferSize = s_BaseLightsCount * sizeof(SpotLight);
	};
}
