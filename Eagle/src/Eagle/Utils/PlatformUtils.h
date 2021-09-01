#pragma once

#include <string>

namespace Eagle
{
	namespace FileDialog
	{
		static const wchar_t* TEXTURE_FILTER = L"Texture (*.png,*.jpg)\0*.png;*.jpg\0";
		static const wchar_t* SCENE_FILTER = L"Eagle Scene (*.eagle)\0*.eagle\0";
		static const wchar_t* MESH_FILTER = L"3D-Model (*.fbx,*.blend,*.3ds,*.obj,*.smd,*.vta,*.stl)|*.fbx;*.blend;*.3ds;*.obj;*.smd;*.vta;*.stl";

		//Returns empty string if failed
		std::filesystem::path OpenFile(const wchar_t* filter);
		std::filesystem::path SaveFile(const wchar_t* filter);
	};
	
	namespace Utils
	{
		void ShowInExplorer(const std::filesystem::path& path);
		void OpenInExplorer(const std::filesystem::path& path);

		bool WereScriptsRebuild();
	}

	namespace Dialog
	{
		bool YesNoQuestion(const std::string& title, const std::string& message);
	}
}