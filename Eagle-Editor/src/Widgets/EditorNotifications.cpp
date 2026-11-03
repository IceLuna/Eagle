#include "EditorNotifications.h"

#include <imgui_internal.h>

namespace Eagle
{
	namespace
	{
		struct ShownNotification
		{
			std::string Text;
			ImGuiID ViewportID = 0;
			double StartTime = 0.0;
		};

		constexpr double s_FadeInDuration = 0.12;
		constexpr double s_VisibleDuration = 2.5;
		constexpr double s_FadeOutDuration = 0.6;
		constexpr double s_TotalDuration = s_FadeInDuration + s_VisibleDuration + s_FadeOutDuration;
		constexpr int s_MaxPerViewport = 4;

		std::vector<ShownNotification> s_Notifications; // The oldest first

		float GetAlpha(double age)
		{
			if (age < s_FadeInDuration)
				return float(age / s_FadeInDuration);
			if (age < s_FadeInDuration + s_VisibleDuration)
				return 1.f;
			return float(std::max(0.0, 1.0 - (age - s_FadeInDuration - s_VisibleDuration) / s_FadeOutDuration));
		}
	}

	void EditorNotifications::Show(std::string text, ImGuiID viewportID)
	{
		std::replace(text.begin(), text.end(), '\n', ' '); // One line each
		s_Notifications.push_back({ std::move(text), viewportID, ImGui::GetTime() });
	}

	void EditorNotifications::Draw()
	{
		if (s_Notifications.empty())
			return;

		const double now = ImGui::GetTime();
		const ImGuiID mainViewportID = ImGui::GetMainViewport()->ID;

		for (ShownNotification& notification : s_Notifications)
		{
			if (notification.ViewportID == 0 || !ImGui::FindViewportByID(notification.ViewportID))
				notification.ViewportID = mainViewportID; // For example, the floating frame was closed
		}

		// Expired ones, and the oldest ones of a window that has too many
		{
			std::unordered_map<ImGuiID, int> counts;
			std::vector<bool> bRemove(s_Notifications.size(), false);
			for (size_t i = s_Notifications.size(); i-- > 0;)
			{
				const ShownNotification& notification = s_Notifications[i];
				bRemove[i] = (now - notification.StartTime >= s_TotalDuration) || (++counts[notification.ViewportID] > s_MaxPerViewport);
			}

			size_t kept = 0;
			for (size_t i = 0; i < s_Notifications.size(); ++i)
			{
				if (!bRemove[i])
				{
					if (kept != i)
						s_Notifications[kept] = std::move(s_Notifications[i]);
					++kept;
				}
			}
			s_Notifications.resize(kept);
		}

		const ImGuiStyle& style = ImGui::GetStyle();
		const float fontSize = ImGui::GetFontSize();
		const ImVec2 padding = ImVec2(fontSize * 0.9f, fontSize * 0.55f);
		const float margin = fontSize;
		const float spacing = fontSize * 0.4f;
		const float height = ImGui::GetTextLineHeight() + padding.y * 2.f;

		constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove;

		// The newest one at the bottom. Windows are reused by their position in the stack, so their number stays bounded
		std::unordered_map<ImGuiID, int> slots;
		for (auto it = s_Notifications.rbegin(); it != s_Notifications.rend(); ++it)
		{
			const ShownNotification& notification = *it;
			ImGuiViewport* viewport = ImGui::FindViewportByID(notification.ViewportID);
			const int slot = slots[notification.ViewportID]++;

			const ImVec2 textSize = ImGui::CalcTextSize(notification.Text.c_str());
			const ImVec2 size = ImVec2(std::min(textSize.x + padding.x * 2.f, viewport->WorkSize.x - margin * 2.f), height);
			const ImVec2 pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + viewport->WorkSize.y - margin - float(slot) * (height + spacing));

			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.f, 1.f));
			ImGui::SetNextWindowSize(size, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.95f);

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * GetAlpha(now - notification.StartTime));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, fontSize * 0.3f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);

			char windowName[64];
			snprintf(windowName, sizeof(windowName), "##EditorNotification_%08X_%d", viewport->ID, slot);
			if (ImGui::Begin(windowName, nullptr, flags))
			{
				// Above the editor's windows, without taking the focus
				ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
				ImGui::TextUnformatted(notification.Text.c_str());
			}
			ImGui::End();

			ImGui::PopStyleVar(4);
		}
	}
}
