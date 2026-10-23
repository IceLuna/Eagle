project "spdlog"
	location "spdlog"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"spdlog/src/*.cpp"
	}

	includedirs
	{
		"spdlog/include"
	}

	defines
	{
		"SPDLOG_COMPILED_LIB",
		"_CRT_SECURE_NO_WARNINGS"
	}

	buildoptions
	{
		"/utf-8"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:ReleaseWithDebug"
		runtime "Release"
		optimize "Speed"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "Speed"
