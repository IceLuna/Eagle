#pragma once

#include <string>

namespace Eagle
{
	namespace FileDialog
	{
		static const char* TEXTURE_FILTER = "Texture (*.png)\0*.png\0";
		static const char* SCENE_FILTER = "Eagle Scene (*.eagle)\0*.eagle\0";
		static const char* MESH_FILTER = "3D-Model (*.fbx)\0*.fbx\0";

		//Returns empty string if failed
		std::string OpenFile(const char* filter);
		std::string SaveFile(const char* filter);
	};

}