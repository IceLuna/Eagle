#include "egpch.h"
#include "Utils.h"

#include <locale>

namespace Eagle::Utils
{
	// templated version of my_equal so it could work with both char and wchar_t
	template<typename charT>
	struct my_equal {
		my_equal(const std::locale& loc) : loc_(loc) {}
		bool operator()(charT ch1, charT ch2) {
			return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
		}
	private:
		const std::locale& loc_;
	};

	// find substring (case insensitive)
	template<typename T>
	static size_t MyFindStrTemplate(const T& str1, const T& str2, const std::locale& loc = std::locale("RU_ru"))
	{
		typename T::const_iterator it = std::search(str1.begin(), str1.end(),
			str2.begin(), str2.end(), my_equal<typename T::value_type>(loc));
		if (it != str1.end()) return it - str1.begin();
		else return std::string::npos; // not found
	}

	std::string ToUtf8(const std::wstring& str)
	{
		std::string ret;
		int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
		if (len > 0)
		{
			ret.resize(len);
			WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), &ret[0], len, NULL, NULL);
		}
		return ret;
	}
	
	size_t FindSubstringI(const std::string& str1, const std::string& str2)
	{
		return MyFindStrTemplate(str1, str2);
	}
	
	size_t FindSubstringI(const std::wstring& str1, const std::wstring& str2)
	{
		return MyFindStrTemplate(str1, str2);
	}
}
