#pragma once

struct ImGuiInputTextCallbackData;

namespace Eagle
{
	class ConsolePanel
	{
	public:
		ConsolePanel() = default;
		void OnImGuiRender();

		void SetOpened(bool bOpen) { m_bOpened = bOpen; }
		bool IsOpened() const { return m_bOpened; }

	private:
		void ExecuteCommand();
		void AddCommandHistory(const std::string& command) { m_CommandsHistory.push_back(command); }
		
		static int CommandCallback(ImGuiInputTextCallbackData* vData);

	private:
		std::vector<std::string> m_CommandsHistory;
		std::string m_Search;
		std::string m_CommandInput;
		size_t m_CurrentHistoryPos = 0;
		bool m_bOpened = true;
		bool m_bAutoScroll = true;
		bool m_bScrollToBottom = false; // Scroll to bottom on command input
	};
}
