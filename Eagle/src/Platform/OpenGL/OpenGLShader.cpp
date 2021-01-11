#include "egpch.h"
#include "OpenGLShader.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <filesystem>

namespace Eagle
{
	static GLenum ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")
		{
			return GL_VERTEX_SHADER;
		}
		else if (type == "fragment" || type == "pixel")
		{
			return GL_FRAGMENT_SHADER;
		}

		EG_CORE_ASSERT(false, "Unknown Shader Type");
		return 0;
	}

	OpenGLShader::OpenGLShader(const std::string& filepath)
	{
		EG_CORE_ASSERT(std::filesystem::exists(filepath), "Shader was not found!");
		std::filesystem::path path = filepath;
		m_Name = path.stem().string();

		std::string source = ReadFile(filepath);
		auto shaderSources = Preprocess(source);
		CompileAndLink(shaderSources);
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource)
		: m_RendererID(0U), m_Name(name)
	{
		std::unordered_map<GLenum, std::string> shaderSources;
		shaderSources[GL_VERTEX_SHADER] = vertexSource;
		shaderSources[GL_FRAGMENT_SHADER] = fragmentSource;

		CompileAndLink(shaderSources);
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteProgram(m_RendererID);
	}

	std::string OpenGLShader::ReadFile(const std::string& filepath)
	{
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);

		if (in)
		{
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			if (size != -1)
			{
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
				in.close();
			}
			else
			{
				EG_CORE_ERROR("Could not read from file '{0}'", filepath);
			}
		}
		else
		{
			EG_CORE_ERROR("Could not open file {0}", filepath);
		}

		return result;
	}

	std::unordered_map<GLenum, std::string> OpenGLShader::Preprocess(const std::string& source)
	{
		std::unordered_map<GLenum, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLenght = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);

		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			EG_CORE_ASSERT(eol != std::string::npos, "Syntax Error!");

			size_t begin = pos + typeTokenLenght + 1;
			std::string type = source.substr(begin, eol - begin);
			EG_CORE_ASSERT(ShaderTypeFromString(type), "Invalid Shader Type!");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			EG_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos);

			shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

	void OpenGLShader::CompileAndLink(const std::unordered_map<GLenum, std::string>& shaderSources)
	{
		GLuint program = glCreateProgram();
		EG_CORE_ASSERT(shaderSources.size() <= 2, "Eagle supports only 2 shaders for now!");
		std::array<GLenum, 2> glShaderIDs;
		int shaderIDsIndex = 0;

		for (const auto& kv : shaderSources)
		{
			GLenum type = kv.first;
			const std::string& source = kv.second;
			
			GLuint shader = glCreateShader(type);

			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);

			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

			if (isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				glDeleteShader(shader);

				EG_CORE_ERROR("{0}", infoLog.data());
				EG_CORE_ASSERT(false, "Shader compilation failure!");

				continue;
			}
			glAttachShader(program, shader);
			glShaderIDs[shaderIDsIndex++] = shader;
		}

		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			glDeleteProgram(program);

			for (auto id : glShaderIDs)
				glDeleteShader(id);

			EG_CORE_ERROR("{0}", infoLog.data());
			EG_CORE_ASSERT(false, "Shader link failure!");

			return;
		}

		for (auto id : glShaderIDs)
		{
			glDetachShader(program, id);
			glDeleteShader(id);
		}

		m_RendererID = program;

	}

	void OpenGLShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniform1i(location, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, const int* values, uint32_t count)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
				if (location == -1)
				{
					EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
					return;
				}
		#endif 
		glUniform1iv(location, count, values);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniform1f(location, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniform2f(location, value.x, value.y);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniform3f(location, value.x, value.y, value.z);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::SetMat3(const std::string& name, const glm::mat3& matrix)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& matrix)
	{
		int location = glGetUniformLocation(m_RendererID, name.c_str());

		#if EG_DEBUG
			if (location == -1)
			{
				EG_CORE_WARN("Did not found uniform {0} in shader {1}", name, m_RendererID);
				return;
			}
		#endif 
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}