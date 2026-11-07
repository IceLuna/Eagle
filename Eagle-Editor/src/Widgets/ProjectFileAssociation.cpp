#include "ProjectFileAssociation.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Utils.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

namespace Eagle
{
	namespace
	{
		enum class PromptState
		{
			NotChecked,
			Hidden,
			Asking,
			ShowingResult
		};

		PromptState s_State = PromptState::NotChecked;
		bool s_bOpenedByUser = false; // By calling `Open()`
		ProjectFileAssociation::Info s_Association;
		std::string s_Result;

		constexpr const char* s_DontAskKey = "DontAskToRegister";

		Path GetSettingsPath()
		{
			return Application::GetCorePath() / "Saved/ProjectFileAssociation";
		}

		bool IsDontAskSet()
		{
			const Path path = GetSettingsPath();
			if (!std::filesystem::exists(path))
				return false;

			try
			{
				YAML::Node node = YAML::Load(FileSystem::ReadText(path));
				if (auto dontAskNode = node[s_DontAskKey])
					return dontAskNode.as<bool>();
			}
			catch (const YAML::Exception& e)
			{
				EG_CORE_WARN("[ProjectFileAssociation] Failed to read '{}': {}", Utils::AsString(path), e.what());
			}
			return false;
		}

		void SetDontAsk()
		{
			const Path path = GetSettingsPath();
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);

			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << s_DontAskKey << YAML::Value << true;
			out << YAML::EndMap;

			std::ofstream fout(path);
			fout << out.c_str();
		}

		std::string GetProgramName(const Path& program)
		{
			return Utils::AsString(program.lexically_normal().make_preferred());
		}

		std::string GetStatusText()
		{
			const std::string extension = Project::GetExtension();
			switch (s_Association.Status)
			{
				case ProjectFileAssociation::AssociationStatus::NotRegistered:
					return "No program opens project files (" + extension + ").";
				case ProjectFileAssociation::AssociationStatus::ProgramMissing:
					return "Project files (" + extension + ") are set to open with a program that doesn't exist anymore:\n" + GetProgramName(s_Association.Program);
				case ProjectFileAssociation::AssociationStatus::ThisProgram:
					return "Project files (" + extension + ") open with this editor.";
				case ProjectFileAssociation::AssociationStatus::OtherProgram:
					return "Project files (" + extension + ") open with another program:\n" + GetProgramName(s_Association.Program);
			}
			EG_CORE_ASSERT(false);
			return {};
		}

		void Register()
		{
			const bool bRegistered = ProjectFileAssociation::Register();
			s_Association = ProjectFileAssociation::Get();

			const std::string extension = Project::GetExtension();
			if (!bRegistered)
			{
				s_Result = "Couldn't register the editor (see the log for details).\n"
					"Win-SetupFileAssociation.bat from the engine's folder registers it as well (it requires admin rights).";
			}
			else if (s_Association.Status == ProjectFileAssociation::AssociationStatus::ThisProgram)
			{
				s_Result = "Done. Project files (" + extension + ") now open with this editor.";
			}
			else
			{
				// The user picked a program with "Open with > Always", and it wins over the registered one
				s_Result = "The editor is registered, but Windows still opens project files with the program that was picked for them in \"Open with\":\n" +
					GetProgramName(s_Association.Program) + "\n\n"
					"To change it, right-click a " + extension + " file > Open with > Choose another app, pick Eagle-Editor, and select \"Always\".";
			}
		}

		bool Button(const char* label)
		{
			const float width = std::max(120.f, ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.f);
			return ImGui::Button(label, ImVec2(width, 0.f));
		}

		void DrawQuestion()
		{
			ImGui::TextWrapped("%s", GetStatusText().c_str());

			const bool bCanRegister = s_Association.Status != ProjectFileAssociation::AssociationStatus::ThisProgram;
			if (bCanRegister)
			{
				ImGui::Spacing();
				ImGui::TextWrapped("Register this editor to open them? Then double-clicking a project opens it in the editor, "
					"and so does Visual Studio's \"Run Eagle Editor\" when using VS extension for debugging");
			}

			ImGui::Separator();

			if (!bCanRegister)
			{
				if (Button("OK"))
				{
					s_State = PromptState::Hidden;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				return;
			}

			if (Button("Register"))
			{
				Register();
				s_State = PromptState::ShowingResult;
			}
			ImGui::SetItemDefaultFocus();

			ImGui::SameLine();
			if (Button(s_bOpenedByUser ? "Cancel" : "Not Now"))
			{
				s_State = PromptState::Hidden;
				ImGui::CloseCurrentPopup();
			}

			if (!s_bOpenedByUser)
			{
				ImGui::SameLine();
				if (Button("Don't Ask Again"))
				{
					SetDontAsk();
					s_State = PromptState::Hidden;
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("You can register it later with Help > Open Projects with this Editor");
			}
		}

		void DrawResult()
		{
			ImGui::TextWrapped("%s", s_Result.c_str());
			ImGui::Separator();

			if (Button("OK"))
			{
				s_State = PromptState::Hidden;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
		}
	}

	void ProjectFileAssociationWidget::Draw()
	{
		if (s_State == PromptState::NotChecked)
		{
			s_State = PromptState::Hidden;
			if (!IsDontAskSet())
			{
				s_Association = ProjectFileAssociation::Get();
				const bool bMissing = s_Association.Status == ProjectFileAssociation::AssociationStatus::NotRegistered ||
					s_Association.Status == ProjectFileAssociation::AssociationStatus::ProgramMissing;
				if (bMissing)
				{
					s_bOpenedByUser = false;
					s_State = PromptState::Asking;
				}
			}
		}

		if (s_State == PromptState::Hidden)
			return;

		constexpr const char* popupTitle = "Project Files";
		ImGui::OpenPopup(popupTitle);

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 36.f);
			if (s_State == PromptState::Asking)
				DrawQuestion();
			else
				DrawResult();
			ImGui::PopTextWrapPos();

			ImGui::EndPopup();
		}
	}

	void ProjectFileAssociationWidget::Open()
	{
		s_Association = ProjectFileAssociation::Get();
		s_bOpenedByUser = true;
		s_State = PromptState::Asking;
	}
}
