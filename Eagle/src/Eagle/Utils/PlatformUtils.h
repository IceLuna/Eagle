#pragma once

#include <string>
#include "Eagle/Core/DataBuffer.h"

namespace Eagle
{
	namespace FileDialog
	{
		static const wchar_t* TEXTURE_FILTER = L"Texture (*.png,*.jpg)\0*.png;*.jpg\0";
		static const wchar_t* SCENE_FILTER = L"Eagle Scene (*.eagle)\0*.eagle\0";
		static const wchar_t* MESH_FILTER = L"3D-Model (*.fbx,*.blend,*.3ds,*.obj,*.smd,*.vta,*.stl)|*.fbx;*.blend;*.3ds;*.obj;*.smd;*.vta;*.stl";

		//Returns empty string if failed
		Path OpenFile(const wchar_t* filter);
		Path SaveFile(const wchar_t* filter);
	};
	
	namespace FileSystem
	{
		bool Write(const Path& path, const DataBuffer& buffer);
		[[nodiscard]] DataBuffer Read(const Path& path);

		// Returns absolute path
		Path GetFullPath(const Path& path);
	}

	namespace Utils
	{
		void ShowInExplorer(const Path& path);
		void OpenInExplorer(const Path& path);

		bool WereScriptsRebuild();
	}

	namespace Dialog
	{
		bool YesNoQuestion(const std::string& title, const std::string& message);
	}
}