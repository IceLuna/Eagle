#include "UndoUI.h"
#include "UndoHistory.h"

#include "../Frames/EditorFrames.h"
#include "../Widgets/EditorNotifications.h"

#include "Eagle/Core/AsyncTask.h"
#include "Eagle/Input/Input.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Eagle::UndoUI
{
	static void Apply(MajorTab& tab, bool bUndo)
	{
		UndoHistory* history = tab.GetUndoHistory();
		if (!history)
			return;

		std::string name;
		const UndoHistory::Result result = bUndo ? history->Undo(&name) : history->Redo(&name);
		if (result == UndoHistory::Result::NothingToApply)
			return;

		const ImGuiViewport* viewport = EditorFrames::GetTabViewport(&tab);
		const ImGuiID viewportID = viewport ? viewport->ID : 0;
		if (result == UndoHistory::Result::Applied)
			EditorNotifications::Show(std::string(bUndo ? "Undo: " : "Redo: ") + name, viewportID);
		else
			EditorNotifications::Show(std::string(bUndo ? "Can't undo \"" : "Can't redo \"") + name + "\". It was changed outside of the undo history", viewportID);
	}

	void Undo(MajorTab& tab)
	{
		Apply(tab, true);
	}

	void Redo(MajorTab& tab)
	{
		Apply(tab, false);
	}

	bool CanUseShortcuts()
	{
		// A modal background task is running (it has its own progress popup), or a modal popup is open
		if (AsyncTaskManager::IsModalTaskActive() || ImGui::GetTopMostPopupModal())
			return false;

		// Text fields have their own undo
		if (ImGui::GetIO().WantTextInput)
			return false;

		// In the middle of an edit. It's recorded once it's complete
		return !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsingAny();
	}

	void HandleShortcuts()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!io.KeyCtrl || io.KeyAlt)
			return;

		constexpr bool bRepeat = true;
		const bool bUndo = ImGui::IsKeyPressed(ImGuiKey_Z, bRepeat) && !io.KeyShift;
		const bool bRedo = ImGui::IsKeyPressed(ImGuiKey_Y, bRepeat) || (ImGui::IsKeyPressed(ImGuiKey_Z, bRepeat) && io.KeyShift);
		if (!bUndo && !bRedo)
			return;

		// The camera is being flown. `Input` only knows about the main window, ImGui knows about the floating ones
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || Input::IsMouseButtonPressed(Mouse::ButtonRight))
			return;

		if (!CanUseShortcuts())
			return;

		MajorTab* tab = EditorFrames::GetFocusedTab();
		if (!tab)
			return;

		if (bUndo)
			Undo(*tab);
		else
			Redo(*tab);
	}

	void DrawMenuItems(MajorTab& tab)
	{
		UndoHistory* history = tab.GetUndoHistory();

		// Unlike the shortcuts, the active item isn't checked: it's the menu item while it's being clicked
		const bool bCanUse = !AsyncTaskManager::IsModalTaskActive() && !ImGuizmo::IsUsingAny();
		const bool bCanUndo = history && history->CanUndo() && bCanUse;
		const bool bCanRedo = history && history->CanRedo() && bCanUse;

		const std::string undoLabel = bCanUndo ? "Undo " + history->GetUndoName() : std::string("Undo");
		const std::string redoLabel = bCanRedo ? "Redo " + history->GetRedoName() : std::string("Redo");

		// The labels change, so the IDs are fixed
		if (ImGui::MenuItem((undoLabel + "###Undo").c_str(), "Ctrl+Z", false, bCanUndo))
			Undo(tab);
		if (ImGui::MenuItem((redoLabel + "###Redo").c_str(), "Ctrl+Y", false, bCanRedo))
			Redo(tab);
	}

	std::string GetGuizmoTransactionName(int guizmoOperation, bool bComponent)
	{
		const char* action = "Transform";
		if (guizmoOperation == ImGuizmo::OPERATION::TRANSLATE)
			action = "Move";
		else if (guizmoOperation == ImGuizmo::OPERATION::ROTATE)
			action = "Rotate";
		else if (guizmoOperation == ImGuizmo::OPERATION::SCALE)
			action = "Scale";

		return std::string(action) + (bComponent ? " Component" : " Entity");
	}
}
