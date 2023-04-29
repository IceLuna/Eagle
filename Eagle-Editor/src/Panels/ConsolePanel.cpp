#include "egpch.h"
#include "ConsolePanel.h"

#include "Eagle/UI/UI.h"
#include "Eagle/Utils/Utils.h"

#include <imgui_internal.h>

namespace Eagle
{
    static void DisplayHelp();

    struct CommandData
    {
        std::string Command;
        std::string Description;
        std::function<void()> Callback;
    };

    static const std::vector<CommandData> s_AllCommands
    {
        { "Help", "Display all commands", DisplayHelp },
        { "Clear", "Clear console log", Log::ClearLogHistory }
    };

    static void DisplayHelp()
    {
        EG_CORE_INFO("All commands: ");

        size_t i = 1;
        for (auto& command : s_AllCommands)
        {
            EG_CORE_INFO("\t{0}) {1}. {2}", i, command.Command, command.Description);
        }
    }

    static ImVec4 GetLogColor(spdlog::level::level_enum level)
    {
        switch (level)
        {
            case spdlog::level::info: return ImVec4(0, 1, 0, 1);
            case spdlog::level::warn: return ImVec4(1, 1, 0, 1);
            case spdlog::level::err: return ImVec4(1, 0, 0, 1);
            case spdlog::level::critical: return ImVec4(1, 0, 0, 1);
            default: return ImVec4(1, 1, 1, 1);
        }
    }

    int ConsolePanel::CommandCallback(ImGuiInputTextCallbackData* data)
    {
        ConsolePanel* panel = (ConsolePanel*)data->UserData;

        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            data->UserData = &panel->m_CommandInput;
            return UI::TextResizeCallback(data);
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        {
            if (panel->m_CommandsHistory.empty())
                return 0;

            const bool up = data->EventKey == ImGuiKey_UpArrow;

            // If we are showing latest item in the history, we shouldn't wrap around we pressing down
            // Same for up
            const bool bBlockDown = !up && panel->m_CurrentHistoryPos == (panel->m_CommandsHistory.size() - 1);
            const bool bBlockUp = up && panel->m_CurrentHistoryPos == 0;

            if (!(bBlockDown || bBlockUp))
                panel->m_CurrentHistoryPos = (panel->m_CurrentHistoryPos + (up ? -1 : 1)) % panel->m_CommandsHistory.size();

            panel->m_CommandInput = panel->m_CommandsHistory[panel->m_CurrentHistoryPos];

            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, panel->m_CommandInput.c_str());
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
        {
            for (auto& command : s_AllCommands)
            {
                // TODO: Improve to find to closest match. Currently it's looking for the first match
                if (Utils::FindSubstringI(command.Command, panel->m_CommandInput) != std::string::npos)
                {
                    panel->m_CommandInput = command.Command;
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, panel->m_CommandInput.c_str());
                    break;
                }
            }
        }
        return 0;
    }

	void ConsolePanel::OnImGuiRender()
	{
        if (!m_bOpened)
            return;

        ImGui::Begin("Console", &m_bOpened);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
        {
            const float labelwidth = ImGui::CalcTextSize("Options", NULL, true).x;
            const float optionsButtonWidth = labelwidth + GImGui->Style.FramePadding.x * 8.0f;
            const float width = ImGui::GetWindowWidth();
            ImGui::PushItemWidth(width - optionsButtonWidth);
        }

        ImGui::InputTextWithHint("##Search", "Search", m_Search.data(), m_Search.length() + 1, ImGuiInputTextFlags_CallbackResize, UI::TextResizeCallback, &m_Search);

        ImGui::PopItemWidth();
        ImGui::SetItemKeyOwner(ImGuiMod_Alt);

        ImGui::SameLine();

        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");

        if (ImGui::BeginPopup("Options"))
        {
            UI::Property("Auto-scroll", m_bAutoScroll);
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 3.f;
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

            const auto& logs = Log::GetLogHistory();
            for (auto& log : logs)
            {
                if (!m_Search.empty())
                {
                    size_t pos = Utils::FindSubstringI(log.Message, m_Search);
                    if (pos == std::string::npos)
                        continue;
                }

                ImVec4 color = GetLogColor(log.Level);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(log.Message.c_str());
                ImGui::PopStyleColor();
            }

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (m_bScrollToBottom || (m_bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            m_bScrollToBottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaimFocus = false;
        constexpr ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackResize;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
        ImGui::PushItemWidth(-1);

        if (ImGui::InputTextWithHint("##Command", "Command", m_CommandInput.data(), m_CommandInput.length() + 1, inputFlags, ConsolePanel::CommandCallback, this))
        {
            reclaimFocus = true;
            ExecuteCommand();
            AddCommandHistory(m_CommandInput);
            m_CurrentHistoryPos = m_CommandsHistory.size();
            m_CommandInput.clear();
        }

        ImGui::PopItemWidth();
        ImGui::SetItemKeyOwner(ImGuiMod_Alt);

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaimFocus)
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

        ImGui::End();
	}
    
    void ConsolePanel::ExecuteCommand()
    {
        EG_CORE_TRACE("Command: {}", m_CommandInput);

        // TODO: Improve this system
        for (auto& command : s_AllCommands)
        {
            if (_stricmp(m_CommandInput.c_str(), command.Command.c_str()) == 0)
            {
                command.Callback();
                return;
            }
        }
        EG_CORE_WARN("Unknown Command: {}", m_CommandInput);
    }
}
