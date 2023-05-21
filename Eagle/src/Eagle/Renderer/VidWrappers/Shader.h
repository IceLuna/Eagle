#pragma once

#include "Eagle/Renderer/RendererUtils.h"
#include <glm/glm.hpp>

namespace Eagle
{
	struct ShaderSpecializationMapEntry
	{
		uint32_t ConstantID = 0;
		uint32_t Offset = 0;
		size_t   Size = 0;
	};

	struct ShaderSpecializationInfo
	{
		std::vector<ShaderSpecializationMapEntry> MapEntries;
		const void* Data = nullptr;
		size_t Size = 0;
	};

	struct PushConstantRange
	{
		ShaderType ShaderStage;
		uint32_t Offset;
		uint32_t Size;
	};

	// Define - value
	using ShaderDefines = std::unordered_map<std::string, std::string>;

	class Shader : public std::enable_shared_from_this<Shader>
	{
	protected:
		Shader(const Path& path, ShaderType shaderType, const ShaderDefines& defines = {})
			: m_Defines(defines)
			, m_Path(path)
			, m_ShaderType(shaderType) {}

	public:
		using ShaderReloadedCallback = std::function<void()>;

		virtual ~Shader() = default;
		virtual void Reload() = 0;
		uint64_t GetHash() const { return m_Hash; };

		void OnReloaded()
		{
			for (auto& it : m_ReloadedCallbacks)
				it.second();
		}

		// Does reload
		virtual void SetDefines(const ShaderDefines& defines) = 0;
		const ShaderDefines& GetDefines() { return m_Defines; }

		void AddReloadedCallback(void* id, ShaderReloadedCallback func) { m_ReloadedCallbacks.insert({ id, std::move(func) }); }
		void RemoveReloadedCallback(void* id) { m_ReloadedCallbacks.erase(id); }

		const std::vector<PushConstantRange>& GetPushConstantRanges() const { return m_PushConstantRanges; }
		ShaderType GetType() const { return m_ShaderType; }
		const Path& GetPath() const { return m_Path; };

		static Ref<Shader> Create(const Path& path, const ShaderDefines& defines = {});
		static Ref<Shader> Create(const Path& path, ShaderType shaderType, const ShaderDefines& defines = {});

	protected:
		ShaderDefines m_Defines;
		Path m_Path;
		uint64_t m_Hash = 0;
		ShaderType m_ShaderType;
		std::vector<PushConstantRange> m_PushConstantRanges;
		std::unordered_map<void*, ShaderReloadedCallback> m_ReloadedCallbacks;
	};

	class ShaderLibrary
	{
	public:
		static Ref<Shader> GetOrLoad(const Path& filepath, ShaderType shaderType);
		static void Add(const Ref<Shader>& shader);
		static bool Exists(const Path& filepath);

		static void ReloadAllShaders();

		//TODO: Move to AssetManager::Shutdown()
		static void Clear() { m_Shaders.clear(); m_ShadersByPath.clear(); }

	private:
		// Not the best solution.
		// The problem is that we can't store same shaders (that have same Path) with different defines if we use Path as a key (so techically they're different, since theirs hash is different)
		// So we use vector to store all shaders and potentially reload all of them
		// m_Shaders2 is there so we can retrive "default" shaders by path using GetOrLoad()

		// Shaders
		static std::vector<Ref<Shader>> m_Shaders;
		// Path -> Shader
		static std::map<Path, Ref<Shader>> m_ShadersByPath;
	};
}