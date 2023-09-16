#pragma once

#include "Eagle/Core/Transform.h"
#include "Eagle/Utils/Utils.h"
#include <yaml-cpp/yaml.h>

namespace YAML
{
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
}

namespace Eagle
{
	class Material;
	class PhysicsMaterial;
	class PublicField;
	class ScriptComponent;
	class StaticMesh;
	class Sound;
	class Texture;
	class Texture2D;
	class Reverb3D;
	class Font;

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

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::quat& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const Rotator& rotation)
	{
		const glm::quat& v = rotation.GetQuat();
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	namespace Serializer
	{
		void SerializeMaterial(YAML::Emitter& out, const Ref<Material>& material);
		void SerializePhysicsMaterial(YAML::Emitter& out, const Ref<PhysicsMaterial>& material);
		void SerializeTexture(YAML::Emitter& out, const Ref<Texture2D>& texture, const std::string& textureName);
		void SerializeStaticMesh(YAML::Emitter& out, const Ref<StaticMesh>& staticMesh);
		void SerializeSound(YAML::Emitter& out, const Ref<Sound>& sound);
		void SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb);
		void SerializeFont(YAML::Emitter& out, const Ref<Font>& font);

		void DeserializeMaterial(YAML::Node& materialNode, Ref<Material>& material);
		void DeserializePhysicsMaterial(YAML::Node& materialNode, Ref<PhysicsMaterial>& material);
		void DeserializeTexture2D(YAML::Node& parentNode, Ref<Texture2D>& texture, const std::string& textureName);
		void DeserializeStaticMesh(YAML::Node& meshNode, Ref<StaticMesh>& staticMesh);
		void DeserializeSound(YAML::Node& audioNode, Path& outSoundPath);
		void DeserializeReverb(YAML::Node& reverbNode, Ref<Reverb3D>& reverb);
		void DeserializeFont(YAML::Node& fontNode, Ref<Font>& font);

		void SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field);
		void DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent);
		bool HasSerializableType(const PublicField& field);
	}
}
