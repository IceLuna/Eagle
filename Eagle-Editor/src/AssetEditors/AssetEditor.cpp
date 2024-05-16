#include "egpch.h"
#include "AssetEditor.h"
#include "Eagle/Asset/Asset.h"

#include <imgui/imgui_internal.h>

namespace Eagle
{
	void AssetEditor::SetInFocus()
	{
		const auto& asset = GetAsset();
		if (!asset)
			return;

		if (ImGuiWindow* window = ImGui::FindWindowByName(asset->GetPath().u8string().c_str()))
			ImGui::FocusWindow(window);
	}
}
