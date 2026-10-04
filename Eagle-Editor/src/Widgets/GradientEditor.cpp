#include "egpch.h"
#include "GradientEditor.h"

namespace Eagle
{
	static ImU32 ToDisplayColor(const glm::vec4& color, bool bOpaque = false)
	{
		const glm::vec4 c = glm::clamp(color, glm::vec4(0.f), glm::vec4(1.f));
		return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, bOpaque ? 1.f : c.a));
	}

	static bool IsDeletePressed()
	{
		return ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput
			&& ImGui::IsKeyPressed(ImGuiKey_Delete, false);
	}

	void GradientEditor::DrawGradient(ImDrawList* drawList, const Curve<glm::vec4>& gradient, const ImVec2& min, const ImVec2& max, const glm::vec4& defaultColor)
	{
		// Checkerboard first, so transparent parts of the gradient are visible
		const float cell = glm::max(4.f, (max.y - min.y) * 0.5f);
		int column = 0;
		for (float x = min.x; x < max.x; x += cell, ++column)
		{
			int row = 0;
			for (float y = min.y; y < max.y; y += cell, ++row)
			{
				const ImU32 color = ((column + row) & 1) ? IM_COL32(200, 200, 200, 255) : IM_COL32(130, 130, 130, 255);
				drawList->AddRectFilled(ImVec2(x, y), ImVec2(glm::min(x + cell, max.x), glm::min(y + cell, max.y)), color);
			}
		}

		constexpr int slices = 64;
		const float width = max.x - min.x;
		for (int i = 0; i < slices; ++i)
		{
			const float t0 = float(i) / float(slices);
			const float t1 = float(i + 1) / float(slices);
			const ImU32 c0 = ToDisplayColor(gradient.Evaluate(t0, defaultColor));
			const ImU32 c1 = ToDisplayColor(gradient.Evaluate(t1, defaultColor));
			drawList->AddRectFilledMultiColor(ImVec2(min.x + width * t0, min.y), ImVec2(min.x + width * t1, max.y), c0, c1, c1, c0);
		}
		drawList->AddRect(min, max, IM_COL32(20, 20, 20, 255));
	}

	bool GradientEditor::Draw(const char* id, Curve<glm::vec4>& gradient)
	{
		bool bChanged = false;
		ImGui::PushID(id);

		constexpr float barHeight = 24.f;
		constexpr float markerSize = 12.f;
		const float width = glm::max(ImGui::GetContentRegionAvail().x, 50.f);
		const ImVec2 barMin = ImGui::GetCursorScreenPos();
		const ImVec2 barMax = ImVec2(barMin.x + width, barMin.y + barHeight);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		auto timeToX = [&](float time) { return barMin.x + width * glm::clamp(time, 0.f, 1.f); };
		auto xToTime = [&](float x) { return glm::clamp((x - barMin.x) / width, 0.f, 1.f); };

		// Clicking the bar adds a stop with the color the gradient already has there
		ImGui::InvisibleButton("##Bar", ImVec2(width, barHeight));
		DrawGradient(drawList, gradient, barMin, barMax);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Click to add a color stop");
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			const float time = xToTime(ImGui::GetIO().MousePos.x);
			const glm::vec4 color = gradient.Evaluate(time, glm::vec4(1.f));
			const size_t index = gradient.AddKey(time, color, CurveInterpolation::Linear);
			m_SelectedKey = gradient.GetKey(index).ID;
			bChanged = true;
		}

		// Stops, drawn as small arrows under the bar
		const float markersTop = barMax.y + 2.f;
		bool bTimesChanged = false;
		bool bOpenContextMenu = false;
		auto& keys = gradient.GetKeys();
		for (size_t i = 0; i < keys.size(); ++i)
		{
			auto& key = keys[i];
			const float x = timeToX(key.Time);

			ImGui::PushID((void*)key.ID.GetHash());
			ImGui::SetCursorScreenPos(ImVec2(x - markerSize * 0.5f, markersTop));
			ImGui::InvisibleButton("##Stop", ImVec2(markerSize, markerSize));
			const bool bHovered = ImGui::IsItemHovered();

			if (ImGui::IsItemActivated())
			{
				m_SelectedKey = key.ID;
				m_DraggedKey = key.ID;
			}
			if (ImGui::IsItemActive() && m_DraggedKey == key.ID && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.f))
			{
				const float newTime = xToTime(ImGui::GetIO().MousePos.x);
				if (newTime != key.Time)
				{
					key.Time = newTime;
					bTimesChanged = true;
				}
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				m_ContextKey = key.ID;
				m_SelectedKey = key.ID;
				bOpenContextMenu = true;
			}
			if (bHovered)
				ImGui::SetTooltip("Drag to move. Right-click for options, or press Delete to remove the selected stop\nLocation: %.0f%%", key.Time * 100.f);
			ImGui::PopID();

			const bool bSelected = key.ID == m_SelectedKey;
			const ImVec2 tip(x, markersTop);
			const ImVec2 left(x - markerSize * 0.5f, markersTop + markerSize);
			const ImVec2 right(x + markerSize * 0.5f, markersTop + markerSize);
			drawList->AddTriangleFilled(tip, right, left, ToDisplayColor(key.Value, true));
			drawList->AddTriangle(tip, right, left, (bSelected || bHovered) ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 255), bSelected ? 2.f : 1.f);
		}

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			m_DraggedKey = GUID(0, 0);

		if (bTimesChanged)
		{
			gradient.SortKeys();
			bChanged = true;
		}

		ImGui::SetCursorScreenPos(ImVec2(barMin.x, markersTop + markerSize + ImGui::GetStyle().ItemSpacing.y));

		if (keys.size() > 1 && gradient.FindKey(m_SelectedKey) && IsDeletePressed())
		{
			gradient.RemoveKey(m_SelectedKey);
			m_SelectedKey = GUID(0, 0);
			bChanged = true;
		}

		if (bOpenContextMenu)
			ImGui::OpenPopup("##StopMenu");
		if (ImGui::BeginPopup("##StopMenu"))
		{
			const bool bCanDelete = keys.size() > 1;
			if (ImGui::MenuItem("Delete Stop", nullptr, false, bCanDelete))
			{
				gradient.RemoveKey(m_ContextKey);
				bChanged = true;
			}
			ImGui::EndPopup();
		}

		if (auto* key = gradient.FindKey(m_SelectedKey))
		{
			ImGui::PushItemWidth(-1.f);

			float location = key->Time * 100.f;
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Location");
			ImGui::SameLine();
			if (ImGui::DragFloat("##Location", &location, 0.5f, 0.f, 100.f, "%.1f%%"))
			{
				key->Time = glm::clamp(location * 0.01f, 0.f, 1.f);
				gradient.SortKeys();
				key = gradient.FindKey(m_SelectedKey); // Sorting moves keys around
				bChanged = true;
			}

			if (key)
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Color   ");
				ImGui::SameLine();
				bChanged |= ImGui::ColorEdit4("##Color", &key->Value.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);

				static const char* s_Modes[] = { "Constant (step)", "Linear", "Smooth" };
				int mode = glm::clamp(int(key->Interpolation), 0, 2);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Blend   ");
				ImGui::SameLine();
				if (ImGui::Combo("##Blend", &mode, s_Modes, IM_ARRAYSIZE(s_Modes)))
				{
					key->Interpolation = CurveInterpolation(mode);
					bChanged = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("How the color changes from this stop to the next one");
			}

			ImGui::PopItemWidth();
		}
		else
		{
			ImGui::TextDisabled("Select a stop to edit its color");
		}

		ImGui::PopID();
		return bChanged;
	}
}
