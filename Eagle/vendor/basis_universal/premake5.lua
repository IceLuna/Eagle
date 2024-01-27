project "BasisUniversal"
    kind "Staticlib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.c",
        "**.hpp",
        "**.cpp"
    }

    includedirs
	{
		"%{prj.name}"
	}

    filter "system:windows"
        systemversion "latest"

    filter { "system:windows", "configurations:Debug" }
        runtime "Debug"

    filter { "system:windows", "configurations:Release" }
        runtime "Release"
        optimize "Speed"

    filter { "system:windows", "configurations:Dist" }
        runtime "Release"
        optimize "Speed"
