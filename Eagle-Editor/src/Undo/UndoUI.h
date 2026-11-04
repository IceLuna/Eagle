#pragma once

namespace Eagle
{
	class MajorTab;

	// Undo/redo for the user: the shortcuts, the Edit menu, and the notifications that tell what was undone/redone.
	// They operate on the history of a major tab (see `MajorTab::GetUndoHistory()`)
	namespace UndoUI
	{
		// Undoes/redoes the last transaction of the tab's history, and shows a notification in the tab's window
		void Undo(MajorTab& tab);
		void Redo(MajorTab& tab);

		// Ctrl+Z (undo), Ctrl+Y and Ctrl+Shift+Z (redo) for the focused tab. They repeat while the keys are held.
		// Call it once per frame, before the tabs are drawn.
		// The keys are read from ImGui rather than from the engine's key events because floating editor windows are separate OS windows,
		// and only ImGui receives their input
		void HandleShortcuts();

		// The Undo/Redo items of the tab's Edit menu
		void DrawMenuItems(MajorTab& tab);

		bool CanUseShortcuts();

		// "Move Entity", "Rotate Component", etc
		// @entitiesCount. How many entities are transformed together
		std::string GetGuizmoTransactionName(int guizmoOperation, bool bComponent, size_t entitiesCount = 1);
	}
}
