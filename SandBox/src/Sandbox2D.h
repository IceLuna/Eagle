#pragma once

#include "Eagle.h"
#include "Game/Circle.h"
#include "Game/CircleSlider.h"

enum class GameState
{
	None,
	MainMenu,
	Preparing,
	Play,
	Won,
	Lost
};

class Sandbox2D : public Eagle::Layer
{
public:
	Sandbox2D();

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(Eagle::Timestep ts) override;
	void OnEvent(Eagle::Event& e) override;
	void OnImGuiRender() override;

	void OnPlay();
	void OnWon();
	void OnLost();
	void OnReset();
	void OnEndPlay();

	void TestLog();

private:
	bool IsHitButtonPressed() const;

private:
	uint32_t m_Width = 1920, m_Height = 1080;

	Eagle::OrthographicCameraController m_CameraController;
	Eagle::TextureProps m_TextureProps;
	Eagle::Ref<Eagle::Shader> m_Gradient;
	Eagle::Ref<Eagle::Texture2D> m_MainMenuTexture;

	std::vector<Circle> m_Circles;
	std::vector<CircleSlider> m_CircleSliders;
	std::vector<float> m_HitTimes;

	Eagle::Ref<Eagle::DelayCall<std::_Binder<std::remove_cv<std::_Unforced>::type, void (Sandbox2D::*)(), Sandbox2D*>>> m_DelayCall;

	Eagle::Key::KeyCode firstKey = Eagle::Key::Z;
	Eagle::Key::KeyCode secondKey = Eagle::Key::X;

	float m_Life = 1.f;
	float m_LifeDecreaseSpeed = 0.30f;
	float m_GradientCover = 1.2f;
	Eagle::Timestep m_Ts;
	int m_GoodClicks = 0;
	GameState m_GameState = GameState::None;
	bool m_NoFail = false;
	bool m_VSync = true;
};
