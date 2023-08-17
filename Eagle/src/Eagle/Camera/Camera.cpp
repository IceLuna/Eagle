#include "egpch.h"
#include "Camera.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

namespace Eagle
{
	void Camera::SetOrthographic(float size, float nearClip, float farClip)
	{
		m_ProjectionMode = CameraProjectionMode::Orthographic;
		m_OrthographicSize = size;
		m_OrthographicNear = nearClip;
		m_OrthographicFar = farClip;

		RecalculateProjection();
	}

	void Camera::SetPerspective(float verticalFOV, float nearClip, float farClip)
	{
		m_ProjectionMode = CameraProjectionMode::Perspective;
		m_PerspectiveVerticalFOV = verticalFOV;
		m_PerspectiveNear = nearClip;
		m_PerspectiveFar = farClip;

		RecalculateProjection();
	}

	void Camera::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_AspectRatio = (float)width / (float)height;

		RecalculateProjection();
	}

	void Camera::RecalculateProjection()
	{
		if (m_ProjectionMode == CameraProjectionMode::Perspective)
		{
			m_Projection = glm::perspective(m_PerspectiveVerticalFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);

			float cascadeSplits[EG_CASCADES_COUNT];

			const float clipRange = m_ShadowFar - m_PerspectiveNear;
			const float minZ = m_PerspectiveNear;
			const float maxZ = m_PerspectiveNear + clipRange;

			const float range = maxZ - minZ;
			const float ratio = maxZ / minZ;

			// Calculate split depths based on view camera frustum
			// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
			for (uint32_t i = 0; i < EG_CASCADES_COUNT; i++)
			{
				float p = (i + 1) / static_cast<float>(EG_CASCADES_COUNT);
				float log = minZ * std::pow(ratio, p);
				float uniform = minZ + range * p;
				float d = m_CascadeSplitLambda * (log - uniform) + uniform;
				cascadeSplits[i] = (d - m_PerspectiveNear) / clipRange;
			}

			// Calculating cascade projections
			{
				for (uint32_t i = 0; i < EG_CASCADES_COUNT; i++)
					m_CascadeFarPlanes[i] = m_ShadowFar * cascadeSplits[i];

				m_CascadeProjections[0] = glm::perspective(m_PerspectiveVerticalFOV, m_AspectRatio, m_PerspectiveNear, m_CascadeFarPlanes[0]);
				for (int i = 1; i < EG_CASCADES_COUNT; ++i)
				{
					const float farPlane = m_CascadeFarPlanes[i - 1];
					// Adding a little overlap between cascades to blend between them
					constexpr float overlap = float(EG_CSM_OVERLAP_PERCENT) / 100.f;
					m_CascadeProjections[i] = glm::perspective(m_PerspectiveVerticalFOV, m_AspectRatio, farPlane - farPlane * overlap, m_CascadeFarPlanes[i]);
				}
			}
		}
		else
		{
			const float orthoLeft = -m_OrthographicSize * m_AspectRatio * 0.5f;
			const float orthoRight = m_OrthographicSize * m_AspectRatio * 0.5f;
			const float orthoBottom = -m_OrthographicSize * 0.5f;
			const float orthoTop = m_OrthographicSize * 0.5f;

			m_Projection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);

			// Calculating cascade projections
			{
				const float orthoSize = 2.f;
				const float orthoLeft = -orthoSize * m_AspectRatio * 0.5f;
				const float orthoRight = orthoSize * m_AspectRatio * 0.5f;
				const float orthoBottom = -orthoSize * 0.5f;
				const float orthoTop = orthoSize * 0.5f;

				const glm::mat4 projection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);
				for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
					m_CascadeProjections[i] = projection; // TODO: Different projections?
			}
		}

		// Flipping for Vulkan
		m_Projection[1][1] *= -1.f;
		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
			m_CascadeProjections[i][1][1] *= -1.f;
	}
}

