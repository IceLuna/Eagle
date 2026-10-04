#pragma once

#include "CurveView.h"

#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace Eagle
{
	// One scalar curve drawn by `CurveEditor`: a component of a channel
	struct CurveEditorEntry
	{
		CurveView* View = nullptr;
		uint32_t Component = 0;
		ImVec4 Color = ImVec4(1.f, 1.f, 1.f, 1.f);
		std::string Label;
		uint32_t UserIndex = 0; // Passed back to the selection callbacks (for example, a channel index)
		bool bVisible = true;
	};

	struct CurveEditorSettings
	{
		std::string TimeAxisLabel = "Time (s)";

		// X range shown when the view is fitted
		float FitTimeMin = 0.f;
		float FitTimeMax = 1.f;

		// Keys can't be moved outside of this range
		float TimeMin = 0.f;
		float TimeMax = std::numeric_limits<float>::max();

		// When true, keys can be added, deleted and their interpolation changed right in the plot
		bool bAllowKeyEditing = false;

		std::function<float(float)> SnapTime;         // Optional
		std::function<std::string(float)> FormatTime; // Optional. Used by tooltips and menus

		// The time is multiplied by the scale (for example, 100 for percents)
		float TimeDisplayScale = 1.f;
		std::string TimeDisplayFormat = "%.3f";

		// Optional external selection. When not set, the editor keeps its own selection
		std::function<bool(const CurveEditorEntry&, const GUID& keyID)> IsKeySelected;
		std::function<void(const CurveEditorEntry&, const GUID& keyID, bool bAdditive)> SelectKey;

		// Optional playhead, drawn as a draggable vertical line
		const float* PlayheadTime = nullptr;
		std::function<void(float)> SetPlayheadTime;
	};

	class CurveEditor
	{
	public:
		// Returns true if any key was changed
		bool Draw(const char* id, const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings, ImVec2 size = ImVec2(0.f, 0.f));

		void RequestFit() { bFitRequested = true; }
		void ClearSelection() { m_Selection.clear(); }

		void DrawVisibilityToggles(std::vector<CurveEditorEntry>& entries);

		// Returns true if a key was changed
		bool DrawSelectedKeyProperties(const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings);

	private:
		bool IsSelected(const CurveEditorEntry& entry, const GUID& keyID, const CurveEditorSettings& settings) const;
		void Select(const CurveEditorEntry& entry, const GUID& keyID, bool bAdditive, const CurveEditorSettings& settings);
		bool DeleteSelectedKeys(const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings);
		bool DrawKeyMenu(const std::vector<CurveEditorEntry>& entries);
		bool DrawAddKeyMenu(const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings);
		// Adds a key at `time` to every visible curve and selects the new keys.
		// When a single curve is visible, the key gets `value`; otherwise every new key sits on its curve
		void AddKeys(const std::vector<CurveEditorEntry>& entries, float time, const float* value);

	private:
		std::vector<GUID> m_Selection; // Used when there's no external selection
		bool bFitRequested = true;

		GUID m_ContextKeyID = GUID(0, 0);
		float m_ContextTime = 0.f;

		bool bClearSelectionOnRelease = false;
	};
}
