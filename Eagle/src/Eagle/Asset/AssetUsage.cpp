#include "egpch.h"
#include "AssetUsage.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Core/AsyncTask.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Utils/SerializerUtils.h"
#include "Eagle/Utils/Timer.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	namespace
	{
		// Lower case, forward slashes, no duplicate slashes, no leading "./".
		// Used to compare asset paths with the paths that are written in C# sources ("Content\\a.egasset" -> "Content/a.egasset")
		std::string NormalizePath(std::string_view str)
		{
			std::string result;
			result.reserve(str.size());
			for (auto c : str)
			{
				if (c == '\\')
					c = '/';
				else
					c = std::tolower(c);

				if (c == '/' && !result.empty() && result.back() == '/')
					continue;
				result.push_back(c);
			}

			while (result.starts_with("./"))
				result.erase(0, 2);
			return result;
		}

		bool IsPathDelimiter(char c)
		{
			switch (c)
			{
				case '"': case '\'': case '`': case '\n': case '\r': case '\t':
				case '(': case ')': case '[': case ']': case '{': case '}': case '<': case '>':
				case '=': case '+': case ',': case ';': case '|': case '?': case '*': case '@': case '$':
					return true;
			}
			return false;
		}

		bool ReadAssetYaml(const Path& path, std::string* outYaml, std::string* outError)
		{
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file)
			{
				*outError = "Couldn't open the file";
				return false;
			}

			const size_t fileSize = (size_t)file.tellg();
			if (fileSize < sizeof(AssetHeader))
			{
				*outError = "The file is too small";
				return false;
			}

			AssetHeader header;
			file.seekg(0);
			file.read((char*)&header, sizeof(header));
			if (!file || header.OffsetToYaml < sizeof(header) || header.OffsetToYaml > fileSize)
			{
				*outError = "The file is corrupted";
				return false;
			}

			outYaml->resize(fileSize - header.OffsetToYaml);
			file.seekg(header.OffsetToYaml);
			file.read(outYaml->data(), outYaml->size());
			if (!file)
			{
				*outError = "Couldn't read the file";
				return false;
			}

			// The YAML is null-terminated
			if (const size_t end = outYaml->find('\0'); end != std::string::npos)
				outYaml->resize(end);
			return true;
		}

		std::vector<Path> FindScripts(const Path& folder)
		{
			namespace fs = std::filesystem;

			std::vector<Path> result;
			std::error_code error;
			if (!fs::is_directory(folder, error))
				return result;

			for (const auto& dir : std::filesystem::recursive_directory_iterator(folder))
			{
				const Path& path = dir.path();
				if (Utils::AsString(path.extension()) == ".cs")
					result.push_back(path);
			}
			return result;
		}
	}

	AssetUsage::Request AssetUsage::CreateRequest(AssetUsageMode mode)
	{
		Request request;
		request.Mode = mode;

		const auto& assets = AssetManager::GetAssets();
		request.Assets.reserve(assets.size());
		for (const auto& [path, asset] : assets)
		{
			request.Assets.push_back({ path, asset->GetGUID(), asset->GetAssetType() });
			if (asset->IsAlwaysIncludedInGameBuilds())
				request.AlwaysIncluded.push_back(asset->GetGUID());
		}

		const auto& projectInfo = Project::GetProjectInfo();
		if (projectInfo.GameStartupScene)
			request.StartupScene = projectInfo.GameStartupScene->GetGUID();
		request.ScriptsFolder = Project::GetSourcePath();

		return request;
	}

	bool AssetUsage::Analyze(const Request& request, AssetUsageReport* outReport)
	{
		Timer timer;
		AsyncTaskContext* task = AsyncTaskContext::GetCurrent();
		auto isCancelled = [task]() { return task && task->IsCancelRequested(); };

		AssetUsageReport report;
		report.Mode = request.Mode;

		const auto& assets = request.Assets;
		const size_t assetsCount = assets.size();

		std::vector<AssetUsageReason> reasons(assetsCount, AssetUsageReason::Unused);
		std::vector<Path> sources(assetsCount);
		std::vector<size_t> queueToCheck;
		queueToCheck.reserve(assetsCount);

		ankerl::unordered_dense::map<GUID, size_t> indicesByGUID;
		ankerl::unordered_dense::map<std::string, std::vector<size_t>> indicesByPath;
		indicesByGUID.reserve(assetsCount);
		indicesByPath.reserve(assetsCount);

		for (size_t i = 0; i < assetsCount; ++i)
		{
			const auto& asset = assets[i];
			indicesByGUID[asset.ID] = i;
			indicesByPath[NormalizePath(Utils::AsString(asset.Filepath))].push_back(i);
		}

		auto markUsed = [&reasons, &sources, &queueToCheck](size_t index, AssetUsageReason reason, const Path& source = "")
		{
			if (reasons[index] != AssetUsageReason::Unused)
				return;

			reasons[index] = reason;
			sources[index] = source;
			queueToCheck.push_back(index);
		};
		auto markUsedByGUID = [&markUsed, &indicesByGUID](const GUID& guid, AssetUsageReason reason, const Path& source = "")
		{
			auto it = indicesByGUID.find(guid);
			if (it == indicesByGUID.end())
				return false;

			const size_t index = it->second;
			markUsed(index, reason, source);
			return true;
		};

		// 1. Root
		if (request.Mode == AssetUsageMode::GameBuild)
		{
			if (request.StartupScene.IsNull())
				report.Warnings.push_back("The game startup scene isn't set, so the game doesn't use any scene");
			else if (!markUsedByGUID(request.StartupScene, AssetUsageReason::StartupScene))
				report.Warnings.push_back("The game startup scene doesn't exist");
		}
		else
		{
			for (size_t i = 0; i < assetsCount; ++i)
			{
				if (assets[i].Type == AssetType::Scene)
					markUsed(i, AssetUsageReason::Scene);
			}
		}

		for (const GUID& guid : request.AlwaysIncluded)
			markUsedByGUID(guid, AssetUsageReason::AlwaysInclude);

		// 2. Assets whose paths are found in C# scripts
		{
			AsyncTaskProgressScope progress(0.f, 0.1f);
			if (task)
				task->SetStatus("Searching C# scripts for asset paths");

			const std::vector<Path> scripts = FindScripts(request.ScriptsFolder);
			const Path projectFolder = request.ScriptsFolder.parent_path();
			for (size_t scriptIndex = 0; scriptIndex < scripts.size(); ++scriptIndex)
			{
				if (isCancelled())
					return false;

				const Path& scriptPath = scripts[scriptIndex];
				if (task)
				{
					task->SetDetail(Utils::AsString(scriptPath.filename()));
					task->SetProgress(scriptIndex, scripts.size());
				}

				const std::string code = NormalizePath(FileSystem::ReadText(scriptPath));
				if (code.empty())
				{
					report.Warnings.push_back("Either there's no code, or couldn't read the script: " + Utils::AsString(scriptPath));
					continue;
				}

				std::error_code error;
				Path displayedPath = std::filesystem::relative(scriptPath, projectFolder, error);
				if (error || displayedPath.empty())
					displayedPath = scriptPath;

				// Find every "something.egasset"
				std::string_view assetExtension = Asset::GetExtension();
				for (size_t pos = code.find(assetExtension); pos != std::string::npos; pos = code.find(assetExtension, pos + 1))
				{
					const size_t end = pos + assetExtension.size();
					size_t begin = pos;
					while (begin > 0 && !IsPathDelimiter(code[begin - 1]))
						--begin;

					const std::string candidate(code.data() + begin, end - begin);
					if (auto it = indicesByPath.find(candidate); it != indicesByPath.end())
					{
						for (size_t index : it->second)
							markUsed(index, AssetUsageReason::ScriptPath, displayedPath);
					}
				}
			}
		}

		// 3. Follow the references
		{
			AsyncTaskProgressScope progress(0.1f, 0.95f);
			if (task)
				task->SetStatus("Following asset references");

			std::string yaml;
			std::string error;
			std::vector<GUID> references;
			for (size_t queueIndex = 0; queueIndex < queueToCheck.size(); ++queueIndex)
			{
				if (isCancelled())
					return false;

				const size_t index = queueToCheck[queueIndex]; // Copy, `queueToCheck` grows in the loop
				const auto& asset = assets[index];
				const std::string assetPath = Utils::AsString(asset.Filepath);
				if (task)
				{
					task->SetDetail(assetPath);
					task->SetProgress(queueIndex, assetsCount);
				}

				if (!ReadAssetYaml(asset.Filepath, &yaml, &error))
				{
					report.Warnings.push_back("Couldn't read the references of " + assetPath + ". " + error);
					continue;
				}

				references.clear();
				try
				{
					AssetManager::CollectGUIDs(YAML::Load(yaml), references);
				}
				catch (const std::exception& e)
				{
					report.Warnings.push_back("Couldn't read the references of " + assetPath + ". " + e.what());
					continue;
				}

				for (const GUID& reference : references)
				{
					if (reference != asset.ID)
						markUsedByGUID(reference, AssetUsageReason::Referenced, asset.Filepath);
				}
			}
		}

		// 4. Results
		{
			AsyncTaskProgressScope progress(0.95f, 1.f);
			if (task)
			{
				task->SetStatus("Collecting the results");
				task->SetDetail({});
			}

			for (size_t i = 0; i < assetsCount; ++i)
			{
				if (isCancelled())
					return false;

				const auto& asset = assets[i];
				AssetUsageData data;
				data.Filepath = asset.Filepath;
				data.ID = asset.ID;
				data.Type = asset.Type;
				data.Reason = reasons[i];
				data.Source = std::move(sources[i]);

				std::error_code error;
				data.FileSize = size_t(std::filesystem::file_size(asset.Filepath, error));
				if (error)
				{
					data.FileSize = 0;
					report.Warnings.push_back("The file doesn't exist: " + Utils::AsString(asset.Filepath));
				}

				if (data.Reason == AssetUsageReason::Unused)
				{
					report.UnusedSize += data.FileSize;
					report.Unused.push_back(std::move(data));
				}
				else
				{
					report.UsedSize += data.FileSize;
					report.Used.push_back(std::move(data));
				}
			}

			auto byPath = [](const AssetUsageData& a, const AssetUsageData& b) { return a.Filepath < b.Filepath; };
			std::sort(report.Used.begin(), report.Used.end(), byPath);
			std::sort(report.Unused.begin(), report.Unused.end(), byPath);
		}

		report.SecondsTook = float(timer.GetSeconds());
		*outReport = std::move(report);
		return true;
	}
}
