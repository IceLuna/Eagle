#include "egpch.h"
#include "CurveEditor.h"

#include <implot.h>

namespace Eagle
{
	static std::string FormatCurveTime(const CurveEditorSettings& settings, float time)
	{
		if (settings.FormatTime)
			return settings.FormatTime(time);

		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%.3f", time);
		return buffer;
	}

	static bool IsDeletePressed()
	{
		return ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput
			&& ImGui::IsKeyPressed(ImGuiKey_Delete, false);
	}

	void CurveEditor::DrawVisibilityToggles(std::vector<CurveEditorEntry>& entries)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float panelRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		for (size_t i = 0; i < entries.size(); ++i)
		{
			// Stay on the line while they fit, wrap otherwise
			if (i > 0)
			{
				const float checkboxWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(entries[i].Label.c_str()).x;
				const float lastItemRight = ImGui::GetItemRectMax().x;
				if (lastItemRight + style.ItemSpacing.x + checkboxWidth <= panelRight)
					ImGui::SameLine();
			}

			ImGui::PushID(int(i));
			ImGui::PushStyleColor(ImGuiCol_CheckMark, entries[i].Color);
			ImGui::Checkbox(entries[i].Label.c_str(), &entries[i].bVisible);
			ImGui::PopStyleColor();
			ImGui::PopID();
		}

		if (!entries.empty())
			ImGui::SameLine();
		if (ImGui::Button("Fit"))
			RequestFit();
	}

	bool CurveEditor::DrawSelectedKeyProperties(const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings)
	{
		std::vector<std::pair<CurveView*, GUID>> selected;
		for (const auto& entry : entries)
		{
			if (!entry.bVisible || !entry.View)
				continue;

			const size_t count = entry.View->GetKeysCount();
			for (size_t i = 0; i < count; ++i)
			{
				const auto item = std::make_pair(entry.View, entry.View->GetKeyID(i));
				if (IsSelected(entry, item.second, settings) && std::find(selected.begin(), selected.end(), item) == selected.end())
					selected.push_back(item);
			}
		}

		ImGui::AlignTextToFramePadding();
		if (selected.empty())
		{
			ImGui::TextDisabled(settings.bAllowKeyEditing ? "Click a key to edit it here, or double-click empty space to add one" : "Click a key to edit it here");
			return false;
		}

		bool bChanged = false;
		ImGui::PushID("##SelectedKey");

		const ImGuiStyle& style = ImGui::GetStyle();
		const float panelRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		const float fieldWidth = 70.f;
		bool bFirstItem = true;

		// Lays out "label [field]" pairs like text: on the same line while they fit, wrapped otherwise
		auto beginField = [&](const char* label)
		{
			const float width = ImGui::CalcTextSize(label).x + style.ItemInnerSpacing.x + fieldWidth;
			if (!bFirstItem && ImGui::GetItemRectMax().x + style.ItemSpacing.x * 2.f + width <= panelRight)
				ImGui::SameLine(0.f, style.ItemSpacing.x * 2.f);
			bFirstItem = false;

			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
			ImGui::SetNextItemWidth(fieldWidth);
		};

		CurveView* firstView = selected[0].first;
		size_t firstIndex = 0;
		if (!firstView->FindKeyIndex(selected[0].second, &firstIndex))
		{
			ImGui::PopID();
			return false;
		}

		if (selected.size() == 1)
		{
			CurveView* view = firstView;
			const GUID keyID = selected[0].second;
			size_t index = firstIndex;

			const float scale = settings.TimeDisplayScale;
			float displayTime = view->GetKeyTime(index) * scale;
			const float maxDisplayTime = settings.TimeMax < std::numeric_limits<float>::max() ? settings.TimeMax * scale : 0.f;
			beginField("Time");
			if (ImGui::DragFloat("##Time", &displayTime, 0.005f * scale, settings.TimeMin * scale, maxDisplayTime, settings.TimeDisplayFormat.c_str()))
			{
				const size_t count = view->GetKeysCount();
				const float lowerBound = index > 0 ? view->GetKeyTime(index - 1) + 1e-3f : settings.TimeMin;
				const float upperBound = index + 1 < count ? view->GetKeyTime(index + 1) - 1e-3f : settings.TimeMax;
				float newTime = displayTime / scale;
				if (settings.SnapTime)
					newTime = settings.SnapTime(newTime);
				if (lowerBound <= upperBound)
				{
					view->SetKeyTime(keyID, glm::clamp(newTime, lowerBound, upperBound));
					view->Sort();
					view->FindKeyIndex(keyID, &index);
					bChanged = true;
				}
			}

			// One field per component
			for (const auto& entry : entries)
			{
				if (entry.View != view)
					continue;

				float value = view->GetKeyComponent(index, entry.Component);
				ImGui::PushID(int(entry.Component));
				beginField(entry.Label.c_str());
				if (ImGui::DragFloat("##Value", &value, 0.01f, 0.f, 0.f, "%.3f"))
				{
					view->SetKeyComponent(keyID, entry.Component, value);
					bChanged = true;
				}
				ImGui::PopID();
			}
		}
		else
		{
			ImGui::Text("%zu keys selected", selected.size());
			bFirstItem = false;
		}

		if (firstView->IsInterpolationEditable())
		{
			static const char* s_Modes[] = { "Constant", "Linear", "Smooth", "Cubic" };
			int mode = glm::clamp(int(firstView->GetKeyInterpolation(firstIndex)), 0, 3);
			beginField("Interpolation");
			if (ImGui::Combo("##Interpolation", &mode, s_Modes, IM_ARRAYSIZE(s_Modes)))
			{
				for (const auto& [view, keyID] : selected)
					view->SetKeyInterpolation(keyID, CurveInterpolation(mode));
				bChanged = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("How the curve goes from this key to the next one. 'Cubic' adds tangent handles");

			if (CurveInterpolation(mode) == CurveInterpolation::Cubic)
			{
				ImGui::SameLine();
				if (ImGui::Button("Flatten"))
				{
					for (const auto& [view, keyID] : selected)
						view->FlattenTangents(keyID);
					bChanged = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Makes the tangents horizontal");
			}
		}

		ImGui::PopID();
		return bChanged;
	}

	bool CurveEditor::IsSelected(const CurveEditorEntry& entry, const GUID& keyID, const CurveEditorSettings& settings) const
	{
		if (settings.IsKeySelected)
			return settings.IsKeySelected(entry, keyID);

		return std::find(m_Selection.begin(), m_Selection.end(), keyID) != m_Selection.end();
	}

	void CurveEditor::Select(const CurveEditorEntry& entry, const GUID& keyID, bool bAdditive, const CurveEditorSettings& settings)
	{
		if (settings.SelectKey)
		{
			settings.SelectKey(entry, keyID, bAdditive);
			return;
		}

		if (!bAdditive)
			m_Selection.clear();
		if (std::find(m_Selection.begin(), m_Selection.end(), keyID) == m_Selection.end())
			m_Selection.push_back(keyID);
	}

	bool CurveEditor::DeleteSelectedKeys(const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings)
	{
		// Entries can share a view (one per component), so gather the keys first and delete each one once
		std::vector<std::pair<CurveView*, GUID>> toDelete;
		for (const auto& entry : entries)
		{
			if (!entry.bVisible || !entry.View)
				continue;

			const size_t count = entry.View->GetKeysCount();
			for (size_t i = 0; i < count; ++i)
			{
				const GUID keyID = entry.View->GetKeyID(i);
				if (!IsSelected(entry, keyID, settings))
					continue;

				const auto item = std::make_pair(entry.View, keyID);
				if (std::find(toDelete.begin(), toDelete.end(), item) == toDelete.end())
					toDelete.push_back(item);
			}
		}

		for (const auto& [view, keyID] : toDelete)
			view->RemoveKey(keyID);

		m_Selection.clear();
		return !toDelete.empty();
	}

	bool CurveEditor::DrawKeyMenu(const std::vector<CurveEditorEntry>& entries)
	{
		CurveView* view = nullptr;
		size_t keyIndex = 0;
		for (const auto& entry : entries)
		{
			if (entry.View && entry.View->FindKeyIndex(m_ContextKeyID, &keyIndex))
			{
				view = entry.View;
				break;
			}
		}

		if (!view)
		{
			ImGui::CloseCurrentPopup();
			return false;
		}

		bool bChanged = false;
		if (view->IsInterpolationEditable())
		{
			static constexpr std::pair<CurveInterpolation, const char*> s_Modes[] =
			{
				{ CurveInterpolation::Constant, "Constant" }, { CurveInterpolation::Linear, "Linear" },
				{ CurveInterpolation::Smooth, "Smooth" }, { CurveInterpolation::Cubic, "Cubic (tangent handles)" },
			};

			const CurveInterpolation current = view->GetKeyInterpolation(keyIndex);
			ImGui::TextDisabled("Interpolation");
			for (const auto& [mode, name] : s_Modes)
			{
				if (ImGui::MenuItem(name, nullptr, current == mode) && current != mode)
				{
					view->SetKeyInterpolation(m_ContextKeyID, mode);
					bChanged = true;
				}
			}

			if (current == CurveInterpolation::Cubic && ImGui::MenuItem("Flatten Tangents"))
			{
				view->FlattenTangents(m_ContextKeyID);
				bChanged = true;
			}
			ImGui::Separator();
		}

		if (ImGui::MenuItem("Delete Key", "Del"))
		{
			view->RemoveKey(m_ContextKeyID);
			m_Selection.erase(std::remove(m_Selection.begin(), m_Selection.end(), m_ContextKeyID), m_Selection.end());
			bChanged = true;
		}
		return bChanged;
	}

	void CurveEditor::AddKeys(const std::vector<CurveEditorEntry>& entries, float time, const float* value)
	{
		const CurveEditorEntry* singleVisible = nullptr;
		size_t visibleCount = 0;
		for (const auto& entry : entries)
		{
			if (entry.bVisible && entry.View)
			{
				singleVisible = &entry;
				++visibleCount;
			}
		}

		// One key per curve
		std::vector<CurveView*> views;
		for (const auto& entry : entries)
			if (entry.bVisible && entry.View && std::find(views.begin(), views.end(), entry.View) == views.end())
				views.push_back(entry.View);

		m_Selection.clear();
		for (CurveView* view : views)
		{
			const GUID keyID = view->AddKeyAtTime(time);
			if (value && visibleCount == 1)
				view->SetKeyComponent(keyID, singleVisible->Component, *value);
			m_Selection.push_back(keyID);
		}
	}

	bool CurveEditor::DrawAddKeyMenu(const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings)
	{
		const std::string label = "Add Key at " + FormatCurveTime(settings, m_ContextTime);
		if (!ImGui::MenuItem(label.c_str()))
			return false;

		AddKeys(entries, m_ContextTime, nullptr);
		return true;
	}

	bool CurveEditor::Draw(const char* id, const std::vector<CurveEditorEntry>& entries, const CurveEditorSettings& settings, ImVec2 size)
	{
		ImGui::PushID(id);

		const ImVec2 available = ImGui::GetContentRegionAvail();
		if (size.x <= 0.f)
			size.x = available.x;
		if (size.y <= 0.f)
			size.y = available.y;

		// With key editing, a double-click adds a key. So ImPlot's double-click-to-fit is moved to the middle mouse button for this plot
		ImPlotInputMap& inputMap = ImPlot::GetInputMap();
		const ImGuiMouseButton previousFitButton = inputMap.Fit;
		if (settings.bAllowKeyEditing)
			inputMap.Fit = ImGuiMouseButton_Middle;

		const ImPlotFlags plotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus;
		if (!ImPlot::BeginPlot("##Curves", size, plotFlags))
		{
			inputMap.Fit = previousFitButton;
			ImGui::PopID();
			return false;
		}

		ImPlot::SetupAxes(settings.TimeAxisLabel.c_str(), nullptr);

		if (bFitRequested)
		{
			bFitRequested = false;

			const float timeStart = settings.FitTimeMin;
			const float timeEnd = glm::max(settings.FitTimeMax, timeStart + 0.01f);
			float minValue = std::numeric_limits<float>::max();
			float maxValue = std::numeric_limits<float>::lowest();

			for (const auto& entry : entries)
			{
				if (!entry.bVisible || !entry.View)
					continue;

				constexpr int fitSamples = 64;
				for (int s = 0; s < fitSamples; ++s)
				{
					const float time = timeStart + (timeEnd - timeStart) * float(s) / float(fitSamples - 1);
					const float value = entry.View->EvaluateComponent(time, entry.Component);
					minValue = glm::min(minValue, value);
					maxValue = glm::max(maxValue, value);
				}
			}

			if (minValue > maxValue)
			{
				minValue = -1.f;
				maxValue = 1.f;
			}
			const float padding = glm::max(0.1f, (maxValue - minValue) * 0.1f);
			const float timePadding = (timeEnd - timeStart) * 0.02f;
			ImPlot::SetupAxesLimits(timeStart - timePadding, timeEnd + timePadding, minValue - padding, maxValue + padding, ImPlotCond_Always);
		}

		const ImPlotRect limits = ImPlot::GetPlotLimits();
		const double xMin = limits.X.Min;
		const double xMax = limits.X.Max;

		constexpr int sampleCount = 300;
		std::vector<glm::dvec2> points(sampleCount);
		for (const auto& entry : entries)
		{
			if (!entry.bVisible || !entry.View)
				continue;

			for (int s = 0; s < sampleCount; ++s)
			{
				const double t = xMin + (xMax - xMin) * double(s) / double(sampleCount - 1);
				const double clampedTime = glm::clamp(t, double(settings.TimeMin), double(settings.TimeMax));
				points[s] = glm::dvec2(t, double(entry.View->EvaluateComponent(float(clampedTime), entry.Component)));
			}

			ImPlotSpec spec{};
			spec.Stride = sizeof(glm::dvec2);
			spec.LineColor = entry.Color;
			spec.LineWeight = 2.f;
			ImPlot::PlotLine(entry.Label.c_str(), &points[0].x, &points[0].y, sampleCount, spec);
		}

		// Key points and tangent handles.
		// Time changes are applied after the loop so indices stay valid while iterating
		struct PendingTime
		{
			CurveView* View = nullptr;
			GUID KeyID = GUID(0, 0);
			float Time = 0.f;
		};
		std::vector<PendingTime> pendingTimes;

		const ImVec4 selectedColor = ImVec4(1.f, 0.8f, 0.25f, 1.f);
		const ImVec4 handleColor = ImVec4(0.9f, 0.9f, 0.9f, 1.f);
		const double handleLength = (xMax - xMin) * 0.06;
		int dragID = 0;
		bool bChanged = false;
		bool bAnyKeyHovered = false;
		bool bOpenKeyMenu = false;

		for (const auto& entry : entries)
		{
			if (!entry.bVisible || !entry.View)
				continue;

			CurveView* view = entry.View;
			const size_t count = view->GetKeysCount();

			for (size_t i = 0; i < count; ++i)
			{
				const GUID keyID = view->GetKeyID(i);
				const bool bSelected = IsSelected(entry, keyID, settings);

				const double keyTime = view->GetKeyTime(i);
				const double keyValue = view->GetKeyComponent(i, entry.Component);

				double x = keyTime;
				double y = keyValue;
				bool bClicked = false;
				bool bHovered = false;
				bool bHeld = false;
				if (ImPlot::DragPoint(dragID++, &x, &y, bSelected ? selectedColor : entry.Color, bSelected ? 6.f : 4.5f,
					ImPlotDragToolFlags_None, &bClicked, &bHovered, &bHeld))
				{
					// Clamp between the neighbours. Letting a key overtake another here would reorder
					// the curve mid-drag, and ImPlot would then hand the drag to a different point
					const float lowerBound = i > 0 ? view->GetKeyTime(i - 1) + 1e-3f : settings.TimeMin;
					const float upperBound = i + 1 < count ? view->GetKeyTime(i + 1) - 1e-3f : settings.TimeMax;
					float newTime = settings.SnapTime ? settings.SnapTime(float(x)) : float(x);
					newTime = lowerBound <= upperBound ? glm::clamp(newTime, lowerBound, upperBound) : float(keyTime);

					view->SetKeyComponent(keyID, entry.Component, float(y));
					if (glm::abs(double(newTime) - keyTime) > 1e-6)
						pendingTimes.push_back({ view, keyID, newTime });

					bChanged = true;
				}

				if (bClicked)
					Select(entry, keyID, ImGui::GetIO().KeyCtrl, settings);

				if (bHovered)
				{
					bAnyKeyHovered = true;
					ImGui::SetTooltip("%s\nTime: %s\nValue: %.3f", entry.Label.c_str(), FormatCurveTime(settings, float(keyTime)).c_str(), keyValue);

					if (settings.bAllowKeyEditing && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
					{
						m_ContextKeyID = keyID;
						bOpenKeyMenu = true;
					}
				}

				// Tangent handles for the selected Cubic keys
				if (bSelected && view->GetKeyInterpolation(i) == CurveInterpolation::Cubic)
				{
					for (int side = 0; side < 2; ++side)
					{
						const bool bOut = side == 1;
						const double direction = bOut ? 1.0 : -1.0;
						const double tangent = view->GetKeyTangent(i, entry.Component, bOut);

						double handleX = keyTime + direction * handleLength;
						double handleY = keyValue + direction * handleLength * tangent;

						glm::dvec2 line[2] = { glm::dvec2(keyTime, keyValue), glm::dvec2(handleX, handleY) };
						ImPlotSpec handleSpec{};
						handleSpec.Stride = sizeof(glm::dvec2);
						handleSpec.LineColor = ImVec4(0.9f, 0.9f, 0.9f, 0.7f);
						handleSpec.LineWeight = 1.f;
						ImPlot::PlotLine("##Tangent", &line[0].x, &line[0].y, 2, handleSpec);

						bool bHandleHovered = false;
						const bool bHandleDragged = ImPlot::DragPoint(dragID++, &handleX, &handleY, handleColor, 3.5f, ImPlotDragToolFlags_None, nullptr, &bHandleHovered, nullptr);
						bAnyKeyHovered |= bHandleHovered;
						if (bHandleDragged)
						{
							// Only accept the handle on its own side of the key, otherwise the slope flips sign
							const double dx = handleX - keyTime;
							if ((bOut && dx > 1e-6) || (!bOut && dx < -1e-6))
							{
								view->SetKeyTangent(keyID, entry.Component, bOut, float((handleY - keyValue) / dx));
								bChanged = true;
							}
						}
					}
				}
			}
		}

		for (const auto& pending : pendingTimes)
		{
			pending.View->SetKeyTime(pending.KeyID, pending.Time);
			pending.View->Sort();
		}

		bool bOpenAddMenu = false;
		const bool bPlotHovered = ImPlot::IsPlotHovered();
		auto mouseTime = [&settings]()
		{
			const float time = float(ImPlot::GetPlotMousePos().x);
			return glm::clamp(settings.SnapTime ? settings.SnapTime(time) : time, settings.TimeMin, settings.TimeMax);
		};

		if (bPlotHovered && !bAnyKeyHovered)
		{
			// The editor's own selection is cleared by clicking empty space (external selections are managed by their owners)
			if (!settings.SelectKey && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				bClearSelectionOnRelease = true;

			if (settings.bAllowKeyEditing && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				const float value = float(ImPlot::GetPlotMousePos().y);
				AddKeys(entries, mouseTime(), &value);
				bClearSelectionOnRelease = false;
				bChanged = true;
			}

			if (settings.bAllowKeyEditing && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				m_ContextTime = mouseTime();
				bOpenAddMenu = true;
			}
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			// Releasing after a drag pans the plot instead
			const float clickThreshold = ImGui::GetIO().MouseDragThreshold;
			const bool bWasClick = ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <= clickThreshold * clickThreshold;
			if (bClearSelectionOnRelease && bWasClick)
				m_Selection.clear();
			bClearSelectionOnRelease = false;
		}

		if (settings.bAllowKeyEditing && IsDeletePressed())
			bChanged |= DeleteSelectedKeys(entries, settings);

		if (settings.PlayheadTime)
		{
			double playhead = *settings.PlayheadTime;
			if (ImPlot::DragLineX(1 << 20, &playhead, ImVec4(0.9f, 0.25f, 0.25f, 1.f), 1.5f) && settings.SetPlayheadTime)
				settings.SetPlayheadTime(float(playhead));
		}

		ImPlot::EndPlot();
		inputMap.Fit = previousFitButton;

		if (bOpenKeyMenu)
			ImGui::OpenPopup("##CurveKeyMenu");
		if (bOpenAddMenu)
			ImGui::OpenPopup("##CurveAddKeyMenu");

		if (ImGui::BeginPopup("##CurveKeyMenu"))
		{
			bChanged |= DrawKeyMenu(entries);
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("##CurveAddKeyMenu"))
		{
			bChanged |= DrawAddKeyMenu(entries, settings);
			ImGui::EndPopup();
		}

		ImGui::PopID();
		return bChanged;
	}
}
