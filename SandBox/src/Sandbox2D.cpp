#include "SandBox2D.h"

#include <Platform/OpenGL/OpenGLShader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Eagle/Core/Core.h>

#include <imgui/imgui.h>

#include <Eagle/Sound.h>

#include <GLFW/include/GLFW/glfw3.h>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"),
	m_CameraController((float)m_Width / (float)m_Height)
{
	
}

void Sandbox2D::OnAttach()
{
	Circle::Init();
	CircleSlider::Init();
	m_GameState = GameState::MainMenu;

	m_Gradient = Eagle::Shader::Create("assets/shaders/Gradient.glsl");
	m_Gradient->Bind();
	m_Gradient->SetFloat2("u_Resolution", {m_Width, m_Height});

	m_MainMenuTexture = Eagle::Texture2D::Create("assets/textures/mainmenu.png");
	
	Eagle::Sound::Init();
	
	Eagle::Sound::playAudio("assets/music/Main Menu.wav");

	m_HitTimes.reserve(26);
	m_HitTimes.push_back(234.f);
	m_HitTimes.push_back(367.f);
	m_HitTimes.push_back(601.f);
	m_HitTimes.push_back(834.f);
	m_HitTimes.push_back(1235.f);
	m_HitTimes.push_back(1435.f);
	m_HitTimes.push_back(1668.f);
	m_HitTimes.push_back(1869.f);
	m_HitTimes.push_back(2069.f);
	m_HitTimes.push_back(2269.f);
	m_HitTimes.push_back(2436.f);
	m_HitTimes.push_back(3537.f);
	m_HitTimes.push_back(3737.f);
	m_HitTimes.push_back(3937.f);
	m_HitTimes.push_back(4171.f);
	m_HitTimes.push_back(4605.f);
	m_HitTimes.push_back(4771.f);
	m_HitTimes.push_back(5005.f);
	m_HitTimes.push_back(5205.f);
	m_HitTimes.push_back(5372.f);
	m_HitTimes.push_back(5606.f);
	m_HitTimes.push_back(5839.f);
	m_HitTimes.push_back(6440.f);
	m_HitTimes.push_back(6607.f);
}

void Sandbox2D::OnDetach()
{
	m_Circles.clear();
	m_CircleSliders.clear();
	m_HitTimes.clear();
}

void Sandbox2D::OnUpdate(Eagle::Timestep ts)
{
	using namespace Eagle;
	EG_PROFILE_FUNCTION();
	
	m_Ts = ts;

	if (m_GameState != GameState::Lost)
	{
		m_GradientCover = 2.f;
	}
	else
	{
		m_GradientCover -= 0.4f * ts;
	}

	//m_CameraController.OnUpdate(ts);

	m_Gradient->Bind();
	m_Gradient->SetFloat("u_Time", (float)glfwGetTime());
	m_Gradient->SetFloat("u_Cover", m_GradientCover);

	if (m_GameState == GameState::MainMenu || m_GameState == GameState::Won)
	{
		m_Gradient->SetFloat("u_ShowCoef", 1.f);
	}
	else
	{
		m_Gradient->SetFloat("u_ShowCoef", 0.5f);
	}

	{
		EG_PROFILE_SCOPE("Sandbox2D::Clear");
		Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Renderer::Clear();
	}

	{
		EG_PROFILE_SCOPE("Sandbox2D::Draw");
		Renderer2D::BeginScene(m_CameraController.GetCamera());

		Renderer2D::DrawQuad( {0.f, 0.f}, {5.f, 5.f}, m_Gradient);

		if (m_GameState == GameState::MainMenu)
		{
			Renderer2D::DrawQuad({0.0f, 0.5f}, {3.f, 0.6f}, m_MainMenuTexture, m_TextureProps);
		}

		if (m_GameState == GameState::Play)
		{
			for (auto& circle : m_Circles)
			{
				circle.OnUpdate(ts);
			}
			for (auto& slider : m_CircleSliders)
			{
				slider.OnUpdate(ts);
			}
		}

		Renderer::EndScene();
	}

	if (m_GameState == GameState::Play)
	{
		if (m_Life <= 0.f && (!m_NoFail))
		{
			OnLost();
		}
		else if ((m_CircleSliders.back().IsDead() == false))
		{
			m_Life -= m_LifeDecreaseSpeed * ts;
		}
		else
		{
			OnWon();
		}

		if (IsHitButtonPressed() && (m_GameState == GameState::Play))
		{
			EG_PROFILE_SCOPE("Click Detection");
			const float aspectRatio = (float)m_Width / (float)m_Height;

			//Converting MousePos to Normalized DC.
			const float mouseX = aspectRatio * (Input::GetMouseX() - (m_Width / 2.f)) / (m_Width / 2.f);
			const float mouseY = -(Input::GetMouseY() - (m_Height / 2.f)) / (m_Height / 2.f);

			for (auto& circle : m_Circles)
			{
				if (!circle.IsAlive())
				{
					continue;
				}
				const glm::vec2& circleOrigin = circle.GetPosition();
				const glm::vec2& circleSize = circle.GetSize();
				const float R = 0.1416f; //Hardcoded radius of an circle texture.
				const float y = sqrt((R * R) - pow((mouseX - circleOrigin.x), 2));

				const float MaxY = (circleOrigin.y + y);
				const float MinY = (circleOrigin.y - y);

				if (mouseY >= MinY && mouseY <= MaxY)
				{
					if (circle.OnClicked()) //Click in time?
					{
						++m_GoodClicks;
						m_Life += 0.1f;
						m_Life = std::min(m_Life, 1.f);
					}
					else
					{
						m_Life -= 0.1f;
						m_Life = std::max(m_Life, 0.f);
					}
				}
			}

			for (auto& slider : m_CircleSliders)
			{
				if (!slider.IsAlive())
				{
					continue;
				}
				const glm::vec2& circleOrigin = slider.GetPosition();
				const glm::vec2& circleSize = slider.GetSize();
				float R = 0.1416f; //Hardcoded radius of an circle texture.

				if (slider.IsActivated())
				{
					R *= 2.f;
				}

				const float y = sqrt((R * R) - pow((mouseX - circleOrigin.x), 2));

				const float MaxY = (circleOrigin.y + y);
				const float MinY = (circleOrigin.y - y);

				if (mouseY >= MinY && mouseY <= MaxY)
				{
					if (slider.OnClicked()) //Click in time?
					{
						++m_GoodClicks;
						m_Life += 0.1f;
						m_Life = std::min(m_Life, 1.f);
					}
					else if (slider.IsFinished())
					{
						m_Life -= 0.1f;
						m_Life = std::max(m_Life, 0.f);
					}
				}
			}
		}
	}

}

void Sandbox2D::OnEvent(Eagle::Event& e)
{
	m_CameraController.OnEvent(e);

	if (Eagle::Input::IsKeyPressed(Eagle::Key::Escape))
	{
		if (m_GameState == GameState::Play || m_GameState == GameState::Lost)
		{
			if (m_DelayCall)
				m_DelayCall->Stop();
			OnEndPlay();
		}
	}

	if (IsHitButtonPressed())
	{
		if (m_GameState == GameState::Lost)
		{
			if (m_DelayCall)
				m_DelayCall->Stop();
			OnReset();
		}
	}

	if (e.GetEventType() == Eagle::EventType::WindowResize)
	{
		Eagle::WindowResizeEvent& event = (Eagle::WindowResizeEvent&)e;
		m_Width = event.GetWidth();
		m_Height = event.GetHeight();

		m_Gradient->Bind();
		m_Gradient->SetFloat2("u_Resolution", { m_Width, m_Height });
	}
}

void Sandbox2D::OnImGuiRender()
{
	EG_PROFILE_FUNCTION();

	if (m_GameState == GameState::MainMenu)
	{
		std::string label = "FPS: ";
		int fps = (1.f / m_Ts);
		label += std::to_string(fps);

		//ImGui::Begin(label.c_str());
		ImGui::Begin("MainMenu", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("\t\tOsu\nMade in Eagle Engine.");

		if (ImGui::Button("Play"))
		{
			m_GameState = GameState::Preparing;
			m_Life = 1.f;
			Eagle::Sound::Shutdown();
			if (m_DelayCall)
				m_DelayCall->Stop();
			m_DelayCall.reset(EG_SET_TIMER_BY_FUNC(Sandbox2D::OnPlay, 500u));
		}
		ImGui::Checkbox("No fail", &m_NoFail);
		if (ImGui::Checkbox("VSync", &m_VSync))
		{
			Eagle::Application::Get().GetWindow().SetVSync(m_VSync);
		}
		ImGui::Text(label.c_str());
		ImGui::NewLine();
		if (ImGui::Button("Quit"))
		{
			Eagle::Application::Get().SetShouldClose(true);
		}
		ImGui::End();
	}
	else
	{
		std::string label = "FPS: ";
		int fps = (1.f / m_Ts);
		label += std::to_string(fps);

		ImGui::Begin("Life", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
		ImGui::ProgressBar(m_Life);
		ImGui::Text(label.c_str());
		//ImGui::DragInt("Good Clicks", &m_GoodClicks);
		ImGui::End();
	}
}

void Sandbox2D::OnPlay()
{
	Eagle::Sound::Shutdown();
	Eagle::Sound::Init();
	Eagle::Sound::playAudio("assets/music/PlayMusic.wav");

	{
		float x = -0.8f, y = -0.8f;
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[0] + 50.f, 0.f); //1
		x += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[1] + 50.f, 0.f); //2
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[2] + 50.f, 0.f); //3
		x += 0.25f;

		m_CircleSliders.emplace_back(glm::vec2(x + 0.5f, y), m_HitTimes[3] + 50.f, 0.f, 330.f, 0.5f); //4
		x += 0.35f;
		y += 0.45f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[4] + 50.f - m_HitTimes[3], m_HitTimes[3]); //5
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[5] + 50.f - m_HitTimes[3], m_HitTimes[3]); //6
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[6] + 50.f - m_HitTimes[3], m_HitTimes[3]); //7.1
		x = -0.8f;
		y += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[7] - m_HitTimes[3], m_HitTimes[3]); //7.2
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[8] - m_HitTimes[3], m_HitTimes[3]); //8
		x += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[9] - m_HitTimes[3], m_HitTimes[3]); //9
		x += 0.25f;
		
		m_CircleSliders.emplace_back(glm::vec2(x + 0.5f, y), m_HitTimes[10] + 50.f, 0.f, 730.f, 0.5f); //4
		x += 0.35f;
		y += 0.45f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[11] - m_HitTimes[10] + 300.f, m_HitTimes[10] - 100.f);
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[12] - m_HitTimes[10] + 300.f, m_HitTimes[10] - 100.f);
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[13] - m_HitTimes[10] + 300.f, m_HitTimes[10] - 100.f);
		x = -0.8f;
		y += 0.25f;

		x = -0.8f; y = -0.8f;
		
		m_CircleSliders.emplace_back(glm::vec2(x + 0.5f, y), m_HitTimes[14] - m_HitTimes[11] + 300.f, m_HitTimes[11] - 100.f, 400.f, 0.5f); //4
		x = -0.8f;
		y += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[15] - m_HitTimes[14] + 300.f, m_HitTimes[14] - 100.f);
		x += 0.25f;
		
		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[16] - m_HitTimes[14] + 300.f, m_HitTimes[14] - 100.f);
		x += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[17] - m_HitTimes[14] + 300.f, m_HitTimes[14] - 100.f);
		x += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[18] - m_HitTimes[14] + 300.f, m_HitTimes[14] - 100.f);
		x += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[19] - m_HitTimes[14] + 300.f, m_HitTimes[14] - 100.f);
		x += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[20] - m_HitTimes[14] + 300.f, m_HitTimes[14] - 100.f);
		x += 0.25f;

		m_CircleSliders.emplace_back(glm::vec2(x + 0.5f, y), m_HitTimes[21] - m_HitTimes[17] + 300.f, m_HitTimes[17] - 100.f, 500.f, 0.5f); //4
		x = -0.8f;
		y += 0.25f;

		m_Circles.emplace_back(glm::vec2(x, y), m_HitTimes[22] - m_HitTimes[21] + 300.f, m_HitTimes[21] - 100.f);
		x += 0.25f;

		m_CircleSliders.emplace_back(glm::vec2(x + 0.5f, y), m_HitTimes[23] - m_HitTimes[21] + 300.f, m_HitTimes[21] - 100.f, 400.f, 0.5f); //4
		x += 0.25f;
	}

	m_Life = 1.f;
	m_GameState = GameState::Play;
	m_GoodClicks = 0;

	if (m_DelayCall)
		m_DelayCall->Stop();
}

void Sandbox2D::OnWon()
{
	m_GameState = GameState::Won;

	Eagle::Sound::Shutdown();
	Eagle::Sound::Init();
	Eagle::Sound::playAudio("assets/music/win.wav");

	if (m_DelayCall)
		m_DelayCall->Stop();
	m_DelayCall.reset(EG_SET_TIMER_BY_FUNC(Sandbox2D::OnEndPlay, 3000u));
}

void Sandbox2D::OnLost()
{
	Eagle::Sound::Shutdown();
	Eagle::Sound::Init();
	Eagle::Sound::playAudio("assets/music/fail.wav");
	m_GameState = GameState::Lost;

	if (m_DelayCall)
		m_DelayCall->Stop();
	m_DelayCall.reset(EG_SET_TIMER_BY_FUNC(Sandbox2D::OnReset, 5000u));
}

void Sandbox2D::OnReset()
{
	m_Circles.clear();
	m_CircleSliders.clear();
	m_Circles.reserve(20);
	m_CircleSliders.reserve(5);
	OnPlay();
}

void Sandbox2D::OnEndPlay()
{
	m_GameState = GameState::MainMenu;

	Eagle::Sound::Shutdown();
	Eagle::Sound::Init();
	Eagle::Sound::playAudio("assets/music/Main Menu.wav");

	m_Circles.clear();
	m_CircleSliders.clear();
}

void Sandbox2D::TestLog()
{
	EG_WARN("TEST LOG!");
}

bool Sandbox2D::IsHitButtonPressed() const
{
	using namespace Eagle;

	return Input::IsMouseButtonPressed(Mouse::ButtonLeft) || Input::IsKeyPressed(firstKey) || Input::IsKeyPressed(secondKey);
}
