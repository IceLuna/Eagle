#include "Circle.h"

#include <Eagle/Sound.h>

using namespace Eagle;

Ref<Texture2D> Circle::s_CircleTexture;
Ref<Texture2D> Circle::s_OuterCircleTexture;
Ref<Texture2D> Circle::s_ErrorTexture;
Ref<Texture2D> Circle::s_Number1Texture;

glm::vec2 Circle::m_CircleSize = { 0.35f, 0.35f };
glm::vec2 Circle::m_ErrorSize = { 0.25f, 0.25f };
glm::vec2 Circle::m_NumberSize = { 0.15f, 0.15f };

Circle::Circle(const glm::vec2& position, float lifetimeMs, float appearenceDelay, float correctProcentage)
: m_Position(position), m_Lifetime(lifetimeMs), m_AppearenceDelay(appearenceDelay), m_Correct(correctProcentage), m_OuterCircleDecreaseSpeed(1000.f * 0.6f / m_Lifetime)
{
	if (m_AppearenceDelay > 0.f)
	{
		m_State = CircleState::Delay;
		m_StartDelay = std::chrono::high_resolution_clock::now();
	}
	else
	{
		m_State = CircleState::Alive;
		m_StartLife = std::chrono::high_resolution_clock::now();
	}

	m_NumberTextureProps.TintFactor = glm::vec4(0.8f, 0.3f, 0.4f, 0.f);
}

void Circle::Init()
{
	s_CircleTexture = Texture2D::Create("assets/textures/circle.png");
	s_OuterCircleTexture = Texture2D::Create("assets/textures/outer-circle.png");
	s_Number1Texture = Texture2D::Create("assets/textures/number1.png");
	s_ErrorTexture = Texture2D::Create("assets/textures/error.png");
}

void Circle::OnUpdate(Eagle::Timestep ts)
{
	if (m_State == CircleState::Delay)
	{
		float passed = GetPassedTime(m_StartDelay);
		if (passed > m_AppearenceDelay)
		{
			m_State = CircleState::Alive;
			SetLifetime(m_Lifetime);
		}

	}

	if (m_State == CircleState::Alive || m_State == CircleState::ErrorClick)
	{
		float passed = GetPassedTime(m_StartLife);
		if (passed > m_Lifetime)
		{
			m_State = CircleState::Dead;
		}
		else if (passed >= m_Lifetime * m_Correct)
		{
			m_OuterCircleTextureProps.TintFactor.r = 1.f;
		}
	}

	if (m_State == CircleState::Alive)
	{
		Renderer2D::DrawQuad(m_Position, m_CircleSize, s_CircleTexture, m_DefaultTextureProps);
		Renderer2D::DrawQuad(m_Position, m_NumberSize, s_Number1Texture, m_NumberTextureProps);

		if (m_OuterCircleEnabled)
		{
			Renderer2D::DrawQuad(m_Position, m_OuterCircleSize, s_OuterCircleTexture, m_OuterCircleTextureProps);
			m_OuterCircleSize -= m_OuterCircleDecreaseSpeed * ts;
		}
	}
	else if (m_State == CircleState::ErrorClick)
	{
		Renderer2D::DrawQuad(m_Position, m_ErrorSize, s_ErrorTexture, m_DefaultTextureProps);
	}
}

void Circle::SetPosition(const glm::vec2& position)
{
	m_Position = position;
}

void Circle::SetState(CircleState state)
{
	m_State = state;
	if (m_State == CircleState::ErrorClick)
	{
		SetLifetime(200);
	}
}

void Circle::SetOuterCircleDisabled(bool disabled)
{
	m_OuterCircleEnabled = !disabled;
}

bool Circle::OnClicked()
{
	if (m_State != CircleState::Dead)
	{
		float passed = GetPassedTime(m_StartLife);

		if (passed >= m_Lifetime * m_Correct)
		{
			m_State = CircleState::Dead;
			Sound::playAudio("assets/music/hitsound.wav");
		}
		else
		{
			m_State = CircleState::ErrorClick;
			SetLifetime(200.f);
		}
	}

	return m_State == CircleState::Dead;
}

void Circle::Die()
{
	m_State = CircleState::Dead;
}

float Circle::GetPassedTime(const std::chrono::time_point<std::chrono::high_resolution_clock>& from) const
{
	auto end = std::chrono::high_resolution_clock::now();
	float passedMS = std::chrono::duration_cast<std::chrono::milliseconds>(end - from).count();

	return passedMS;
}

void Circle::SetLifetime(float lifetime)
{
	m_Lifetime = lifetime;
	m_StartLife = std::chrono::high_resolution_clock::now();
}


