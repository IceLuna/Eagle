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

namespace Eagle
{
	ProjectInfo Project::s_Info = {};
	
	bool Project::Create(const ProjectInfo& info)
	{
		// TODO: needed?
		if (!std::filesystem::exists(info.BasePath))
			std::filesystem::create_directory(info.BasePath);

		if (!std::filesystem::is_directory(info.BasePath))
		{
			EG_CORE_ERROR("Couldn't create project at: {}. It's not a folder!", info.BasePath.u8string());
			return false;
		}

		// Checking if folder already contains another eagle project
		for (auto& dirEntry : std::filesystem::directory_iterator(info.BasePath))
		{
			if (std::filesystem::is_directory(dirEntry))
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

		std::filesystem::create_directory(info.BasePath / "Content");
		std::filesystem::create_directory(info.BasePath / "Binaries");

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Name" << YAML::Value << info.Name;

		out << YAML::EndMap;

		std::ofstream fout(info.BasePath / (info.Name + GetExtension()));
		fout << out.c_str();
		fout.close();

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

		YAML::Node data = YAML::LoadFile(filepath.string());
		auto nameNode = data["Name"];

		if (!nameNode)
		{
			EG_CORE_ERROR("Failed to load a project. Invalid format: {}", filepath.u8string());
			return {};
		}

		s_Info = {};
		s_Info.BasePath = filepath.parent_path();
		s_Info.Name = nameNode.as<std::string>();

		Application::OnProjectChanged(true);
		EG_CORE_INFO("Opened project at: {}", s_Info.BasePath.u8string());

		return true;
	}
	
	bool Project::Close()
	{
		if (s_Info.BasePath.empty())
		{
			EG_CORE_ERROR("Failed to close the project. There's no an opened project!");
			return false;
		}

		EG_CORE_INFO("Closed project at: {}", s_Info.BasePath.u8string());
		s_Info = {};
		Application::OnProjectChanged(false);

		return true;
	}
	
	void Project::GenerateSolution(const ProjectInfo& info)
	{
		std::string args = std::string("vs2022 --file=..\\premake5_project.lua ") + "--projectname=" + info.Name
			+ " --projectdir=" + info.BasePath.u8string() + " --eagledir=" + Application::GetCorePath().parent_path().u8string();
		
		const int result = Utils::Execute("..\\vendor\\premake\\premake5.exe", args);
		if (result == 0)
			EG_CORE_INFO("Successfully generated VS 2022 solution files: {}", info.BasePath);
		else
			EG_CORE_INFO("Failed to generate VS 2022 solution files: {}", info.BasePath);
	}
	
	void Project::Build(const Path& outputFolder)
	{
		YAML::Emitter shaderPackOut;
		std::thread buildThread([&outputFolder, &shaderPackOut]()
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
				fs::copy(Application::GetCorePath() / "Eagle-Game.exe", outputFolder / (s_Info.Name + ".exe"), fileCopyOptions);
				fs::copy(Application::GetCorePath() / "Eagle-Scripts.dll", outputFolder / "Eagle-Scripts.dll", fileCopyOptions);
				fs::copy(Project::GetBinariesPath() / projectScriptsFilename, outputFolder / projectScriptsFilename, fileCopyOptions);

				fs::copy(Application::GetCorePath() / "assimp-vc143-mt.dll", outputFolder / "assimp-vc143-mt.dll", fileCopyOptions);
				fs::copy(Application::GetCorePath() / "mono-2.0-sgen.dll", outputFolder / "mono-2.0-sgen.dll", fileCopyOptions);
				fs::copy(Application::GetCorePath() / "zlib.dll", outputFolder / "zlib.dll", fileCopyOptions);
				fs::copy(Application::GetCorePath() / "mono", outputFolder / "mono", folderCopyOptions);
		#ifdef EG_DEBUG
				fs::copy(Application::GetCorePath() / "fmodL.dll", outputFolder / "fmodL.dll", fileCopyOptions);
		#else
				fs::copy(Application::GetCorePath() / "fmod.dll", outputFolder / "fmod.dll", fileCopyOptions);
		#endif
			}
		});

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << s_Info.Name;
		out << YAML::Key << "Version" << YAML::Value << s_Info.Version;

		if (s_Info.GameStartupScene)
			out << YAML::Key << "StartupScene" << YAML::Value << s_Info.GameStartupScene->GetGUID();

		AssetManager::BuildAssetPack(out);
		out << YAML::EndMap;

		std::string serializedData = out.c_str();
		serializedData += '\n';
		buildThread.join();
		serializedData += shaderPackOut.c_str();

		YAML::Node baseNode = YAML::Load(serializedData);

		// Compress and save
		{
			DataBuffer packData(serializedData.data(), serializedData.size());

			ScopedDataBuffer compressedPack = ScopedDataBuffer(Compressor::Compress(packData));
			const size_t compressedSize = compressedPack.Size();

			ScopedDataBuffer outputData;
			outputData.Allocate(compressedPack.Size() + sizeof(size_t)); // We append buffer's size at the beginning, so we need room for it
			outputData.Write(&compressedSize, sizeof(size_t));
			outputData.Write(compressedPack.Data(), compressedSize, sizeof(size_t));

			const Path outputFilename = outputFolder / "Data" / (s_Info.Name + AssetManager::GetAssetPackExtension());
			FileSystem::Write(outputFilename, DataBuffer(outputData.Data(), outputData.Size()));

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

		return;
	}
	
	void Project::OpenGameBuild(const Path& filepath)
	{
		const Path& assetPack = filepath;
		ScopedDataBuffer data = ScopedDataBuffer(FileSystem::Read(assetPack));
		if (!data)
		{
			EG_CORE_CRITICAL("Failed to load the asset pack: {}", assetPack);
			exit(-1);
		}

		const size_t compressedSize2 = data.Read<size_t>();
		DataBuffer compressedData2((uint8_t*)data.Data() + sizeof(size_t), data.Size() - sizeof(size_t));

		ScopedDataBuffer decompressedData = ScopedDataBuffer(Compressor::Decompress(compressedData2, compressedSize2));
		std::string packData;
		packData.resize(decompressedData.Size());
		memcpy(packData.data(), decompressedData.Data(), decompressedData.Size());

		YAML::Node baseNode = YAML::Load(packData);

		s_Info.Name = baseNode["Name"].as<std::string>();
		s_Info.Version = baseNode["Version"].as<glm::uvec3>();
		s_Info.BasePath = Application::GetCorePath();

		AssetManager::InitGame(baseNode["Assets"]);
		ShaderManager::InitGame(baseNode["Shaders"]);
		if (auto startupSceneNode = baseNode["StartupScene"])
		{
			Ref<Asset> asset;
			if (AssetManager::Get(startupSceneNode.as<GUID>(), &asset))
				s_Info.GameStartupScene = Cast<AssetScene>(asset);
		}
	}
}
