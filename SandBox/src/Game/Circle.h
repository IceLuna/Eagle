#pragma once

#include <Eagle.h>

enum class CircleState
{
	None,
	Delay,
	Alive,
	ErrorClick,
	Dead
};

class Circle
{
public:
	Circle(const glm::vec2& position, float lifetimeMs, float appearenceDelay, float correctProcentage = 0.6f);
	static void Init();

	void OnUpdate(Eagle::Timestep ts);

	void SetPosition(const glm::vec2& position);
	void SetState(CircleState state);
	void SetOuterCircleDisabled(bool disabled);

	inline const glm::vec2 GetPosition() const { return m_Position; }
	inline const glm::vec2 GetSize() const { return m_CircleSize; }
	inline CircleState GetState() const { return m_State; }

	inline float LeftToLife() const { return GetPassedTime(m_StartLife); }

	bool OnClicked();

	inline bool IsAlive() const { return m_State == CircleState::Alive; }

	void SetLifetime(float lifetime);
	
	void Die();

private:

	float GetPassedTime(const std::chrono::time_point<std::chrono::high_resolution_clock>& from) const;

private:

	glm::vec2 m_Position = { 0.0f, 0.0f };

	Eagle::TextureProps m_DefaultTextureProps;
	Eagle::TextureProps m_NumberTextureProps;
	Eagle::TextureProps m_OuterCircleTextureProps;
	Eagle::Ref<Eagle::Texture2D> m_NumberTexture;

	std::chrono::time_point<std::chrono::high_resolution_clock> m_StartLife;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_StartDelay;

	static Eagle::Ref<Eagle::Texture2D> s_CircleTexture;
	static Eagle::Ref<Eagle::Texture2D> s_OuterCircleTexture;
	static Eagle::Ref<Eagle::Texture2D> s_ErrorTexture;
	static Eagle::Ref<Eagle::Texture2D> s_Number1Texture;

	static glm::vec2 m_CircleSize;
	static glm::vec2 m_ErrorSize;
	static glm::vec2 m_NumberSize;

	glm::vec2 m_OuterCircleSize = glm::vec2(1.f);
	CircleState m_State = CircleState::None;
	float m_Lifetime;
	float m_AppearenceDelay;
	float m_Correct;
	float m_OuterCircleDecreaseSpeed;

	bool m_OuterCircleEnabled = true;
};