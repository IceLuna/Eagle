workspace "Eagle"
	architecture "x64"
	startproject "Eagle-Editor"
	multiprocessorcompile ("On")

	configurations
	{
		"Debug",
		"ReleaseWithDebug",
		"Release"
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
IncludeDir["ImPlot"] = "Eagle/vendor/ImPlot"
IncludeDir["ImGuiNodeEditor"] = "Eagle/vendor/imgui-node-editor"
IncludeDir["glm"] = "Eagle/vendor/glm"
IncludeDir["stb_image"] = "Eagle/vendor/stb_image"
IncludeDir["entt"] = "Eagle/vendor/entt/include"
IncludeDir["yaml_cpp"] = "Eagle/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "Eagle/vendor/ImGuizmo/src"
IncludeDir["imoguizmo"] = "Eagle/vendor/imoguizmo"
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
IncludeDir["RecastNavigation"] = "Eagle/vendor/recastnavigation"
IncludeDir["compressonator"] = "Eagle/vendor/compressonator/include"
IncludeDir["FFX_SSSR"] = "Eagle/vendor/ffx_sssr/include"
IncludeDir["unordered_dense"] = "Eagle/vendor/unordered_dense"

-- Lib dirs
LibDir = {}
LibDir["assimp"] = "%{wks.location}/Eagle/vendor/assimp/lib"
LibDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibDir["PhysXDebug"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Debug/"
LibDir["PhysXReleaseWithDebug"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Release_PVD/"
LibDir["PhysXRelease"] = "%{wks.location}/Eagle/vendor/PhysX/lib/Release/"
LibDir["fmodDebug"] = "%{wks.location}/Eagle/vendor/fmod/lib/Debug"
LibDir["fmodRelease"] = "%{wks.location}/Eagle/vendor/fmod/lib/Release"
LibDir["zstdDebug"] = "%{wks.location}/Eagle/vendor/zstd/lib/Debug"
LibDir["zstdRelease"] = "%{wks.location}/Eagle/vendor/zstd/lib/Release"
LibDir["FFX_SSSR"] = "%{wks.location}/Eagle/vendor/ffx_sssr/lib"

-- Lib files
LibFiles = {}
LibFiles["monoDebug"] = "%{wks.location}/Eagle/vendor/mono/lib/Debug/mono-2.0-sgen.lib"
LibFiles["monoRelease"] = "%{wks.location}/Eagle/vendor/mono/lib/Release/mono-2.0-sgen.lib"

-- PhysX is linked by the applications, not by Eagle: a static library merges the libraries it links into itself,
-- so the editor and a game couldn't link different builds of it.
-- The editor links a build with PhysX Visual Debugger (PVD) support in every configuration, and so do the Debug and
-- ReleaseWithDebug builds of a game. A game's Release build links the one without it, for full performance.
-- Eagle decides whether to use PVD at runtime (see `Application`), so it's built once for both.
local function PhysXLibFiles(libDir)
	local result = {}
	for _, name in ipairs({ "PhysX", "PhysXCharacterKinematic", "PhysXCommon", "PhysXCooking", "PhysXExtensions", "PhysXFoundation", "PhysXPvdSDK", "PhysXVehicle2" }) do
		table.insert(result, libDir .. name .. "_static_64.lib")
	end
	return result
end

PhysXLibs = {}
PhysXLibs["Debug"]   = PhysXLibFiles(LibDir.PhysXDebug)
PhysXLibs["PVD"]     = PhysXLibFiles(LibDir.PhysXReleaseWithDebug) -- Release with PVD support
PhysXLibs["Release"] = PhysXLibFiles(LibDir.PhysXRelease)

LibFiles["Vulkan"] = "%{LibDir.VulkanSDK}/vulkan-1.lib"

LibFiles["fmodDebug"] = "%{LibDir.fmodDebug}/fmodL_vc.lib"
LibFiles["fmodRelease"] = "%{LibDir.fmodRelease}/fmod_vc.lib"

LibFiles["ShaderC_Debug"] = "%{LibDir.VulkanSDK}/shaderc_combinedd.lib"
LibFiles["SPIRV_Cross_Debug"] = "%{LibDir.VulkanSDK}/spirv-cross-cored.lib"

LibFiles["ShaderC_Release"] = "%{LibDir.VulkanSDK}/shaderc_combined.lib"
LibFiles["SPIRV_Cross_Release"] = "%{LibDir.VulkanSDK}/spirv-cross-core.lib"

LibFiles["zstdDebug"] = "%{LibDir.zstdDebug}/zstd_static.lib"
LibFiles["zstdRelease"] = "%{LibDir.zstdRelease}/zstd_static.lib"

LibFiles["compressonatorDebug"]   = "%{wks.location}/Eagle/vendor/compressonator/lib/Debug/Compressonator_MDd.lib"
LibFiles["compressonatorRelease"] = "%{wks.location}/Eagle/vendor/compressonator/lib/Release/Compressonator_MD.lib"

LibFiles["FFX_SSSR_Backend_VK_Debug"] = "%{LibDir.FFX_SSSR}/Debug/ffx_backend_vk_x64d.lib"
LibFiles["FFX_SSSR_Denoiser_Debug"] = "%{LibDir.FFX_SSSR}/Debug/ffx_denoiser_x64d.lib"
LibFiles["FFX_SSSR_Debug"] = "%{LibDir.FFX_SSSR}/Debug/ffx_sssr_x64d.lib"

LibFiles["FFX_SSSR_Backend_VK_Release"] = "%{LibDir.FFX_SSSR}/Release/ffx_backend_vk_x64.lib"
LibFiles["FFX_SSSR_Denoiser_Release"] = "%{LibDir.FFX_SSSR}/Release/ffx_denoiser_x64.lib"
LibFiles["FFX_SSSR_Release"] = "%{LibDir.FFX_SSSR}/Release/ffx_sssr_x64.lib"

--------------------------------------------------------------------------------------------------------------

group "Dependecies"
	include "Eagle/vendor/GLFW"
	include "Eagle/vendor/imgui"
	include "Eagle/vendor/imgui-node-editor"
	include "Eagle/vendor/ImPlot"
	include "Eagle/vendor/yaml-cpp"
	include "Eagle/vendor/msdf-atlas-gen"
	include "Eagle/vendor/spdlog.lua"
	group "Dependecies/RecastNavigation"
		include "Eagle/vendor/recastnavigation"
	group "Dependecies"
group ""

project "Eagle"
	location "Eagle"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "egpch.h"
	pchsource "Eagle/src/egpch.cpp"

	fatalwarnings("All")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",

		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",

		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",

		"%{prj.name}/vendor/ImGuizmo/src/ImGuizmo.h",
		"%{prj.name}/vendor/ImGuizmo/src/ImGuizmo.cpp",

		"%{prj.name}/vendor/imoguizmo/imoguizmo.hpp",

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
		"%{IncludeDir.ImPlot}",
		"%{IncludeDir.ImGuiNodeEditor}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.imoguizmo}",
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
		"%{IncludeDir.RecastNavigation}/Detour/Include",
		"%{IncludeDir.RecastNavigation}/DetourCrowd/Include",
		"%{IncludeDir.RecastNavigation}/DetourTileCache/Include",
		"%{IncludeDir.RecastNavigation}/Recast/Include",
		"%{IncludeDir.RecastNavigation}/DebugUtils/Include",
		"%{IncludeDir.compressonator}",
		"%{IncludeDir.FFX_SSSR}",
		"%{IncludeDir.unordered_dense}",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
		"PX_PHYSX_STATIC_LIB",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"MSDF_ATLAS_PUBLIC=",
		"IMGUI_DEFINE_MATH_OPERATORS",
		"_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING",
		"KHRONOS_STATIC",
		"_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING",
		"SPDLOG_COMPILED_LIB",
	}

	libdirs
	{
		"%{LibDir.assimp}",
		"%{LibDir.VulkanSDK}",
	}

	links
	{
		"spdlog",
		"GLFW",
		"ImGui",
		"ImPlot",
		"ImGuiNodeEditor",
		"yaml-cpp",
		"MSDF-Atlas",
		"assimp-vc143-mt.lib",
		"%{LibFiles.Vulkan}",

		-- Recast navigation
		"DebugUtils",
		"Detour",
		"DetourCrowd",
		"DetourTileCache",
		"Recast",
	}

	linkoptions
	{
		"/ignore:4099", -- Disable 'PDB was not found' warnings 
		"/ignore:4006" -- Disable 'already defined in ...; second definition ignored' warnings 
	}

	buildoptions
	{
		"/utf-8"
	}

	filter "files:Eagle/vendor/ImGuizmo/src/**.cpp"
		enablepch "Off"

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"
		libdirs
		{
			"%{LibDir.fmodDebug}"
		}
		links
		{
			"%{LibFiles.fmodDebug}",
			"%{LibFiles.monoDebug}",
			"%{LibFiles.zstdDebug}",
			"%{LibFiles.compressonatorDebug}",

			"%{LibFiles.ShaderC_Debug}",
			"%{LibFiles.SPIRV_Cross_Debug}",

			"%{LibFiles.FFX_SSSR_Backend_VK_Debug}",
			"%{LibFiles.FFX_SSSR_Denoiser_Debug}",
			"%{LibFiles.FFX_SSSR_Debug}",
		}

	filter "configurations:ReleaseWithDebug"
		defines 
		{
			"EG_RELEASE_WITH_DEBUG",
			"NDEBUG"
		}
		libdirs
		{
			"%{LibDir.fmodRelease}"
		}
		links
		{
			"%{LibFiles.fmodRelease}",
			"%{LibFiles.monoRelease}",
			"%{LibFiles.zstdRelease}",
			"%{LibFiles.compressonatorRelease}",

			"%{LibFiles.ShaderC_Release}",
			"%{LibFiles.SPIRV_Cross_Release}",

			"%{LibFiles.FFX_SSSR_Backend_VK_Release}",
			"%{LibFiles.FFX_SSSR_Denoiser_Release}",
			"%{LibFiles.FFX_SSSR_Release}",
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"
		symbols "on"

	filter "configurations:Release"
		defines 
		{
			"EG_RELEASE",
			"NDEBUG"
		}
		libdirs
		{
			"%{LibDir.fmodRelease}"
		}
		links
		{
			"%{LibFiles.fmodRelease}",
			"%{LibFiles.monoRelease}",
			"%{LibFiles.zstdRelease}",
			"%{LibFiles.compressonatorRelease}",

			"%{LibFiles.ShaderC_Release}",
			"%{LibFiles.SPIRV_Cross_Release}",

			"%{LibFiles.FFX_SSSR_Backend_VK_Release}",
			"%{LibFiles.FFX_SSSR_Denoiser_Release}",
			"%{LibFiles.FFX_SSSR_Release}",
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
	cppdialect "C++23"
	staticruntime "off"

	pchheader "edpch.h"
	pchsource "Eagle-Editor/src/edpch.cpp"
	forceincludes { "edpch.h" }

	targetdir ("%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	fatalwarnings("All")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/assets/**.**"
	}

	includedirs
	{
		"%{prj.name}/src", -- so the force-included PCH header is found from every subfolder
		"Eagle/vendor/spdlog/include",
		"Eagle/src",
		"Eagle/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.imoguizmo}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImPlot}",
		"%{IncludeDir.ImGuiNodeEditor}",
		"%{IncludeDir.ThreadPool}",
		"%{IncludeDir.MagicEnum}",
		"%{IncludeDir.RecastNavigation}/Detour/Include",
		"%{IncludeDir.RecastNavigation}/DetourCrowd/Include",
		"%{IncludeDir.RecastNavigation}/DetourTileCache/Include",
		"%{IncludeDir.RecastNavigation}/Recast/Include",
		"%{IncludeDir.RecastNavigation}/Recast/DebugUtils",
		"%{IncludeDir.RecastNavigation}/DebugUtils/Include",
		"%{IncludeDir.unordered_dense}",
	}

	links
	{
		"Eagle",
		"Eagle-Scripts"
	}

	defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"IMGUI_DEFINE_MATH_OPERATORS",
		"KHRONOS_STATIC",
		"_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING",
		"SPDLOG_COMPILED_LIB",
	}

	linkoptions
	{
		"/ignore:4099", -- Disable 'PDB was not found' warnings 
		"/ignore:4006", -- Disable 'already defined in ...; second definition ignored' warnings 
		"/ignore:4098" -- Disable `defaultlib 'libcmt.lib' conflicts with use of other libs`
	}

	buildoptions
	{
		"/utf-8"
	}

	filter "system:windows"
		files { '%{prj.name}/%{prj.name}.rc', '%{prj.name}/assets/textures/icon.ico' }
		vpaths { ['%{prj.name}/'] = { '*.rc', '**.ico' } }
		systemversion "latest"

	postbuildcommands 
	{
		'{COPY} "%{cfg.targetdir}/%{prj.name}.exe" "."',
		'{COPY} "%{LibDir.assimp}/assimp-vc143-mt.dll" "."',
	}

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"
		links { PhysXLibs.Debug }

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "."',
			'{COPY} "../Eagle/vendor/fmod/lib/Debug/fmodL.dll" "."',
			'{COPY} "%{VULKAN_SDK}/Bin/shaderc_sharedd.dll" "."'
		}

	filter "configurations:ReleaseWithDebug"
		defines 
		{
			"EG_RELEASE_WITH_DEBUG",
			"NDEBUG"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"
		symbols "on"
		links { PhysXLibs.PVD }

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "."',
			'{COPY} "../Eagle/vendor/fmod/lib/Release/fmod.dll" "."'
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
		kind "WindowedApp"
		links { PhysXLibs.PVD } -- The editor keeps PVD in Release to allow users to debug physics

		postbuildcommands 
		{
			'{COPY} "../Eagle/vendor/mono/bin/Release/mono-2.0-sgen.dll" "."',
			'{COPY} "../Eagle/vendor/fmod/lib/Release/fmod.dll" "."'
		}

project "Eagle-Scripts"
	location "Eagle-Scripts"
	kind "SharedLib"
	language "C#"
	csversion "12"

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
	cppdialect "C++23"
	staticruntime "off"
	
	pchheader "gamepch.h"
	pchsource "Eagle-Game/src/gamepch.cpp"
	forceincludes { "gamepch.h" }

	targetdir ("Eagle-Editor")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	fatalwarnings("All")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/assets/**.**"
	}

	includedirs
	{
		"%{prj.name}/src", -- so the force-included PCH header is found from every subfolder
		"Eagle/vendor/spdlog/include",
		"Eagle/src",
		"Eagle/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.imoguizmo}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImPlot}",
		"%{IncludeDir.ImGuiNodeEditor}",
		"%{IncludeDir.ThreadPool}",
		"%{IncludeDir.MagicEnum}",
		"%{IncludeDir.RecastNavigation}/Detour/Include",
		"%{IncludeDir.RecastNavigation}/DetourCrowd/Include",
		"%{IncludeDir.RecastNavigation}/DetourTileCache/Include",
		"%{IncludeDir.RecastNavigation}/Recast/Include",
		"%{IncludeDir.RecastNavigation}/Recast/DebugUtils",
		"%{IncludeDir.RecastNavigation}/DebugUtils/Include",
		"%{IncludeDir.unordered_dense}",
	}

	links
	{
		"Eagle",
		"Eagle-Scripts"
	}

	defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"IMGUI_DEFINE_MATH_OPERATORS",
		"KHRONOS_STATIC",
		"_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING",
		"SPDLOG_COMPILED_LIB",
	}

	linkoptions
	{
		"/ignore:4099", -- Disable 'PDB was not found' warnings 
		"/ignore:4006", -- Disable 'already defined in ...; second definition ignored' warnings 
		"/ignore:4098" -- Disable `defaultlib 'libcmt.lib' conflicts with use of other libs`
	}

	buildoptions
	{
		"/utf-8"
	}

	filter "system:windows"
		files { 'Eagle-Editor/Eagle-Editor.rc', 'Eagle-Editor/assets/textures/icon.ico' }
		vpaths { ['Eagle-Editor/'] = { '*.rc', '**.ico' } }
		systemversion "latest"

	filter "configurations:Debug"
		defines "EG_DEBUG"
		runtime "Debug"
		symbols "on"
		links { PhysXLibs.Debug }

	filter "configurations:ReleaseWithDebug"
		defines 
		{
			"EG_RELEASE_WITH_DEBUG",
			"NDEBUG"
		}
		buildoptions
		{
			"/Ob2"
		}
		runtime "Release"
		optimize "Speed"
		symbols "on"
		links { PhysXLibs.PVD }

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
		kind "WindowedApp"
		links { PhysXLibs.Release } -- Without PVD support, for full performance
