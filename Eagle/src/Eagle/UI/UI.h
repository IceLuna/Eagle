#pragma once

#include "Eagle/Renderer/Texture.h"

namespace Eagle
{
	class StaticMeshComponent;
}

namespace Eagle::UI
{
	enum Button : int
	{
		None = 0,
		OK = 0b00000001,
		Cancel = 0b00000010,
		OKCancel = 0b00000011,
	};


	bool DrawTextureSelection(Ref<Texture>& modifyingTexture, const std::string& textureName);
	void DrawStaticMeshSelection(StaticMeshComponent& smComponent, const std::string& smName);

	Button ShowMessage(const std::string& title, const std::string& message, Button buttons);
}
