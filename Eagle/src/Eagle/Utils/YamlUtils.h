#pragma once

#include "Eagle/Core/GUID.h"

#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>
#include <Eagle/Core/Transform.h>

namespace Eagle
{
	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::mat4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq;
		for (uint32_t i = 0; i < 4; ++i)
			for (uint32_t j = 0; j < 4; ++j)
				out << v[i][j];
		out << YAML::EndSeq;
		return out;
	}
	
	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::uvec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::uvec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::uvec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::quat& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const Eagle::Rotator& rotation)
	{
		const glm::quat& v = rotation.GetQuat();
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const Eagle::GUID& guid)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << guid.GetHigh() << guid.GetLow() << YAML::EndSeq;
		return out;
	}
}

namespace YAML
{
	template<>
	struct convert<glm::mat4>
	{
		static Node encode(const glm::mat4& rhs)
		{
			Node node;
			for (uint32_t i = 0; i < 4; ++i)
				for (uint32_t j = 0; j < 4; ++j)
					node.push_back(rhs[i][j]);
			return node;
		}

		static bool decode(const Node& node, glm::mat4& rhs)
		{
			if (!node.IsSequence() || node.size() != 16)
				return false;

			for (uint32_t i = 0; i < 4; ++i)
				for (uint32_t j = 0; j < 4; ++j)
					rhs[i][j] = node[i * 4 + j].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
	
	template<>
	struct convert<glm::uvec2>
	{
		static Node encode(const glm::uvec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}

		static bool decode(const Node& node, glm::uvec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<uint32_t>();
			rhs.y = node[1].as<uint32_t>();
			return true;
		}
	};

	template<>
	struct convert<glm::uvec3>
	{
		static Node encode(const glm::uvec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::uvec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<uint32_t>();
			rhs.y = node[1].as<uint32_t>();
			rhs.z = node[2].as<uint32_t>();
			return true;
		}
	};

	template<>
	struct convert<glm::uvec4>
	{
		static Node encode(const glm::uvec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, glm::uvec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<uint32_t>();
			rhs.y = node[1].as<uint32_t>();
			rhs.z = node[2].as<uint32_t>();
			rhs.w = node[3].as<uint32_t>();
			return true;
		}
	};

	template<>
	struct convert<glm::quat>
	{
		static Node encode(const glm::quat& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, glm::quat& rhs)
		{
			if (!node.IsSequence())
				return false;

			if (node.size() == 3)
			{
				glm::vec3 euler;
				euler.x = node[0].as<float>();
				euler.y = node[1].as<float>();
				euler.z = node[2].as<float>();

				rhs = Eagle::Rotator::FromEulerAngles(euler).GetQuat();
				return true;
			}
			else if (node.size() == 4)
			{
				rhs.x = node[0].as<float>();
				rhs.y = node[1].as<float>();
				rhs.z = node[2].as<float>();
				rhs.w = node[3].as<float>();
				return true;
			}

			return false;
		}
	};

	template<>
	struct convert<Eagle::Rotator>
	{
		static Node encode(const Eagle::Rotator& rhs)
		{
			Node node;
			node.push_back(rhs.GetQuat().x);
			node.push_back(rhs.GetQuat().y);
			node.push_back(rhs.GetQuat().z);
			node.push_back(rhs.GetQuat().w);
			return node;
		}

		static bool decode(const Node& node, Eagle::Rotator& rhs)
		{
			if (!node.IsSequence())
				return false;

			if (node.size() == 3)
			{
				glm::vec3 euler;
				euler.x = node[0].as<float>();
				euler.y = node[1].as<float>();
				euler.z = node[2].as<float>();

				rhs = Eagle::Rotator::FromEulerAngles(euler);
				return true;
			}
			else if (node.size() == 4)
			{
				rhs.GetQuat().x = node[0].as<float>();
				rhs.GetQuat().y = node[1].as<float>();
				rhs.GetQuat().z = node[2].as<float>();
				rhs.GetQuat().w = node[3].as<float>();
				return true;
			}

			return false;
		}
	};

	template<>
	struct convert<Eagle::GUID>
	{
		static Node encode(const Eagle::GUID& rhs)
		{
			Node node;
			node.push_back(rhs.GetHigh());
			node.push_back(rhs.GetLow());
			return node;
		}

		static bool decode(const Node& node, Eagle::GUID& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs = Eagle::GUID(node[0].as<uint64_t>(), node[1].as<uint64_t>());
			return true;
		}
	};

}

