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

#include <magic_enum_utility.hpp>
#include <glm/gtc/integer.hpp>

namespace Eagle
{
	constexpr uint32_t s_MaxCollisionGroups = sizeof(uint32_t) * 8;

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

	static void LoadCollisionGroups(YAML::Node& groupsNode, ProjectInfo& info)
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

	static void SaveCollisionGroups(YAML::Emitter& out, const ProjectInfo& info)
	{
		out << YAML::Key << "CollisionGroups" << YAML::Value;
		out << YAML::BeginSeq;
		for (const auto& groups : info.UserCollisionGroups)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << groups.first;
			out << YAML::Key << "Mask" << YAML::Value << groups.second;
			out << YAML::Key << "GUID" << YAML::Value << Project::GetCollisionGroupGUIDByMask(groups.second);
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	ProjectInfo Project::s_Info = {};
	
	bool Project::Create(const ProjectInfo& info)
	{
		namespace fs = std::filesystem;

		// TODO: needed?
		std::error_code error;
		if (!fs::exists(info.BasePath))
			fs::create_directory(info.BasePath, error);

		if (!fs::is_directory(info.BasePath))
		{
			EG_CORE_ERROR("Couldn't create project at: {}. It's not a folder!", info.BasePath.u8string());
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
				EG_CORE_ERROR("Couldn't create project at: {}. This folder already contains another Eagle project!", info.BasePath.u8string());
				return false;
			}
		}

		const fs::path contentPath = info.BasePath / "Content";

		fs::create_directory(contentPath);
		fs::create_directory(info.BasePath / "Binaries");
		fs::create_directory(info.BasePath / "Source");

		fs::copy(Application::GetCorePath() / "assets/meshes/Cube.egasset", contentPath / "Cube.egasset");
		fs::copy(Application::GetCorePath() / "assets/meshes/Sphere.egasset", contentPath / "Sphere.egasset");

		// Git ignore
		{
			std::ofstream fout(info.BasePath / ".gitignore");
			fout << "Binaries/*\n";
			fout << "Cache/*\n";
			fout << "Saved/*\n";
		}

		Save(info);

		GenerateSolution(info);

		EG_CORE_INFO("Created project at: {}", info.BasePath.u8string());

		return true;
	}
	
	bool Project::Open(const Path& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_ERROR("Couldn't open a project at: {}. The file doesn't exist!", filepath.u8string());
			return {};
		}

		if (!Utils::HasExtension(filepath, GetExtension()))
		{
			EG_CORE_ERROR("Couldn't open a project at: {}. It's not a project file!", filepath.u8string());
			return false;
		}

		if (!Load(filepath, &s_Info))
			return false;

		Application::OnProjectChanged(true);
		EG_CORE_INFO("Opened project at: {}", s_Info.BasePath.u8string());

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

		EG_CORE_INFO("Closed project at: {}", s_Info.BasePath.u8string());
		s_Info = {};
		Application::OnProjectChanged(false);

		return true;
	}
	
	void Project::GenerateSolution(const ProjectInfo& info)
	{
		const std::string vs2019 = "vs2019";
		const std::string vs2022 = "vs2022";
		const std::string eagleDir = Application::GetCorePath().parent_path().u8string();
		std::string args = std::string(" --file=" + eagleDir + "/premake5_project.lua ") + "--projectname=" + info.Name
			+ " --projectdir=" + info.BasePath.u8string() + " --eagledir=" + eagleDir;
		
		const int result = Utils::Execute(eagleDir + "/vendor/premake/premake5.exe", vs2022 + args);
		if (result == 0)
			EG_CORE_INFO("Successfully generated VS 2022 solution files: {}", info.BasePath.u8string());
		else
		{
			EG_CORE_ERROR("Failed to generate VS 2022 solution files: {}", info.BasePath.u8string());
			EG_CORE_INFO("Trying with VS 2019...");
			const int result = Utils::Execute("..\\vendor\\premake\\premake5.exe", vs2019 + args);
			if (result == 0)
				EG_CORE_INFO("Successfully generated VS 2019 solution files: {}", info.BasePath.u8string());
			else
				EG_CORE_ERROR("Failed to generate VS 2019 solution files: {}", info.BasePath.u8string());
		}
	}
	
	void Project::Build(const Path& outputFolder)
	{
		const Path gameExeFile = Application::GetCorePath() / "Eagle-Game.exe";
		if (!std::filesystem::exists(gameExeFile))
		{
			Application::Get().GetImGuiLayer()->AddMessage("Failed to build the game. Game executable is missing. Please, build the `Eagle-Game` project!");
			return;
		}

		YAML::Emitter shaderPackOut;
		std::thread buildThread([&outputFolder, &shaderPackOut, &gameExeFile]()
		{
			shaderPackOut << YAML::BeginMap;
			ShaderManager::BuildShaderPack(shaderPackOut);
			shaderPackOut << YAML::EndMap;

			// Creating a renderer config file
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

				const Path configFilepath = outputFolder / "Config" / "RenderConfig.ini";
				if (std::filesystem::exists(configFilepath.parent_path()) == false)
					std::filesystem::create_directory(configFilepath.parent_path());
				std::ofstream fout(configFilepath);
				fout << outRenderer.c_str();
			}

			// Copy game executable, scripts, and libs
			{
				namespace fs = std::filesystem;
				const fs::copy_options folderCopyOptions = fs::copy_options::overwrite_existing | fs::copy_options::recursive;
				const fs::copy_options fileCopyOptions = fs::copy_options::overwrite_existing;

				const Path projectScriptsFilename = s_Info.Name + ".dll";
				fs::copy(gameExeFile, outputFolder / (s_Info.Name + ".exe"), fileCopyOptions);
				fs::copy(Application::GetCorePath() / "Eagle-Scripts.dll", outputFolder / "Eagle-Scripts.dll", fileCopyOptions);
				fs::copy(Project::GetBinariesPath() / projectScriptsFilename, outputFolder / projectScriptsFilename, fileCopyOptions);

				fs::copy(Application::GetCorePath() / "assimp-vc143-mt.dll", outputFolder / "assimp-vc143-mt.dll", fileCopyOptions);
				fs::copy(Application::GetCorePath() / "mono-2.0-sgen.dll", outputFolder / "mono-2.0-sgen.dll", fileCopyOptions);
				fs::copy(Application::GetCorePath() / "mono", outputFolder / "mono", folderCopyOptions);
		#ifdef EG_DEBUG
				fs::copy(Application::GetCorePath() / "fmodL.dll", outputFolder / "fmodL.dll", fileCopyOptions);
		#else
				fs::copy(Application::GetCorePath() / "fmod.dll", outputFolder / "fmod.dll", fileCopyOptions);
		#endif
			}
		});

		size_t totalSize = sizeof(AssetHeader);
		const ScopedDataBuffer assetPack = AssetManager::BuildAssetPack();
		const size_t assetPackOffset = Utils::AddSize(assetPack, &totalSize);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << s_Info.Name;
		out << YAML::Key << "Version" << YAML::Value << s_Info.Version;

		if (s_Info.GameStartupScene)
			out << YAML::Key << "StartupScene" << YAML::Value << s_Info.GameStartupScene->GetGUID();

		out << YAML::Key << "AssetPackSize" << assetPack.Size();
		out << YAML::Key << "AssetPackOffset" << assetPackOffset;

		SaveCollisionGroups(out, s_Info);
		out << YAML::EndMap;

		std::string yamlStr = out.c_str();
		yamlStr += '\n';
		buildThread.join();
		yamlStr += shaderPackOut.c_str();

		// Compress and save
		{
			AssetHeader header = Utils::CreateHeader(yamlStr, &totalSize);
			ScopedDataBuffer build(totalSize);

			size_t offset = 0;
			Utils::WriteToBuffer(build, &header, sizeof(header), &offset);
			Utils::WriteToBuffer(build, assetPack, &offset);
			Utils::WriteStringToBuffer(build, yamlStr, &offset);

			const size_t origSize = build.Size();
			ScopedDataBuffer compressed = Compressor::Compress(build);

			ScopedDataBuffer outputData(compressed.Size() + sizeof(size_t)); // We append buffer's size at the beginning, so we need room for it
			outputData.Write(&origSize, sizeof(size_t));
			outputData.Write(compressed.Data(), compressed.Size(), sizeof(size_t));

			const Path outputFilename = outputFolder / "Data" / (s_Info.Name + AssetManager::GetAssetPackExtension());
			FileSystem::Write(outputFilename, outputData.GetDataBuffer());

#if 0 // Check if compressed correctly
			{
				ScopedDataBuffer data = ScopedDataBuffer(FileSystem::Read(outputFilename));
				const size_t compressedSize2 = data.Read<size_t>();
				DataBuffer compressedData2((uint8_t*)data.Data() + sizeof(size_t), data.Size() - sizeof(size_t));

				ScopedDataBuffer decompressedData = ScopedDataBuffer(Compressor::Decompress(compressedData2, compressedSize2));
				const bool bValid = Compressor::Validate(packData, DataBuffer(decompressedData.Data(), decompressedData.Size()));
				EG_CORE_INFO("Asset pack is valid: {}", bValid);
			}
#endif
		}
	}
	
	void Project::OpenGameBuild(const Path& filepath)
	{
		const Path& assetPackPath = filepath;
		ScopedDataBuffer compressedData = FileSystem::Read(assetPackPath);
		if (!compressedData)
		{
			EG_CORE_CRITICAL("Failed to load the asset pack: {}", assetPackPath.u8string());
			exit(-1);
		}

		const size_t origSize = compressedData.Read<size_t>();
		DataBuffer compressedDataWithOffset((uint8_t*)compressedData.Data() + sizeof(size_t), compressedData.Size() - sizeof(size_t));

		ScopedDataBuffer data = Compressor::Decompress(compressedDataWithOffset, origSize);

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		const size_t assetPackSize = baseNode["AssetPackSize"].as<size_t>();
		const size_t assetPackOffset = baseNode["AssetPackOffset"].as<size_t>();

		s_Info.Name = baseNode["Name"].as<std::string>();
		s_Info.Version = baseNode["Version"].as<glm::uvec3>();
		s_Info.BasePath = Application::GetCorePath();

		ShaderManager::InitGame(baseNode["Shaders"]);
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
		if ((s_Info.AllCollisionGroupsMask & mask) == 0u)
			return GUID64(0);

		const uint32_t index = glm::log2(mask);
		return s_Info.CollisionGroupGUIDs[index];
	}

	bool Project::Load(const Path& filepath, ProjectInfo* outInfo)
	{
		*outInfo = {};

		YAML::Node data = YAML::LoadFile(filepath.string());
		auto nameNode = data["Name"];

		if (!nameNode)
		{
			EG_CORE_ERROR("Failed to load a project. Invalid format: {}", filepath.u8string());
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
