#pragma once

#include <Eagle.h>
#include "Circle.h"

class CircleSlider
{
public:
	CircleSlider(const glm::vec2& position, float lifetimeMs, float appearenceDelay, float slideDuration, float correctProcentage = 0.7f);
	
	static void Init();

	void OnUpdate(Eagle::Timestep ts);

	inline const glm::vec2 GetPosition() const { return m_Circle.GetPosition(); }
	inline const glm::vec2 GetSize() const { return m_Circle.GetSize(); }

	bool OnClicked();

	inline bool IsAlive() const { return m_Circle.IsAlive(); }
	inline bool IsDead() const { return m_State == CircleState::Dead; }
	inline bool IsActivated() const { return m_Activated; }
	inline bool IsFinished() const { return m_Finished; }

	void Die();
	void SetLifetime(float lifetime);

private:

	Circle m_Circle;
	Eagle::TextureProps m_TextureProps;
	glm::vec2 m_Position = { 0.0f, 0.0f };
	CircleState m_State = CircleState::None;

	float m_Lifetime;
	float m_Correct;
	float m_SliderMin;
	float m_SliderPos;
	float m_SliderMax;
	float m_SliderDuration;

	Eagle::Timestep m_Ts;

	bool m_IsGoodClick = false;
	bool m_SuccessHit = false;
	bool m_Activated = false;
	bool m_Finished = false;

	static Eagle::Ref<Eagle::Texture2D> s_SliderTexture;

};