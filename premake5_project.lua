newoption {
   trigger = "projectname",
   description = "Eagle project name"
}

newoption {
   trigger = "projectdir",
   description = "Eagle project dir"
}

newoption {
   trigger = "eagledir",
   description = "Eagle Engine dir"
}

ProjectName = _OPTIONS["projectname"]
ProjectDir = _OPTIONS["projectdir"]
EagleDir = _OPTIONS["eagledir"]

workspace "%{ProjectName}"
	architecture "x64"
	targetdir "build"
	startproject "%{ProjectName}"
	location "%{ProjectDir}"
	
	configurations 
	{ 
		"Debug", 
		"Release",
		"Dist"
	}

	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Eagle"
externalproject "Eagle-Scripts"
	location ("%{EagleDir}" .. "/Eagle-Scripts")
	kind "SharedLib"
	language "C#"
group ""

project "%{ProjectName}"
	location "%{ProjectDir}"
	kind "SharedLib"
	language "C#"

	targetdir ("%{ProjectDir}" .. "/Binaries")
	objdir ("%{ProjectDir}" .. "/Binaries/Intermediate/" .. outputdir)
	targetname ("%{ProjectName}")

	files
	{
		"%{ProjectDir}/Content/**.cs"
	}

	links
	{
		"Eagle-Scripts"
	}

	postbuildcommands 
	{
		"%{EagleDir}" .. "/Eagle-Editor/BuildEvent.exe"
	}

	filter { "system:windows", "configurations:Debug" }
        runtime "Debug"
        optimize "Speed"

    filter { "system:windows", "configurations:Release" }
        runtime "Release"
        optimize "Speed"

    filter { "system:windows", "configurations:Dist" }
        runtime "Release"
        optimize "Speed"

-- Script below fixes some issues that occur while generating VS project files
require "vstudio"

local function platformsElement(cfg)
  _p(2,'<Platforms>x64</Platforms>')
end

premake.override(premake.vstudio.cs2005.elements, "projectProperties", function (oldfn, cfg)
  return table.join(oldfn(cfg), {
    platformsElement,
  })
end)
