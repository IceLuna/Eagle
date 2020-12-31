#include "egpch.h"
#include "OrthographicCamera.h"

namespace Eagle
{
	OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
		:	m_Projection(glm::ortho(left, right, bottom, top, -1.f, 1.f))
		,	m_Position({0.f, 0.f, 0.f})
		,	m_ZRotation(0.f)
	{
		RecalculateViewProjection();
	}

	const glm::mat4& OrthographicCamera::GetViewProjectionMatrix() const
	{
		return m_ViewProjection;
	}

	const glm::mat4& OrthographicCamera::GetViewMatrix() const
	{
		return m_View;
	}

	const glm::mat4& OrthographicCamera::GetProjectionMatrix() const
	{
		return m_Projection;
	}

	void OrthographicCamera::SetPosition(const glm::vec3& position)
	{
		m_Position = position;
		RecalculateViewProjection();
	}

	const glm::vec3& OrthographicCamera::GetPosition() const
	{
		return m_Position;
	}

	void OrthographicCamera::SetRotation(const glm::vec3& rotation)
	{
		m_ZRotation.z = rotation.z;
		RecalculateViewProjection();
	}

	const glm::vec3& OrthographicCamera::GetRotation() const
	{
		return m_ZRotation;
	}

	void OrthographicCamera::RecalculateViewProjection()
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.f), m_Position) * 
			glm::rotate(glm::mat4(1.f), glm::radians(m_ZRotation.z), glm::vec3(0, 0, 1));
		
		m_View = glm::inverse(transform);
		m_ViewProjection = m_Projection * m_View;
	}
}