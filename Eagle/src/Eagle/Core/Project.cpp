#include "egpch.h"
#include "Project.h"
#include "Application.h"

#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Renderer/VidWrappers/Shader.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/YamlUtils.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/SerializerUtils.h"
#include "Eagle/Physics/PhysicsEngine.h"
#include "Eagle/Core/AsyncTask.h"

#include <magic_enum_utility.hpp>
#include <glm/gtc/integer.hpp>

namespace Eagle
{
	constexpr uint32_t s_MaxCollisionGroups = sizeof(uint32_t) * 8;
	static bool s_bGameBuildInProgress = false; // Main thread only

	ProjectInfo Project::s_Info = {};

	static void OnCollisionGroupsChanged(const ProjectInfo& info)
	{
		AssetEntity::InvalidateCollisionGroups(info.AllCollisionGroupsMask);

		// Invalidate skeletal meshes (ragdolls)
		const auto& assets = AssetManager::GetAssets();
		for (const auto& [_, asset] : assets)
		{
			if (asset->GetAssetType() != AssetType::SkeletalMesh)
				continue;

			auto skeletalAsset = Cast<AssetSkeletalMesh>(asset);
			const auto& mesh = skeletalAsset->GetMesh();

			const uint32_t collisionGroup = uint32_t(mesh->GetCollisionGroup()) & info.AllCollisionGroupsMask;
			const uint32_t interactingCollisionGroup = uint32_t(mesh->GetInteractingCollisionGroup()) & info.AllCollisionGroupsMask;

			mesh->SetCollisionGroup(CollisionGroup(collisionGroup));
			mesh->SetInteractingCollisionGroup(CollisionGroup(interactingCollisionGroup));
			mesh->RegenerateRagdollData(mesh->GetMinRagdollBoneSize());
		}
	}

	static void LoadCollisionGroups(const YAML::Node& groupsNode, ProjectInfo& info)
	{
		info.UserCollisionGroups.clear();
		info.AllCollisionGroups.clear();
		info.AllCollisionGroupsMask = 0u;
		info.CollisionGroupGUIDs.clear();

		info.CollisionGroupGUIDs.reserve(s_MaxCollisionGroups);
		info.AllCollisionGroups.reserve(s_MaxCollisionGroups);
		magic_enum::enum_for_each<CollisionGroup>([&info](CollisionGroup group)
		{
			uint32_t mask = uint32_t(group);
			info.AllCollisionGroups.push_back(std::make_pair(Utils::GetEnumName(group), mask));
			const size_t index = info.CollisionGroupGUIDs.size() + 1;
			info.CollisionGroupGUIDs.emplace_back(index);

			EG_CORE_ASSERT((info.AllCollisionGroupsMask & mask) == 0);
			info.AllCollisionGroupsMask |= mask;
		});
		info.CollisionGroupGUIDs.resize(s_MaxCollisionGroups, GUID64(0));

		if (!groupsNode)
			return;

		info.UserCollisionGroups.reserve(groupsNode.size());
		for (const auto& groupNode : groupsNode)
		{
			CollisionGroupInfo group;
			group.first = groupNode["Name"].as<std::string>();
			group.second = groupNode["Mask"].as<uint32_t>();
			GUID64 guid = groupNode["GUID"].as<GUID64>();
			Project::AddUserCollisionGroup(group, guid);
		}
		OnCollisionGroupsChanged(info);
	}

	static GUID64 GetCollisionGroupGUID(const ProjectInfo& info, uint32_t mask)
	{
		if ((info.AllCollisionGroupsMask & mask) == 0u)
			return GUID64(0);

		const uint32_t index = glm::log2(mask);
		return index < info.CollisionGroupGUIDs.size() ? info.CollisionGroupGUIDs[index] : GUID64(0);
	}

	static void SaveCollisionGroups(YAML::Emitter& out, const ProjectInfo& info)
	{
		out << YAML::Key << "CollisionGroups" << YAML::Value;
		out << YAML::BeginSeq;
		for (const auto& groups : info.UserCollisionGroups)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << groups.first;
			out << YAML::Key << "Mask" << YAML::Value << groups.second;
			out << YAML::Key << "GUID" << YAML::Value << GetCollisionGroupGUID(info, groups.second);
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	namespace
	{
		// Everything a game build needs from the editor. It's gathered on the main thread when the build is requested,
		// so that the background thread doesn't read state that the editor might change while the build is running
		struct GameBuildInfo
		{
			Path OutputFolder;
			Path CorePath;
			Path GameExe;
			Path ProjectBinariesPath;
			ProjectInfo ProjectData; // Copy of the opened project info
			GUID StartupSceneGUID = GUID(0, 0);
			std::string RendererConfig; // Contents of `Config/RenderConfig.ini`
		};

		std::string FormatBytes(size_t bytes)
		{
			const char* units[] = { "B", "KB", "MB", "GB", "TB" };
			double value = double(bytes);
			size_t unit = 0;
			while (value >= 1024.0 && unit + 1 < std::size(units))
			{
				value /= 1024.0;
				++unit;
			}

			char buffer[32];
			snprintf(buffer, sizeof(buffer), unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
			return buffer;
		}

		bool CopyBuildFile(const Path& from, const Path& to, AsyncTaskContext& task)
		{
			task.SetDetail(Utils::AsString(to.filename()));

			std::error_code error;
			std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
			{
				EG_CORE_ERROR("Failed to copy `{}` to `{}`: {}", from, to, error.message());
				return false;
			}
			return true;
		}

		bool CopyBuildFolder(const Path& from, const Path& to, AsyncTaskContext& task)
		{
			namespace fs = std::filesystem;

			std::error_code error;
			std::vector<fs::directory_entry> entries;
			for (auto it = fs::recursive_directory_iterator(from, error); !error && it != fs::recursive_directory_iterator(); it.increment(error))
				entries.push_back(*it);

			if (error)
			{
				EG_CORE_ERROR("Failed to copy `{}` to `{}`: {}", from, to, error.message());
				return false;
			}

			fs::create_directories(to, error);
			for (size_t i = 0; i < entries.size(); ++i)
			{
				const auto& entry = entries[i];
				const Path destination = to / fs::relative(entry.path(), from, error);
				if (!error)
				{
					if (entry.is_directory(error))
					{
						fs::create_directories(destination, error);
					}
					else
					{
						task.SetDetail(Utils::AsString(entry.path().filename()));
						fs::create_directories(destination.parent_path(), error);
						if (!error)
							fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, error);
					}
				}

				if (error)
				{
					EG_CORE_ERROR("Failed to copy `{}` to `{}`: {}", entry.path(), destination, error.message());
					return false;
				}
				task.SetProgress(i + 1, entries.size());
			}
			return true;
		}

		// Executed on a background thread
		AsyncTaskResult BuildGame(const GameBuildInfo& info, AsyncTaskContext& task)
		{
			namespace fs = std::filesystem;

			const Path assetPackPath = info.OutputFolder / "Data" / (info.ProjectData.Name + AssetManager::GetAssetPackExtension());
			const Path shaderPackPath = info.OutputFolder / Project::GetShaderPackRelativePath();

			// Packs are written into temporary files first, and are moved into place at the very end.
			// So a cancelled or a failed build doesn't leave a half-written build behind
			const Path assetPackTempPath = Path(assetPackPath) += ".tmp";
			const Path shaderPackTempPath = Path(shaderPackPath) += ".tmp";

			auto removeTempFiles = [&]()
			{
				std::error_code error;
				fs::remove(assetPackTempPath, error);
				fs::remove(shaderPackTempPath, error);
			};
			auto onCancelled = [&]()
			{
				removeTempFiles();
				EG_CORE_WARN("The game build was cancelled");
				return AsyncTaskResult::Cancelled;
			};
			auto onFailed = [&](const std::string& reason)
			{
				removeTempFiles();
				EG_CORE_ERROR("Failed to build the game. {}", reason);
				return AsyncTaskResult::Failed;
			};

			// Progress bar is split between steps based on how long they usually take

			// 1. Shader pack
			ScopedDataBuffer shaderPack;
			{
				AsyncTaskProgressScope progress(0.00f, 0.05f);
				task.SetStatus("Packing shaders");
				task.SetDetail({});

				YAML::Emitter shaderPackOut;
				ShaderManager::BuildShaderPack(shaderPackOut);
				if (task.IsCancelRequested())
					return onCancelled();

				size_t totalSize = sizeof(AssetHeader);
				const std::string yamlStr = shaderPackOut.c_str();
				AssetHeader header = Utils::CreateHeader(yamlStr, &totalSize);
				ScopedDataBuffer build(totalSize);

				size_t offset = 0;
				Utils::WriteToBuffer(build, &header, sizeof(header), &offset);
				Utils::WriteStringToBuffer(build, yamlStr, &offset);

				shaderPack = Compressor::Compress(build);
				if (!shaderPack)
					return onFailed("Couldn't compress the shader pack");
			}

			// 2. Read all assets into one buffer
			ScopedDataBuffer build;
			{
				AsyncTaskProgressScope progress(0.05f, 0.35f);
				task.SetStatus("Packing assets");

				ScopedDataBuffer assetPack = AssetManager::BuildAssetPack(); // Reports progress itself
				if (task.IsCancelRequested())
					return onCancelled();
				if (!assetPack)
					return onFailed("Couldn't pack the assets");

				task.SetDetail({});

				size_t totalSize = sizeof(AssetHeader);
				const size_t assetPackOffset = Utils::AddSize(assetPack, &totalSize);

				YAML::Emitter out;
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << info.ProjectData.Name;
				out << YAML::Key << "Version" << YAML::Value << info.ProjectData.Version;

				if (!info.StartupSceneGUID.IsNull())
					out << YAML::Key << "StartupScene" << YAML::Value << info.StartupSceneGUID;

				out << YAML::Key << "AssetPackSize" << assetPack.Size();
				out << YAML::Key << "AssetPackOffset" << assetPackOffset;

				SaveCollisionGroups(out, info.ProjectData);
				out << YAML::EndMap;

				const std::string yamlStr = out.c_str();
				AssetHeader header = Utils::CreateHeader(yamlStr, &totalSize);
				build = ScopedDataBuffer(totalSize);

				size_t offset = 0;
				Utils::WriteToBuffer(build, &header, sizeof(header), &offset);
				Utils::WriteToBuffer(build, assetPack, &offset);
				Utils::WriteStringToBuffer(build, yamlStr, &offset);
			}

			// 3. Compress
			ScopedDataBuffer compressed;
			{
				AsyncTaskProgressScope progress(0.35f, 0.92f);
				task.SetStatus("Compressing assets");

				const std::string totalSizeStr = FormatBytes(build.Size());
				compressed = Compressor::Compress(build, [&task, &totalSizeStr](size_t processed, size_t total)
				{
					task.SetProgress(processed, total);
					task.SetDetail(FormatBytes(processed) + " / " + totalSizeStr);
					return !task.IsCancelRequested();
				});

				if (task.IsCancelRequested())
					return onCancelled();
				if (!compressed)
					return onFailed("Couldn't compress the asset pack");

				build.Release();
			}

			// 4. Write the packs into temporary files
			{
				AsyncTaskProgressScope progress(0.92f, 0.95f);
				task.SetStatus("Writing packs");

				task.SetDetail(Utils::AsString(assetPackPath.filename()));
				if (!FileSystem::Write(assetPackTempPath, compressed))
					return onFailed("Couldn't write " + Utils::AsString(assetPackTempPath));

				task.SetDetail(Utils::AsString(shaderPackPath.filename()));
				if (!FileSystem::Write(shaderPackTempPath, shaderPack))
					return onFailed("Couldn't write " + Utils::AsString(shaderPackTempPath));
			}

			if (task.IsCancelRequested())
				return onCancelled();

			// 5. Game packs are built and ready. Which means we're almost done,
			// so it doesn't make much sense to let it be canceled at this point
			task.SetCancelable(false);
			{
				AsyncTaskProgressScope progress(0.95f, 1.00f);
				task.SetStatus("Copying game files");

				const std::string projectScriptsFilename = info.ProjectData.Name + ".dll";
				bool bSuccess = true;
				{
					AsyncTaskProgressScope filesProgress(0.0f, 0.3f);
					bSuccess = bSuccess && CopyBuildFile(info.GameExe, info.OutputFolder / (info.ProjectData.Name + ".exe"), task);
					bSuccess = bSuccess && CopyBuildFile(info.CorePath / "Eagle-Scripts.dll", info.OutputFolder / "Eagle-Scripts.dll", task);
					bSuccess = bSuccess && CopyBuildFile(info.ProjectBinariesPath / projectScriptsFilename, info.OutputFolder / projectScriptsFilename, task);
					bSuccess = bSuccess && CopyBuildFile(info.CorePath / "assimp-vc143-mt.dll", info.OutputFolder / "assimp-vc143-mt.dll", task);
					bSuccess = bSuccess && CopyBuildFile(info.CorePath / "mono-2.0-sgen.dll", info.OutputFolder / "mono-2.0-sgen.dll", task);
#ifdef EG_DEBUG
					bSuccess = bSuccess && CopyBuildFile(info.CorePath / "fmodL.dll", info.OutputFolder / "fmodL.dll", task);
#else
					bSuccess = bSuccess && CopyBuildFile(info.CorePath / "fmod.dll", info.OutputFolder / "fmod.dll", task);
#endif
				}
				{
					AsyncTaskProgressScope monoProgress(0.3f, 0.9f);
					bSuccess = bSuccess && CopyBuildFolder(info.CorePath / "mono", info.OutputFolder / "mono", task);
				}
				if (!bSuccess)
					return onFailed("Couldn't copy the game files. See the errors above");

				// Renderer config
				{
					task.SetDetail("RenderConfig.ini");
					const Path configFolder = info.OutputFolder / "Config";
					std::error_code error;
					fs::create_directories(configFolder, error);

					std::ofstream fout(configFolder / "RenderConfig.ini");
					fout << info.RendererConfig;
					if (!fout)
						return onFailed("Couldn't write " + Utils::AsString(configFolder / "RenderConfig.ini"));
				}

				// Move the packs into place
				{
					task.SetDetail({});
					std::error_code error;
					fs::rename(assetPackTempPath, assetPackPath, error);
					if (error)
						return onFailed("Couldn't move the asset pack into place: " + error.message());

					fs::rename(shaderPackTempPath, shaderPackPath, error);
					if (error)
						return onFailed("Couldn't move the shader pack into place: " + error.message());
				}
			}

			EG_CORE_INFO("The game was built successfully: {}", info.OutputFolder);
			return AsyncTaskResult::Succeeded;
		}
	}
	
	bool Project::Create(const ProjectInfo& info)
	{
		namespace fs = std::filesystem;

		// TODO: needed?
		std::error_code error;
		if (!fs::exists(info.BasePath))
			fs::create_directory(info.BasePath, error);

		if (!fs::is_directory(info.BasePath))
		{
			EG_CORE_ERROR("Couldn't create project at: {}. It's not a folder!", info.BasePath);
			return false;
		}

		// Checking if folder already contains another eagle project
		for (auto& dirEntry : fs::directory_iterator(info.BasePath))
		{
			if (fs::is_directory(dirEntry))
				continue;

			const Path filepath = dirEntry;
			if (!filepath.has_extension())
				continue;
			
			if (Utils::HasExtension(filepath, GetExtension()))
			{
				EG_CORE_ERROR("Couldn't create project at: {}. This folder already contains another Eagle project!", info.BasePath);
				return false;
			}
		}

		const fs::path contentPath = info.BasePath / "Content";

		fs::create_directory(contentPath);
		fs::create_directory(info.BasePath / "Binaries");
		fs::create_directory(info.BasePath / "Source");
		fs::create_directory(info.BasePath / "Config");

		fs::copy(Application::GetCorePath() / "assets/meshes/Cube.egasset", contentPath / "Cube.egasset");
		fs::copy(Application::GetCorePath() / "assets/meshes/Sphere.egasset", contentPath / "Sphere.egasset");
		fs::copy(Application::GetCorePath() / "default_imgui_layout.ini", info.BasePath / "Config" / "imgui.ini");

		// Git ignore
		{
			std::ofstream fout(info.BasePath / ".gitignore");
			fout << "Binaries/*\n";
			fout << "Cache/*\n";
			fout << "Saved/*\n";
			fout << "Config/imgui.ini\n";
		}

		Save(info);

		GenerateSolution(info);

		EG_CORE_INFO("Created project at: {}", info.BasePath);

		return true;
	}
	
	bool Project::Open(const Path& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_ERROR("Couldn't open a project at: {}. The file doesn't exist!", filepath);
			return {};
		}

		if (!Utils::HasExtension(filepath, GetExtension()))
		{
			EG_CORE_ERROR("Couldn't open a project at: {}. It's not a project file!", filepath);
			return false;
		}

		if (!Load(filepath, &s_Info))
			return false;

		Application::OnProjectChanged(true);
		EG_CORE_INFO("Opened project at: {}", s_Info.BasePath);

		return true;
	}
	
	bool Project::Close()
	{
		if (!Project::IsOpened())
		{
			EG_CORE_ERROR("Failed to close the project. There's no an opened project!");
			return false;
		}

		if (!Application::Get().IsGame())
			Save();

		EG_CORE_INFO("Closed project at: {}", s_Info.BasePath);
		s_Info = {};
		Application::OnProjectChanged(false);

		return true;
	}
	
	void Project::GenerateSolution(const ProjectInfo& info)
	{
		const std::string vsVersions[] = { "vs2022", "vs2019" };

		const std::string eagleDir = Utils::AsString(std::filesystem::absolute(Application::GetCorePath().parent_path()));
		std::string args = std::string(" --file=" + eagleDir + "/premake5_project.lua ") + "--projectname=" + info.Name
			+ " --projectdir=" + Utils::AsString(info.BasePath) + " --eagledir=" + eagleDir;

		for (const auto& version : vsVersions)
		{
			const int result = Utils::Execute(Utils::AsPath(eagleDir) / "vendor/premake/premake5.exe", version + args);
			if (result == 0)
			{
				EG_CORE_INFO("Successfully generated {} solution files: {}", version, info.BasePath);
				break;
			}
			EG_CORE_ERROR("Failed to generate {} solution files: {}", version, info.BasePath);
		}
	}
	
	void Project::Build(const Path& outputFolder)
	{
		Ref<ImGuiLayer>& imguiLayer = Application::Get().GetImGuiLayer();

		const Path gameExeFile = Application::GetCorePath() / "Eagle-Game.exe";
		if (!std::filesystem::exists(gameExeFile))
		{
			imguiLayer->AddMessage("Failed to build the game. Game executable is missing. Please, build the `Eagle-Game` project!");
			return;
		}

		if (s_bGameBuildInProgress)
		{
			imguiLayer->AddMessage("The game is already being built. Please, wait for it to finish");
			return;
		}

		// Gather everything that's needed from the editor now. The build itself runs on a background thread
		auto info = MakeRef<GameBuildInfo>();
		info->OutputFolder = outputFolder;
		info->CorePath = Application::GetCorePath();
		info->GameExe = gameExeFile;
		info->ProjectBinariesPath = GetBinariesPath();
		info->ProjectData = s_Info;
		if (s_Info.GameStartupScene)
			info->StartupSceneGUID = s_Info.GameStartupScene->GetGUID();

		// Renderer config
		{
			const auto& currentScene = Scene::GetCurrentScene();
			const auto rendererOptions = currentScene ? currentScene->GetSceneRenderer()->GetOptions() : SceneRendererSettings{};

			YAML::Emitter outRenderer;
			const bool bVSync = Application::Get().GetWindow().IsVSync();
			outRenderer << YAML::BeginMap;
			outRenderer << YAML::Key << "VSync" << YAML::Value << bVSync;
			outRenderer << YAML::Key << "Fullscreen" << YAML::Value << true;
			Serializer::SerializeRendererSettings(outRenderer, rendererOptions);
			outRenderer << YAML::EndMap;
			info->RendererConfig = outRenderer.c_str();
		}

		s_bGameBuildInProgress = true;

		AsyncTaskDesc desc;
		desc.Name = "Building the game";
		desc.bCancelable = true;
		desc.bModal = true; // The editor shouldn't change assets while they're being packed

		AsyncTaskManager::Submit(desc,
			[info](AsyncTaskContext& task) { return BuildGame(*info, task); },
			[](AsyncTaskResult result)
			{
				s_bGameBuildInProgress = false;

				Ref<ImGuiLayer>& imguiLayer = Application::Get().GetImGuiLayer();
				switch (result)
				{
					case AsyncTaskResult::Succeeded:
						imguiLayer->AddMessage("The build finished successfully!");
						break;
					case AsyncTaskResult::Failed:
						imguiLayer->AddMessage("Failed to build the game. See logs for more details");
						break;
					case AsyncTaskResult::Cancelled:
						break; // No need for a popup, since the user cancelled it
				}
			});
	}
	
	void Project::OpenGameBuild(const Path& filepath)
	{
		const Path& assetPackPath = filepath;
		ScopedDataBuffer compressedData = FileSystem::Read(assetPackPath);
		if (!compressedData)
		{
			EG_CORE_CRITICAL("Failed to load the asset pack: {}", assetPackPath);
			exit(-1);
		}

		ScopedDataBuffer data = Compressor::Decompress(compressedData);

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		const size_t assetPackSize = baseNode["AssetPackSize"].as<size_t>();
		const size_t assetPackOffset = baseNode["AssetPackOffset"].as<size_t>();

		s_Info.Name = baseNode["Name"].as<std::string>();
		s_Info.Version = baseNode["Version"].as<glm::uvec3>();
		s_Info.BasePath = Application::GetCorePath();

		GUID startupSceneGUID = GUID(0, 0);
		if (auto startupSceneNode = baseNode["StartupScene"])
		{
			startupSceneGUID = startupSceneNode.as<GUID>();
		}

		LoadCollisionGroups(baseNode["CollisionGroups"], s_Info);

		DataBuffer assetPack(assetPackSize);
		Utils::ReadBinary(data.GetDataBuffer(), assetPackSize, assetPackOffset, &assetPack);
		Application::Get().CallNextFrame([assetPack, startupSceneGUID]() mutable
		{
			AssetManager::InitGame(assetPack);
			assetPack.Release();

			Ref<Asset> asset;
			if (AssetManager::Get(startupSceneGUID, &asset))
				s_Info.GameStartupScene = Cast<AssetScene>(asset);
		});
	}

	void Project::Save()
	{
		if (Project::IsOpened())
			Save(s_Info);
	}
	
	void Project::Save(const ProjectInfo& info)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "Engine Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Name" << YAML::Value << info.Name;
		out << YAML::Key << "Project Version" << YAML::Value << info.Version;
		if (info.GameStartupScene)
			out << YAML::Key << "Game Startup Scene" << YAML::Value << info.GameStartupScene->GetGUID();

		SaveCollisionGroups(out, info);

		out << YAML::EndMap;

		std::ofstream fout(info.BasePath / (info.Name + GetExtension()));
		fout << out.c_str();
		fout.close();
	}

	void Project::OnProjectOpenProcessed()
	{
		Load(GetProjectFilePath(), &s_Info); // Required to correctly load `StartupScene`. Previously, it was not loaded because AssetManager wasn't initialized
	}

	void Project::AddUserCollisionGroup(const CollisionGroupInfo& group, const GUID64& guid)
	{
		if ((s_Info.AllCollisionGroupsMask & s_CollisionGroupAny) == s_CollisionGroupAny)
		{
			constexpr uint32_t builtinGroupsCount = (uint32_t)magic_enum::enum_count<CollisionGroup>();
			constexpr uint32_t maxUserGroups = s_MaxCollisionGroups - builtinGroupsCount;

			EG_CORE_ERROR("Failed to add a new user collision group `{}`. The engine only supports {} groups, and {} of them are built-in. Which means the engine only supports {} user collision groups",
				group.first, s_MaxCollisionGroups, builtinGroupsCount, maxUserGroups);
			return;
		}

		const uint32_t& mask = group.second;
		if ((s_Info.AllCollisionGroupsMask & mask) == 0)
		{
			s_Info.AllCollisionGroupsMask |= mask;
			s_Info.AllCollisionGroups.push_back(group);
			s_Info.UserCollisionGroups.push_back(group);
			const uint32_t index = glm::log2(mask);
			s_Info.CollisionGroupGUIDs[index] = guid;
			OnCollisionGroupsChanged(s_Info);
		}
		else
		{
			EG_CORE_ERROR("Failed to add a new user collision group. Group with its mask already exists! Mask: {}", mask);
			EG_CORE_ASSERT(!"Mask collision");
		}
	}

	void Project::AddUserCollisionGroup(const std::string& name)
	{
		constexpr uint32_t builtinGroupsCount = (uint32_t)magic_enum::enum_count<CollisionGroup>();
		constexpr uint32_t maxUserGroups = s_MaxCollisionGroups - builtinGroupsCount;

		if ((s_Info.AllCollisionGroupsMask & s_CollisionGroupAny) == s_CollisionGroupAny)
		{
			EG_CORE_ERROR("Failed to add a new user collision group. The engine only supports {} groups, and {} of them are built-in. Which means the engine only supports {} user collision groups",
				s_MaxCollisionGroups, builtinGroupsCount, maxUserGroups);
			return;
		}

		// Find first unused bit
		const CollisionGroup lastBuiltin = magic_enum::enum_value<CollisionGroup>(builtinGroupsCount - 1); // Get by index
		uint32_t mask = (uint32_t)lastBuiltin << 1; // Skip all built in masks
		while (true)
		{
			if ((s_Info.AllCollisionGroupsMask & mask) == 0u)
			{
				break;
			}
			mask <<= 1;
		}

		CollisionGroupInfo newGroup;
		newGroup.first = name;
		newGroup.second = mask;
		AddUserCollisionGroup(newGroup, GUID64{});
	}

	void Project::RemoveUserCollisionGroup(uint32_t mask)
	{
		auto eraseGroupFunc = [](std::vector<CollisionGroupInfo>& groups, uint32_t mask)
		{
			auto it = std::find_if(groups.begin(), groups.end(), [mask](const CollisionGroupInfo& group)
			{
				return group.second == mask;
			});
			if (it != groups.end())
				groups.erase(it);
		};

		eraseGroupFunc(s_Info.AllCollisionGroups, mask);
		eraseGroupFunc(s_Info.UserCollisionGroups, mask);
		s_Info.AllCollisionGroupsMask &= (~mask);

		const uint32_t index = glm::log2(mask);
		s_Info.CollisionGroupGUIDs[index] = GUID64(0);
		OnCollisionGroupsChanged(s_Info);
	}

	void Project::RenameUserCollisionGroup(uint32_t mask, const std::string& newName)
	{
		auto renameGroupFunc = [](std::vector<CollisionGroupInfo>& groups, uint32_t mask, const std::string& newName)
		{
			auto it = std::find_if(groups.begin(), groups.end(), [mask](const CollisionGroupInfo& group)
			{
				return group.second == mask;
			});
			if (it != groups.end())
				it->first = newName;
		};

		renameGroupFunc(s_Info.AllCollisionGroups, mask, newName);
		renameGroupFunc(s_Info.UserCollisionGroups, mask, newName);
	}

	bool Project::CanAddUserCollisionGroup()
	{
		return s_Info.AllCollisionGroups.size() < s_MaxCollisionGroups;
	}

	GUID64 Project::GetCollisionGroupGUIDByMask(uint32_t mask)
	{
		return GetCollisionGroupGUID(s_Info, mask);
	}

	bool Project::Load(const Path& filepath, ProjectInfo* outInfo)
	{
		*outInfo = {};

		YAML::Node data = YAML::Load(FileSystem::ReadText(filepath));
		auto nameNode = data["Name"];

		if (!nameNode)
		{
			EG_CORE_ERROR("Failed to load a project. Invalid format: {}", filepath);
			return false;
		}

		(*outInfo).BasePath = filepath.parent_path();
		(*outInfo).Name = nameNode.as<std::string>();
		if (auto projectVersionNode = data["Project Version"])
			(*outInfo).Version = projectVersionNode.as<glm::uvec3>();
		if (auto sceneNode = data["Game Startup Scene"])
		{
			GUID guid = sceneNode.as<GUID>();
			Ref<Asset> asset;
			if (AssetManager::Get(guid, &asset))
				(*outInfo).GameStartupScene = Cast<AssetScene>(asset);
		}
		LoadCollisionGroups(data["CollisionGroups"], *outInfo);

		return true;
	}
}
