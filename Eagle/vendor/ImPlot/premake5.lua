project "ImPlot"
    kind "Staticlib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    targetdir("bin/" .. outputdir .. "/%{prj.name}")
    objdir("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "implot.h",
        "implot_internal.h",
        "implot.cpp",
        "implot_items.cpp",
        "implot_demo.cpp"
    }

    includedirs
	{
		"../imgui",
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
