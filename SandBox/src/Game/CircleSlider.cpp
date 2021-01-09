#include "CircleSlider.h"

#include <Eagle/Sound.h>

using namespace Eagle;

Ref<Texture2D> CircleSlider::s_SliderTexture;

CircleSlider::CircleSlider(const glm::vec2& position, float lifetimeMs, float appearenceDelay, float slideDuration, float correctProcentage)
	: m_Circle({position.x - 0.33f, position.y}, lifetimeMs, appearenceDelay, correctProcentage)
	, m_Position(position)
	, m_Lifetime(lifetimeMs)
	, m_Correct(correctProcentage)
	, m_SliderMin(position.x - 0.33f)
	, m_SliderPos(m_SliderMin)
	, m_SliderMax(m_SliderMin + 0.66f)
	, m_SliderDuration(slideDuration)
{
	m_State = m_Circle.GetState();
}

void CircleSlider::Init()
{
	s_SliderTexture = Texture2D::Create("assets/textures/move-road.png");
}

void CircleSlider::OnUpdate(Eagle::Timestep ts)
{
	m_Ts = ts;

	if (m_State == CircleState::Dead)
	{
		return;
	}

	if (m_Finished)
	{
		if (m_IsGoodClick)
		{
			Sound::playAudio("assets/music/hitsound.wav");
		}
		m_State = CircleState::Dead;
		m_Circle.SetState(m_State);
		return;
	}

	if (m_State == CircleState::Alive)
	{
		Renderer2D::DrawQuad(m_Position, { 2.f, 2.0f }, s_SliderTexture, m_TextureProps);

		if (m_Activated)
		{
			//New pos
			m_Circle.SetState(CircleState::Alive);
			float offsetX = (m_SliderMax - m_SliderMin) * ts / (m_SliderDuration / 1000.f);
			glm::vec2 circleNewPos = m_Circle.GetPosition();
			circleNewPos.x += offsetX;
			circleNewPos.x = std::max(circleNewPos.x, m_SliderMin);
			circleNewPos.x = std::min(circleNewPos.x, m_SliderMax);

			if (std::abs(circleNewPos.x - m_SliderMax) < 1e-6)
			{
				m_Finished = true;
				m_SuccessHit = m_IsGoodClick;
			}
			m_Circle.SetPosition(circleNewPos);
		}

		m_Circle.OnUpdate(ts);
		m_State = m_Circle.GetState();

		m_IsGoodClick = false;
	}
	else if (m_State == CircleState::ErrorClick)
	{
		Renderer2D::DrawQuad(m_Position, { 2.f, 2.0f }, s_SliderTexture, m_TextureProps);
		m_Circle.OnUpdate(ts);
		m_State = m_Circle.GetState();
	}
	else if (m_State == CircleState::Delay)
	{
		m_Circle.OnUpdate(ts);
		m_State = m_Circle.GetState();
	}
}

bool CircleSlider::OnClicked()
{
	if (m_State != CircleState::Dead)
	{
		float passed = m_Circle.LeftToLife();

		if (passed >= m_Lifetime * m_Correct)
		{
			if (m_Activated == false)
			{
				Sound::playAudio("assets/music/hitsound.wav");
				m_Circle.SetOuterCircleDisabled(true);
				m_Lifetime = m_SliderDuration;
				m_Circle.SetLifetime(m_SliderDuration);
			}
			m_IsGoodClick = true;
			m_Activated = true;
		}
		else if (!m_Activated)
		{
			m_State = CircleState::ErrorClick;
			m_Circle.SetState(m_State);
			m_IsGoodClick = false;
		}
	}

	return m_SuccessHit;
}

void CircleSlider::Die()
{
	m_Circle.Die();
}

void CircleSlider::SetLifetime(float lifetime)
{
	m_Circle.SetLifetime(lifetime);
}
