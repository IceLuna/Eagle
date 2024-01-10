#pragma once

#include <string>
#include "Eagle/Core/DataBuffer.h"

namespace Eagle
{
	namespace FileDialog
	{
		static const wchar_t* TEXTURE_FILTER = L"Texture (*.png,*.jpg,*.tga)\0*.png;*.jpg;*.tga\0";
		static const wchar_t* TEXTURE_CUBE_FILTER = L"Texture Cube (*.hdr)\0*.hdr\0";
		static const wchar_t* ASSET_FILTER = L"Eagle Asset (*.egasset)\0*.egasset\0";
		static const wchar_t* MESH_FILTER = L"3D-Model (*.fbx,*.blend,*.3ds,*.obj,*.smd,*.vta,*.stl)\0*.fbx;*.blend;*.3ds;*.obj;*.smd;*.vta;*.stl\0";
		static const wchar_t* SOUND_FILTER = L"Sound (*.wav,*.ogg,*.wma)\0*.wav;*.ogg;*.wma\0";
		static const wchar_t* FONT_FILTER = L"Font (*.ttf,*.otf)\0*.ttf;*.otf\0";
		static const wchar_t* IMPORT_FILTER = L"File (*.png,*.jpg,*.tga,*.hdr,*.fbx,*.blend,*.3ds,*.obj,*.smd,*.vta,*.stl,*.wav,*.ogg,*.wma,*.ttf,*.otf)\0*.png;*.jpg;*.tga;*.hdr;*.fbx;*.blend;*.3ds;*.obj;*.smd;*.vta;*.stl;*.wav;*.ogg;*.wma;*.ttf;*.otf\0";

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
		void OpenLink(const Path& path);

		bool WereScriptsRebuild();

		bool IsSSE2Supported();
	}

	namespace Dialog
	{
		bool YesNoQuestion(const std::string& title, const std::string& message);
	}
}