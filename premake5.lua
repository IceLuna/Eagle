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

VULKAN_SDK = os.getenv("VULKAN_SDK")
if VULKAN_SDK == nil then
	io.write("ERROR: Vulkan SDK is not installed. Min required version is 1.3\n")
	os.exit()
end

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
IncludeDir["fmod"] = "Eagle/vendor/fmod/inc"
IncludeDir["VulkanMemAlloc"] = "Eagle/vendor/VulkanMemoryAllocator"
IncludeDir["NvidiaAftermath"] = "Eagle/vendor/NvidiaAftermath/include"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

LibDir = {}
LibDir["assimp"] = "%{wks.location}/Eagle/vendor/assimp/lib"
LibDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibDir["PhysXDebug"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Debug/"
LibDir["PhysXRelease"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Release/"
LibDir["fmodDebug"] = "%{wks.location}/Eagle/vendor/fmod/lib/Debug"
LibDir["fmodRelease"] = "%{wks.location}/Eagle/vendor/fmod/lib/Release"
LibDir["NvidiaAftermath"] = "%{wks.location}/Eagle/vendor/NvidiaAftermath/lib/x64"


LibFiles = {}
LibFiles["monoDebug"] = "%{wks.location}/Eagle/vendor/mono/lib/Debug/mono-2.0-sgen.lib"
LibFiles["monoRelease"] = "%{wks.location}/Eagle/vendor/mono/lib/Release/mono-2.0-sgen.lib"

LibFiles["PhysXDebug"] = "%{LibDir.PhysXDebug}/PhysX_static_64.lib"
LibFiles["PhysXCharacterKinematicDebug"] = "%{LibDir.PhysXDebug}PhysXCharacterKinematic_static_64.lib"
LibFiles["PhysXCommonDebug"] = "%{LibDir.PhysXDebug}/PhysXCommon_static_64.lib"
LibFiles["PhysXCookingDebug"] = "%{LibDir.PhysXDebug}/PhysXCooking_static_64.lib"
LibFiles["PhysXExtensionsDebug"] = "%{LibDir.PhysXDebug}/PhysXExtensions_static_64.lib"
LibFiles["PhysXFoundationDebug"] = "%{LibDir.PhysXDebug}/PhysXFoundation_static_64.lib"
LibFiles["PhysXPvdSDKDebug"] = "%{LibDir.PhysXDebug}/PhysXPvdSDK_static_64.lib"
LibFiles["PhysXVehicleDebug"] = "%{LibDir.PhysXDebug}/PhysXVehicle_static_64.lib"

LibFiles["PhysXRelease"] = "%{LibDir.PhysXRelease}/PhysX_static_64.lib"
LibFiles["PhysXCharacterKinematicRelease"] = "%{LibDir.PhysXRelease}PhysXCharacterKinematic_static_64.lib"
LibFiles["PhysXCommonRelease"] = "%{LibDir.PhysXRelease}/PhysXCommon_static_64.lib"
LibFiles["PhysXCookingRelease"] = "%{LibDir.PhysXRelease}/PhysXCooking_static_64.lib"
LibFiles["PhysXExtensionsRelease"] = "%{LibDir.PhysXRelease}/PhysXExtensions_static_64.lib"
LibFiles["PhysXFoundationRelease"] = "%{LibDir.PhysXRelease}/PhysXFoundation_static_64.lib"
LibFiles["PhysXPvdSDKRelease"] = "%{LibDir.PhysXRelease}/PhysXPvdSDK_static_64.lib"
LibFiles["PhysXVehicleRelease"] = "%{LibDir.PhysXRelease}/PhysXVehicle_static_64.lib"

LibFiles["NvidiaAftermath"] = "%{LibDir.NvidiaAftermath}/GFSDK_Aftermath_Lib.x64.lib"
LibFiles["Vulkan"] = "%{LibDir.VulkanSDK}/vulkan-1.lib"
LibFiles["VulkanUtils"] = "%{LibDir.VulkanSDK}/VkLayer_utils.lib"

LibFiles["fmodDebug"] = "%{LibDir.fmodDebug}/fmodL_vc.lib"
LibFiles["fmodRelease"] = "%{LibDir.fmodRelease}/fmod_vc.lib"

LibFiles["ShaderC_Debug"] = "%{LibDir.VulkanSDK}/shaderc_sharedd.lib"
LibFiles["ShaderC_Utils_Debug"] = "%{LibDir.VulkanSDK}/shaderc_utild.lib"
LibFiles["SPIRV_Cross_Debug"] = "%{LibDir.VulkanSDK}/spirv-cross-cored.lib"
LibFiles["SPIRV_Cross_GLSL_Debug"] = "%{LibDir.VulkanSDK}/spirv-cross-glsld.lib"
LibFiles["SPIRV_Tools_Debug"] = "%{LibDir.VulkanSDK}/SPIRV-Toolsd.lib"

LibFiles["ShaderC_Release"] = "%{LibDir.VulkanSDK}/shaderc_shared.lib"
LibFiles["ShaderC_Utils_Release"] = "%{LibDir.VulkanSDK}/shaderc_util.lib"
LibFiles["SPIRV_Cross_Release"] = "%{LibDir.VulkanSDK}/spirv-cross-core.lib"
LibFiles["SPIRV_Cross_GLSL_Release"] = "%{LibDir.VulkanSDK}/spirv-cross-glsl.lib"

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
	staticruntime "off"

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
		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.cpp",

        "%{prj.name}/vendor/imgui/imgui_impl_vulkan_with_textures.h",
		"%{prj.name}/vendor/imgui/imgui_impl_vulkan_with_textures.cpp",

		"%{prj.name}/vendor/VulkanMemoryAllocator/vk_mem_alloc.h",
		"%{prj.name}/vendor/VulkanMemoryAllocator/vk_mem_alloc.cpp"
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
		"%{IncludeDir.PhysX}",
		"%{IncludeDir.fmod}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.VulkanMemAlloc}",
		"%{IncludeDir.NvidiaAftermath}"
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
		"%{LibDir.VulkanSDK}",
		"%{LibDir.NvidiaAftermath}"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"yaml-cpp",
		"assimp-vc143-mt.lib",
		"%{LibFiles.NvidiaAftermath}",
		"%{LibFiles.Vulkan}",
		"%{LibFiles.VulkanUtils}"
	}

	filter "files:Eagle/vendor/ImGuizmo/**.cpp"
		flags { "NoPCH"	}
	filter "files:Eagle/src/Platform/Vulkan/Debug/**.cpp"
		flags { "NoPCH"	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"
		libdirs
		{
			"%{LibDir.PhysXDebug}",
			"%{LibDir.fmodDebug}"
		}
		links
		{
			"%{LibFiles.PhysXDebug}",
			"%{LibFiles.PhysXCharacterKinematicDebug}",
			"%{LibFiles.PhysXCommonDebug}",
			"%{LibFiles.PhysXCookingDebug}", 
			"%{LibFiles.PhysXExtensionsDebug}",
			"%{LibFiles.PhysXFoundationDebug}",
			"%{LibFiles.PhysXPvdSDKDebug}",
			"%{LibFiles.PhysXVehicleDebug}",
			"%{LibFiles.fmodDebug}",
			"%{LibFiles.monoDebug}",

			"%{LibFiles.ShaderC_Debug}",
			"%{LibFiles.ShaderC_Utils_Debug}",
			"%{LibFiles.SPIRV_Cross_Debug}",
			"%{LibFiles.SPIRV_Cross_GLSL_Debug}",
			"%{LibFiles.SPIRV_Tools_Debug}"
		}

	filter "configurations:Release"
		defines 
		{
			"EG_RELEASE",
			"NDEBUG"
		}
		libdirs
		{
			"%{LibDir.PhysXRelease}",
			"%{LibDir.fmodRelease}"
		}
		links
		{
			"%{LibFiles.PhysXRelease}",
			"%{LibFiles.PhysXCharacterKinematicRelease}",
			"%{LibFiles.PhysXCommonRelease}",
			"%{LibFiles.PhysXCookingRelease}", 
			"%{LibFiles.PhysXExtensionsRelease}",
			"%{LibFiles.PhysXFoundationRelease}",
			"%{LibFiles.PhysXPvdSDKRelease}",
			"%{LibFiles.PhysXVehicleRelease}",
			"%{LibFiles.fmodRelease}",
			"%{LibFiles.monoRelease}",

			"%{LibFiles.ShaderC_Release}",
			"%{LibFiles.ShaderC_Utils_Release}",
			"%{LibFiles.SPIRV_Cross_Release}",
			"%{LibFiles.SPIRV_Cross_GLSL_Release}"
		}
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines 
		{
			"EG_DIST",
			"NDEBUG"
		}
		libdirs
		{
			"%{LibDir.PhysXRelease}",
			"%{LibDir.fmodRelease}"
		}
		links
		{
			"%{LibFiles.PhysXRelease}",
			"%{LibFiles.PhysXCharacterKinematicRelease}",
			"%{LibFiles.PhysXCommonRelease}",
			"%{LibFiles.PhysXCookingRelease}", 
			"%{LibFiles.PhysXExtensionsRelease}",
			"%{LibFiles.PhysXFoundationRelease}",
			"%{LibFiles.PhysXPvdSDKRelease}",
			"%{LibFiles.PhysXVehicleRelease}",
			"%{LibFiles.fmodRelease}",
			"%{LibFiles.monoRelease}",

			"%{LibFiles.ShaderC_Release}",
			"%{LibFiles.ShaderC_Utils_Release}",
			"%{LibFiles.SPIRV_Cross_Release}",
			"%{LibFiles.SPIRV_Cross_GLSL_Release}"
		}
		runtime "Release"
		optimize "on"

project "Eagle-Editor"
	location "Eagle-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/assets/**.**"
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
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.ImGui}"
	}

	links
	{
		"Eagle"
	}

	postbuildcommands 
	{
		'{COPY} "../Eagle/vendor/NvidiaAftermath/lib/x64/GFSDK_Aftermath_Lib.x64.dll" "%{cfg.targetdir}"'
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Eagle/vendor/fmod/lib/Debug/fmodL.dll" "%{cfg.targetdir}"',
			'{COPY} "%{VULKAN_SDK}/Bin/shaderc_sharedd.dll" "%{cfg.targetdir}"'
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
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Eagle/vendor/fmod/lib/Release/fmod.dll" "%{cfg.targetdir}"'
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
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Eagle/vendor/fmod/lib/Release/fmod.dll" "%{cfg.targetdir}"'
		}

project "Eagle-Scripts"
	location "Eagle-Scripts"
	kind "SharedLib"
	language "C#"

	targetdir ("Eagle-Editor")
	--targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	links
	{
		"Eagle"
	}

	files
	{
		"%{prj.name}/Source/**.cs"
	}

workspace "Sandbox"
	architecture "x64"
	targetdir "build"
	startproject "Sandbox"
	location "Sandbox"
	
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
