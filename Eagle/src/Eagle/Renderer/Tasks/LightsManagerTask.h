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
	private:
		struct PointLight
		{
			glm::mat4 ViewProj[6];

			glm::vec3 Position;
			uint32_t unused1_;

			glm::vec3 LightColor;
			float Intensity;
		};

		struct DirectionalLight
		{
			glm::mat4 ViewProj[EG_CASCADES_COUNT];
			float CascadePlaneDistances[EG_CASCADES_COUNT];

			glm::vec3 Direction;
			float Intensity;

			glm::vec3 LightColor;
			uint32_t unused3;

			glm::vec3 Specular;
			uint32_t unused4;
		};

		struct SpotLight
		{
			glm::mat4 ViewProj;

			glm::vec3 Position;
			float InnerCutOffRadians;

			glm::vec3 Direction;
			float OuterCutOffRadians;

			glm::vec3 LightColor;
			float Intensity;
		};

	public:
		LightsManagerTask(SceneRenderer& renderer);

		void SetPointLights(const std::vector<const PointLightComponent*>& pointLights, bool bDirty);
		void SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights, bool bDirty);
		void SetDirectionalLight(const DirectionalLightComponent* directionalLightComponent);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override {}

		const std::vector<PointLight>& GetPointLights() const { return m_PointLights; }
		const std::vector<SpotLight>& GetSpotLights() const { return m_SpotLights; }
		const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }
		bool HasDirectionalLight() const { return bHasDirectionalLight; }

		const Ref<Buffer>& GetPointLightsBuffer() const { return m_PointLightsBuffer; }
		const Ref<Buffer>& GetSpotLightsBuffer() const { return m_SpotLightsBuffer; }
		const Ref<Buffer>& GetDirectionalLightBuffer() const { return m_DirectionalLightBuffer; }

	private:
		void UploadLightBuffers(const Ref<CommandBuffer>& cmd);

	private:
		std::vector<PointLight> m_PointLights;
		std::vector<SpotLight> m_SpotLights;
		DirectionalLight m_DirectionalLight{};

		Ref<Buffer> m_PointLightsBuffer;
		Ref<Buffer> m_SpotLightsBuffer;
		Ref<Buffer> m_DirectionalLightBuffer;

		bool bPointLightsDirty = true;
		bool bSpotLightsDirty = true;
		bool bHasDirectionalLight = false;

		static constexpr size_t s_BaseLightsCount = 10;
		static constexpr size_t s_BasePointLightsBufferSize = s_BaseLightsCount * sizeof(PointLight);
		static constexpr size_t s_BaseSpotLightsBufferSize = s_BaseLightsCount * sizeof(SpotLight);
	};
}
