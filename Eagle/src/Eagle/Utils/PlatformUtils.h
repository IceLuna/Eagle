#pragma once

#include <string>

namespace Eagle
{
	class FileDialog
	{
	public:
		//Returns empty string if failed
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
	};
}