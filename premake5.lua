workspace "Eagle"
	architecture "x64"
	startproject "Eagle-Editor"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "Eagle/vendor/GLFW/include"
IncludeDir["Glad"] = "Eagle/vendor/Glad/include"
IncludeDir["ImGui"] = "Eagle/vendor/imgui"
IncludeDir["glm"] = "Eagle/vendor/glm"
IncludeDir["stb_image"] = "Eagle/vendor/stb_image"
IncludeDir["entt"] = "Eagle/vendor/entt/include"
IncludeDir["yaml_cpp"] = "Eagle/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "Eagle/vendor/ImGuizmo"
IncludeDir["assimp"] = "Eagle/vendor/assimp/include"
IncludeDir["mono"] = "Eagle/vendor/mono/include"
IncludeDir["PhysX"] = "Eagle/vendor/PhysX/include"

LibDir = {}
LibDir["assimp"] = "%{wks.location}/Eagle/vendor/assimp/lib"
LibDir["PhysX"] = "%{wks.location}/Eagle/vendor/PhysX/lib/%{cfg.buildcfg}/"

LibFiles = {}
LibFiles["mono"] = "%{wks.location}/Eagle/vendor/mono/lib/%{cfg.buildcfg}/mono-2.0-sgen.lib"
LibFiles["PhysX"] = "%{LibDir.PhysX}/PhysX_static_64.lib"
LibFiles["PhysXCharacterKinematic"] = "%{LibDir.PhysX}PhysXCharacterKinematic_static_64.lib"
LibFiles["PhysXCommon"] = "%{LibDir.PhysX}/PhysXCommon_static_64.lib"
LibFiles["PhysXCooking"] = "%{LibDir.PhysX}/PhysXCooking_static_64.lib"
LibFiles["PhysXExtensions"] = "%{LibDir.PhysX}/PhysXExtensions_static_64.lib"
LibFiles["PhysXFoundation"] = "%{LibDir.PhysX}/PhysXFoundation_static_64.lib"
LibFiles["PhysXPvdSDK"] = "%{LibDir.PhysX}/PhysXPvdSDK_static_64.lib"
LibFiles["PhysXVehicle"] = "%{LibDir.PhysX}/PhysXVehicle_static_64.lib"

group "Dependecies"
	include "Eagle/vendor/GLFW"
	include "Eagle/vendor/Glad"
	include "Eagle/vendor/imgui"
	include "Eagle/vendor/yaml-cpp"
group ""

project "Eagle"
	location "Eagle"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "egpch.h"
	pchsource "Eagle/src/egpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",

		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",

		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",

		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.h",
		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.cpp"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.PhysX}"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
		"PX_PHYSX_STATIC_LIB"
	}

	libdirs
	{
		"%{LibDir.assimp}",
		"%{LibDir.PhysX}"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"yaml-cpp",
		"opengl32.lib",
		"assimp-vc142-mt.lib",
		"%{LibFiles.mono}",
		"%{LibFiles.PhysX}",
		"%{LibFiles.PhysXCharacterKinematic}",
		"%{LibFiles.PhysXCommon}",
		"%{LibFiles.PhysXCooking}", 
		"%{LibFiles.PhysXExtensions}",
		"%{LibFiles.PhysXFoundation}",
		"%{LibFiles.PhysXPvdSDK}",
		"%{LibFiles.PhysXVehicle}" 
	}

	filter "files:Eagle/vendor/ImGuizmo/**.cpp"
		flags { "NoPCH"	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines 
		{
			"EG_RELEASE",
			"NDEBUG"
		}
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines 
		{
			"EG_DIST",
			"NDEBUG"
		}
		runtime "Release"
		optimize "on"

project "Eagle-Editor"
	location "Eagle-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	includedirs
	{
		"Eagle/vendor/spdlog/include",
		"Eagle/src",
		"Eagle/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGui}"
	}

	links
	{
		"Eagle"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"'
		}

	filter "configurations:Release"
		defines 
		{
			"EG_RELEASE",
			"NDEBUG"
		}
		runtime "Release"
		optimize "on"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"'
		}

	filter "configurations:Dist"
		defines 
		{
			"EG_DIST",
			"NDEBUG"
		}
		runtime "Release"
		optimize "on"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"'
		}

project "Eagle-Scripts"
	location "Eagle-Scripts"
	kind "SharedLib"
	language "C#"

	targetdir ("Eagle-Editor")
	--targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/Source/**.cs"
	}

workspace "Sandbox"
	architecture "x64"
	targetdir "build"
	startproject "Sandbox"
	
	configurations 
	{ 
		"Debug", 
		"Release",
		"Dist"
	}

group "Eagle"
project "Eagle-Scripts"
	location "Eagle-Scripts"
	kind "SharedLib"
	language "C#"

	targetdir ("Eagle-Editor")
	--targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/Source/**.cs"
	}

group ""
project "Sandbox"
	location "Sandbox"
	kind "SharedLib"
	language "C#"

	targetdir ("Eagle-Editor")
	--targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/Content/**.cs"
	}

	links
	{
		"Eagle-Scripts"
	}

	postbuildcommands 
	{
		"BuildEvent.exe"
	}
