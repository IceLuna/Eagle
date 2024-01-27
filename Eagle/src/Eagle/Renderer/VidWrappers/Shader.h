#pragma once

#include "Eagle/Renderer/RendererUtils.h"
#include <glm/glm.hpp>

namespace YAML
{
	class Emitter;
	class Node;
}

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

	class Shader
	{
	protected:
		Shader(const Path& path, ShaderType shaderType, const ShaderDefines& defines = {})
			: m_Defines(defines)
			, m_Path(path)
			, m_ShaderType(shaderType) {}

	public:
		using ShaderReloadedCallback = std::function<void()>;

		virtual ~Shader() = default;

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
		virtual void Reload() = 0;

	protected:
		ShaderDefines m_Defines;
		Path m_Path;
		ShaderType m_ShaderType;
		std::vector<PushConstantRange> m_PushConstantRanges;
		std::unordered_map<void*, ShaderReloadedCallback> m_ReloadedCallbacks;

		friend class ShaderManager;
	};

	class ShaderManager
	{
	public:
		static void Init();
		static void InitGame(const YAML::Node& shadersNode);

		static void Add(const Ref<Shader>& shader);
		static bool Exists(const Path& filepath);
		static const std::string& GetSource(const Path& filepath);

		static void BuildShaderPack(YAML::Emitter& out);
		static void ReloadAllShaders();

		static void Reset()
		{
			m_Shaders.clear();
			m_ShadersByPath.clear();
			m_ShadersSourceCode.clear();
		}

	private:
		// Not the best solution.
		// The problem is that we can't store same shaders (that have same Path) with different defines if we use Path as a key (so techically they're different, since theirs hash is different)
		// So we use vector to store all shaders and potentially reload all of them
		// m_Shaders is there so we can retrive "default" shaders by path using GetOrLoad()

		// Shaders
		static std::vector<Ref<Shader>> m_Shaders;
		// Path -> Shader
		static std::unordered_map<Path, Ref<Shader>> m_ShadersByPath;
		// Path -> Source code
		static std::unordered_map<Path, std::string> m_ShadersSourceCode;
	};
}