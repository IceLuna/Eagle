#include "egpch.h"
#include "Project.h"
#include "Application.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/YamlUtils.h"

namespace Eagle
{
	ProjectInfo Project::s_Info = {};

	static const char* s_ProjectExtension = ".egproj";
	
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
			
			if (Utils::HasExtension(filepath, s_ProjectExtension))
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

		std::ofstream fout(info.BasePath / (info.Name + s_ProjectExtension));
		fout << out.c_str();
		fout.close();

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

		if (!Utils::HasExtension(filepath, s_ProjectExtension))
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

		s_Info = {};
		Application::OnProjectChanged(false);

		EG_CORE_INFO("Closed project at: {}", s_Info.BasePath.u8string());
		return true;
	}
}
