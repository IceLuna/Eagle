#pragma once

// Private helpers shared by the Serializer*.cpp files. Don't include it anywhere else.

#include "Eagle/Core/Entity.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Script/PublicField.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Utils/YamlUtils.h"

namespace Eagle
{
	template<typename AssetType>
	Ref<AssetType> GetAsset(const YAML::Node& node)
	{
		if (node)
		{
			Ref<Asset> asset;
			if (AssetManager::Get(node.as<GUID>(), &asset))
				return Cast<AssetType>(asset);
		}

		return {};
	}

	inline void SerializeGraphVar(YAML::Emitter& out, const Ref<GraphVariable>& var)
	{
		GraphVariableType varType = var->GetType();
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(varType);
		out << YAML::Key << "bShowInUI" << YAML::Value << var->bShowInUI;

		if (var->HasValue())
		{
			out << YAML::Key << "Value";
			switch (varType)
			{
			case GraphVariableType::Bool:
				out << YAML::Value << Cast<GraphVariableBool>(var)->Value;
				break;
			case GraphVariableType::Float:
				out << YAML::Value << Cast<GraphVariableFloat>(var)->Value;
				break;
			case GraphVariableType::Int:
				out << YAML::Value << Cast<GraphVariableInt>(var)->Value;
				break;
			case GraphVariableType::Animation:
				out << YAML::Value << Cast<GraphVariableAnimation>(var)->Value->GetGUID();
				break;
			case GraphVariableType::String:
				out << YAML::Value << Cast<GraphVariableString>(var)->Value;
				break;
			case GraphVariableType::Vec4:
				out << YAML::Value << Cast<GraphVariableVec4>(var)->Value;
				break;
			default:
				EG_CORE_ASSERT(false);
			}
		}
	}

	inline Ref<GraphVariable> DeserializeGraphVar(const YAML::Node& varNode)
	{
		GraphVariableType varType = Utils::GetEnumFromName<GraphVariableType>(varNode["Type"].as<std::string>());
		auto valueNode = varNode["Value"];
		Ref<GraphVariable> result;

		switch (varType)
		{
		case GraphVariableType::Bool:
			result = MakeRef<GraphVariableBool>(valueNode ? valueNode.as<bool>() : false, true);
			break;
		case GraphVariableType::Int:
			result = MakeRef<GraphVariableInt>(valueNode ? valueNode.as<int>() : 0, true);
			break;
		case GraphVariableType::Float:
			result = MakeRef<GraphVariableFloat>(valueNode ? valueNode.as<float>() : 0.f, true);
			break;
		case GraphVariableType::Animation:
			result = MakeRef<GraphVariableAnimation>(valueNode ? GetAsset<AssetAnimation>(valueNode) : nullptr, true);
			break;
		case GraphVariableType::String:
			result = MakeRef<GraphVariableString>(valueNode ? valueNode.as<std::string>() : "", true);
			break;
		case GraphVariableType::Vec4:
			result = MakeRef<GraphVariableVec4>(valueNode ? valueNode.as<glm::vec4>() : glm::vec4(0), true);
			break;
		default:
			EG_CORE_ASSERT(false);
			break;
		}

		if (result)
		{
			if (auto bShowNode = varNode["bShowInUI"])
			{
				result->bShowInUI = bShowNode.as<bool>();
			}
		}

		return result;
	}

	inline void SerializeScriptFields(YAML::Emitter& out, const std::vector<PublicField>& fields)
	{
		out << YAML::Key << "PublicFields";
		out << YAML::BeginMap;
		for (const auto& field : fields)
		{
			if (Serializer::HasSerializableType(field))
				Serializer::SerializePublicFieldValue(out, field);
		}
		out << YAML::EndMap;
	}
}
