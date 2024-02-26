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

-- Include dirs
IncludeDir = {}
IncludeDir["GLFW"] = "Eagle/vendor/GLFW/include"
IncludeDir["ImGui"] = "Eagle/vendor/imgui"
IncludeDir["ImGuiNodeEditor"] = "Eagle/vendor/imgui-node-editor"
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
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["ThreadPool"] = "Eagle/vendor/thread-pool"
IncludeDir["MSDF"] = "Eagle/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["MSDFGen"] = "Eagle/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["MagicEnum"] = "Eagle/vendor/magic_enum/include"
IncludeDir["zstd"] = "Eagle/vendor/zstd/include"
IncludeDir["BasisUniversal"] = "Eagle/vendor/KTX-Software/basis_universal"
IncludeDir["KTX"] = "Eagle/vendor/KTX-Software/include"

-- Lib dirs
LibDir = {}
LibDir["assimp"] = "%{wks.location}/Eagle/vendor/assimp/lib"
LibDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibDir["PhysXDebug"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Debug/"
LibDir["PhysXRelease"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Release/"
LibDir["fmodDebug"] = "%{wks.location}/Eagle/vendor/fmod/lib/Debug"
LibDir["fmodRelease"] = "%{wks.location}/Eagle/vendor/fmod/lib/Release"
LibDir["zstdDebug"] = "%{wks.location}/Eagle/vendor/zstd/lib/Debug"
LibDir["zstdRelease"] = "%{wks.location}/Eagle/vendor/zstd/lib/Release"
LibDir["KTXDebug"] = "%{wks.location}/Eagle/vendor/KTX-Software/libs/Debug"
LibDir["KTXRelease"] = "%{wks.location}/Eagle/vendor/KTX-Software/libs/Release"

-- Lib files
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

LibFiles["Vulkan"] = "%{LibDir.VulkanSDK}/vulkan-1.lib"

LibFiles["fmodDebug"] = "%{LibDir.fmodDebug}/fmodL_vc.lib"
LibFiles["fmodRelease"] = "%{LibDir.fmodRelease}/fmod_vc.lib"

LibFiles["ShaderC_Debug"] = "%{LibDir.VulkanSDK}/shaderc_combinedd.lib"
LibFiles["SPIRV_Cross_Debug"] = "%{LibDir.VulkanSDK}/spirv-cross-cored.lib"

LibFiles["ShaderC_Release"] = "%{LibDir.VulkanSDK}/shaderc_combined.lib"
LibFiles["SPIRV_Cross_Release"] = "%{LibDir.VulkanSDK}/spirv-cross-core.lib"

LibFiles["zstdDebug"] = "%{LibDir.zstdDebug}/zstd_static.lib"
LibFiles["zstdRelease"] = "%{LibDir.zstdRelease}/zstd_static.lib"

LibFiles["KTXDebug"] = "%{LibDir.KTXDebug}/ktx.lib"
LibFiles["KTXReadDebug"] = "%{LibDir.KTXDebug}/ktx_read.lib"
LibFiles["KTXUtilsDebug"] = "%{LibDir.KTXDebug}/objUtil.lib"

LibFiles["KTXRelease"] = "%{LibDir.KTXRelease}/ktx.lib"
LibFiles["KTXReadRelease"] = "%{LibDir.KTXRelease}/ktx_read.lib"
LibFiles["KTXUtilsRelease"] = "%{LibDir.KTXRelease}/objUtil.lib"
--------------------------------------------------------------------------------------------------------------

group "Dependecies"
	include "Eagle/vendor/GLFW"
	include "Eagle/vendor/imgui"
	include "Eagle/vendor/imgui-node-editor"
	include "Eagle/vendor/yaml-cpp"
	include "Eagle/vendor/msdf-atlas-gen"
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

	flags { "FatalWarnings" }

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

        "%{prj.name}/vendor/imgui/imgui_impl_vulkan.h",
		"%{prj.name}/vendor/imgui/imgui_impl_vulkan.cpp",

		"%{prj.name}/vendor/VulkanMemoryAllocator/vk_mem_alloc.h",
		"%{prj.name}/vendor/VulkanMemoryAllocator/vk_mem_alloc.cpp"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{prj.name}/vendor/argparse/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuiNodeEditor}",
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
		"%{IncludeDir.ThreadPool}",
		"%{IncludeDir.MSDF}",
		"%{IncludeDir.MSDFGen}",
		"%{IncludeDir.MagicEnum}",
		"%{IncludeDir.zstd}",
		"%{IncludeDir.BasisUniversal}",
		"%{IncludeDir.KTX}"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
		"PX_PHYSX_STATIC_LIB",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"MSDF_ATLAS_PUBLIC=",
		"IMGUI_DEFINE_MATH_OPERATORS=",
		"_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING",
		"KHRONOS_STATIC",
	}

	libdirs
	{
		"%{LibDir.assimp}",
		"%{LibDir.VulkanSDK}"
	}

	links
	{
		"GLFW",
		"ImGui",
		"ImGuiNodeEditor",
		"yaml-cpp",
		"MSDF-Atlas",
		"assimp-vc143-mt.lib",
		"%{LibFiles.Vulkan}"
	}

	linkoptions
	{
		"/ignore:4099", -- Disable 'PDB was not found' warnings 
		"/ignore:4006" -- Disable 'already defined in ...; second definition ignored' warnings 
	}

	filter "files:Eagle/vendor/ImGuizmo/**.cpp"
		flags { "NoPCH"	}
	filter "files:Eagle/src/Eagle/Script/ScriptEngineRegistry.cpp"
		buildoptions { "/bigobj" }

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
			"%{LibFiles.zstdDebug}",

			"%{LibFiles.ShaderC_Debug}",
			"%{LibFiles.SPIRV_Cross_Debug}",

			"%{LibFiles.KTXDebug}",
			"%{LibFiles.KTXReadDebug}",
			"%{LibFiles.KTXUtilsDebug}"
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
			"%{LibFiles.zstdRelease}",

			"%{LibFiles.ShaderC_Release}",
			"%{LibFiles.SPIRV_Cross_Release}",

			"%{LibFiles.KTXRelease}",
			"%{LibFiles.KTXReadRelease}",
			"%{LibFiles.KTXUtilsRelease}"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"

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
			"%{LibFiles.zstdRelease}",

			"%{LibFiles.ShaderC_Release}",
			"%{LibFiles.SPIRV_Cross_Release}",

			"%{LibFiles.KTXRelease}",
			"%{LibFiles.KTXReadRelease}",
			"%{LibFiles.KTXUtilsRelease}"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"

project "Eagle-Editor"
	location "Eagle-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	flags { "FatalWarnings" }

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
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuiNodeEditor}",
		"%{IncludeDir.ThreadPool}",
		"%{IncludeDir.MagicEnum}"
	}

	links
	{
		"Eagle",
		"Eagle-Scripts"
	}

	defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"IMGUI_DEFINE_MATH_OPERATORS=",
		"KHRONOS_STATIC",
	}

	linkoptions
	{
		"/ignore:4099", -- Disable 'PDB was not found' warnings 
		"/ignore:4006", -- Disable 'already defined in ...; second definition ignored' warnings 
		"/ignore:4098" -- Disable `defaultlib 'libcmt.lib' conflicts with use of other libs`
	}

	filter "system:windows"
		files { '%{prj.name}/%{prj.name}.rc', '%{prj.name}/assets/textures/icon.ico' }
		vpaths { ['%{prj.name}/'] = { '*.rc', '**.ico' } }
		systemversion "latest"

	postbuildcommands 
	{
		'{COPY} "%{cfg.targetdir}/%{prj.name}.exe" "."',
	}

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "."',
			'{COPY} "../Eagle/vendor/fmod/lib/Debug/fmodL.dll" "."',
			'{COPY} "%{VULKAN_SDK}/Bin/shaderc_sharedd.dll" "."'
		}

	filter "configurations:Release"
		defines 
		{
			"EG_RELEASE",
			"NDEBUG"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "."',
			'{COPY} "../Eagle/vendor/fmod/lib/Release/fmod.dll" "."'
		}

	filter "configurations:Dist"
		defines 
		{
			"EG_DIST",
			"NDEBUG"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "."',
			'{COPY} "../Eagle/vendor/fmod/lib/Release/fmod.dll" "."'
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

project "Eagle-Game"
	location "Eagle-Game"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("Eagle-Editor")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	flags { "FatalWarnings" }

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
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuiNodeEditor}",
		"%{IncludeDir.ThreadPool}",
		"%{IncludeDir.MagicEnum}"
	}

	links
	{
		"Eagle",
		"Eagle-Scripts"
	}

	defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"IMGUI_DEFINE_MATH_OPERATORS=",
		"KHRONOS_STATIC",
	}

	linkoptions
	{
		"/ignore:4099", -- Disable 'PDB was not found' warnings 
		"/ignore:4006", -- Disable 'already defined in ...; second definition ignored' warnings 
		"/ignore:4098" -- Disable `defaultlib 'libcmt.lib' conflicts with use of other libs`
	}

	filter "system:windows"
		files { 'Eagle-Editor/Eagle-Editor.rc', 'Eagle-Editor/assets/textures/icon.ico' }
		vpaths { ['Eagle-Editor/'] = { '*.rc', '**.ico' } }
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
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"

	filter "configurations:Dist"
		defines 
		{
			"EG_DIST",
			"NDEBUG"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"
