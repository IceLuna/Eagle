#pragma once

#include <string>

namespace Eagle
{
	namespace FileDialog
	{
		static const char* TEXTURE_FILTER = "Texture (*.png,*.jpg)\0*.png;*.jpg\0";
		static const char* SCENE_FILTER = "Eagle Scene (*.eagle)\0*.eagle\0";
		static const char* MESH_FILTER = "3D-Model (*.fbx,*.blend,*.3ds,*.obj,*.smd,*.vta,*.stl)|*.fbx;*.blend;*.3ds;*.obj;*.smd;*.vta;*.stl";

		//Returns empty string if failed
		std::string OpenFile(const char* filter);
		std::string SaveFile(const char* filter);
	};

	namespace Dialog
	{
		bool YesNoQuestion(const std::string& title, const std::string& message);
	}
}