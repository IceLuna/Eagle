#include "egpch.h"
#include "Serializer.h"

#include "Eagle/Components/Components.h"
#include "Eagle/UI/Font.h"

namespace Eagle
{
	void Serializer::SerializeMaterial(YAML::Emitter& out, const Ref<Material>& material)
	{
		out << YAML::Key << "Material";
		out << YAML::BeginMap; //Material

		SerializeTexture(out, material->GetAlbedoTexture(), "AlbedoTexture");
		SerializeTexture(out, material->GetMetallnessTexture(), "MetallnessTexture");
		SerializeTexture(out, material->GetNormalTexture(), "NormalTexture");
		SerializeTexture(out, material->GetRoughnessTexture(), "RoughnessTexture");
		SerializeTexture(out, material->GetAOTexture(), "AOTexture");
		SerializeTexture(out, material->GetEmissiveTexture(), "EmissiveTexture");

		out << YAML::Key << "TintColor" << YAML::Value << material->TintColor;
		out << YAML::Key << "EmissiveIntensity" << YAML::Value << material->EmissiveIntensity;
		out << YAML::Key << "TilingFactor" << YAML::Value << material->TilingFactor;
		out << YAML::EndMap; //Material
	}

	void Serializer::SerializePhysicsMaterial(YAML::Emitter& out, const Ref<PhysicsMaterial>& material)
	{
		out << YAML::Key << "PhysicsMaterial";
		out << YAML::BeginMap; //PhysicsMaterial

		out << YAML::Key << "StaticFriction" << YAML::Value << material->StaticFriction;
		out << YAML::Key << "DynamicFriction" << YAML::Value << material->DynamicFriction;
		out << YAML::Key << "Bounciness" << YAML::Value << material->Bounciness;

		out << YAML::EndMap; //PhysicsMaterial
	}

	void Serializer::SerializeTexture(YAML::Emitter& out, const Ref<Texture>& texture, const std::string& textureName)
	{
		if (bool bValidTexture = texture.operator bool())
		{
			Path currentPath = std::filesystem::current_path();
			Path textureRelPath = std::filesystem::relative(texture->GetPath(), currentPath);
			if (textureRelPath.empty())
				textureRelPath = texture->GetPath();

			out << YAML::Key << textureName;
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << textureRelPath.string();
			out << YAML::EndMap;
		}
		else
		{
			out << YAML::Key << textureName;
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << "None";
			out << YAML::EndMap;
		}
	}

	void Serializer::SerializeStaticMesh(YAML::Emitter& out, const Ref<StaticMesh>& staticMesh)
	{
		if (staticMesh)
		{
			Path currentPath = std::filesystem::current_path();
			Path smRelPath = std::filesystem::relative(staticMesh->GetPath(), currentPath);

			out << YAML::Key << "StaticMesh";
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << smRelPath.string();
			out << YAML::Key << "Index" << YAML::Value << staticMesh->GetIndex();
			out << YAML::Key << "MadeOfMultipleMeshes" << YAML::Value << staticMesh->IsMadeOfMultipleMeshes();
			out << YAML::EndMap;
		}
	}

	void Serializer::SerializeSound(YAML::Emitter& out, const Ref<Sound>& sound)
	{
		out << YAML::Key << "Sound";
		out << YAML::BeginMap;
		out << YAML::Key << "Path" << YAML::Value << (sound ? sound->GetSoundPath().string() : "");
		out << YAML::EndMap;
	}

	void Serializer::SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb)
	{
		if (reverb)
		{
			out << YAML::Key << "Reverb";
			out << YAML::BeginMap;
			out << YAML::Key << "MinDistance" << YAML::Value << reverb->GetMinDistance();
			out << YAML::Key << "MaxDistance" << YAML::Value << reverb->GetMaxDistance();
			out << YAML::Key << "Preset" << YAML::Value << (uint32_t)reverb->GetPreset();
			out << YAML::Key << "IsActive" << YAML::Value << reverb->IsActive();
			out << YAML::EndMap;
		}
	}

	void Serializer::SerializeFont(YAML::Emitter& out, const Ref<Font>& font)
	{
		if (font)
		{
			out << YAML::Key << "Font";
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << font->GetPath().string();
			out << YAML::EndMap;
		}
	}

	void Serializer::DeserializeMaterial(YAML::Node& materialNode, Ref<Material>& material)
	{
		Ref<Texture2D> temp;
		DeserializeTexture2D(materialNode, temp, "AlbedoTexture");     material->SetAlbedoTexture(temp);
		DeserializeTexture2D(materialNode, temp, "MetallnessTexture"); material->SetMetallnessTexture(temp);
		DeserializeTexture2D(materialNode, temp, "NormalTexture");     material->SetNormalTexture(temp);
		DeserializeTexture2D(materialNode, temp, "RoughnessTexture");  material->SetRoughnessTexture(temp);
		DeserializeTexture2D(materialNode, temp, "AOTexture");         material->SetAOTexture(temp);
		DeserializeTexture2D(materialNode, temp, "EmissiveTexture");   material->SetEmissiveTexture(temp);

		if (auto node = materialNode["TintColor"])
			material->TintColor = node.as<glm::vec4>();

		if (auto node = materialNode["EmissiveIntensity"])
			material->EmissiveIntensity = node.as<glm::vec3>();

		if (auto node = materialNode["TilingFactor"])
			material->TilingFactor = node.as<float>();
	}

	void Serializer::DeserializePhysicsMaterial(YAML::Node& materialNode, Ref<PhysicsMaterial>& material)
	{
		if (auto node = materialNode["StaticFriction"])
		{
			float staticFriction = node.as<float>();
			material->StaticFriction = staticFriction;
		}

		if (auto node = materialNode["DynamicFriction"])
		{
			float dynamicFriction = node.as<float>();
			material->DynamicFriction = dynamicFriction;
		}

		if (auto node = materialNode["Bounciness"])
		{
			float bounciness = node.as<float>();
			material->Bounciness = bounciness;
		}
	}

	void Serializer::DeserializeTexture2D(YAML::Node& parentNode, Ref<Texture2D>& texture, const std::string& textureName)
	{
		if (auto textureNode = parentNode[textureName])
		{
			const Path& path = textureNode["Path"].as<std::string>();

			if (path == "None")
				texture.reset();
			else if (path == "White")
				texture = Texture2D::WhiteTexture;
			else if (path == "Black")
				texture = Texture2D::BlackTexture;
			else
			{
				Ref<Texture> libTexture;
				if (TextureLibrary::Get(path, &libTexture))
				{
					texture = Cast<Texture2D>(libTexture);
				}
				else
				{
					texture = Texture2D::Create(path);
				}
			}
		}
	}

	void Serializer::DeserializeStaticMesh(YAML::Node& meshNode, Ref<StaticMesh>& staticMesh)
	{
		Path smPath = meshNode["Path"].as<std::string>();
		uint32_t meshIndex = 0u;
		bool bImportAsSingleFileIfPossible = false;
		if (auto node = meshNode["Index"])
			meshIndex = node.as<uint32_t>();
		if (auto node = meshNode["MadeOfMultipleMeshes"])
			bImportAsSingleFileIfPossible = node.as<bool>();

		if (StaticMeshLibrary::Get(smPath, &staticMesh, meshIndex) == false)
		{
			staticMesh = StaticMesh::Create(smPath, true, bImportAsSingleFileIfPossible, false);
		}
	}

	void Serializer::DeserializeSound(YAML::Node& audioNode, Path& outSoundPath)
	{
		outSoundPath = audioNode["Path"].as<std::string>();
	}

	void Serializer::DeserializeReverb(YAML::Node& reverbNode, Ref<Reverb3D>& reverb)
	{
		float minDistance = reverbNode["MinDistance"].as<float>();
		float maxDistance = reverbNode["MaxDistance"].as<float>();
		reverb->SetMinMaxDistance(minDistance, maxDistance);
		reverb->SetPreset(ReverbPreset(reverbNode["Preset"].as<uint32_t>()));
		reverb->SetActive(reverbNode["IsActive"].as<bool>());
	}

	void Serializer::DeserializeFont(YAML::Node& fontNode, Ref<Font>& font)
	{
		Path path = fontNode["Path"].as<std::string>();
		if (FontLibrary::Get(path, &font) == false)
			font = Font::Create(path);
	}

	void Serializer::SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Key << field.Name;
		switch (field.Type)
		{
			case FieldType::Int:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<int>() << YAML::EndSeq;
				break;
			}
			case FieldType::UnsignedInt:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<unsigned int>() << YAML::EndSeq;
				break;
			}
			case FieldType::Float:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<float>() << YAML::EndSeq;
				break;
			}
			case FieldType::String:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<std::string>() << YAML::EndSeq;
				break;
			}
			case FieldType::Vec2:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<glm::vec2>() << YAML::EndSeq;
				break;
			}
			case FieldType::Vec3:
			case FieldType::Color3:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<glm::vec3>() << YAML::EndSeq;
				break;
			}
			case FieldType::Vec4:
			case FieldType::Color4:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<glm::vec4>() << YAML::EndSeq;
				break;
			}
			case FieldType::Bool:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<bool>() << YAML::EndSeq;
				break;
			}
		}
	}

	void Serializer::DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent)
	{
		auto& publicFields = scriptComponent.PublicFields;
		for (auto& it : publicFieldsNode)
		{
			std::string fieldName = it.first.as<std::string>();
			FieldType fieldType = (FieldType)it.second[0].as<uint32_t>();

			auto& fieldIt = publicFields.find(fieldName);
			if ((fieldIt != publicFields.end()) && (fieldType == fieldIt->second.Type))
			{
				PublicField& field = fieldIt->second;
				switch (fieldType)
				{
					case FieldType::Int:
					{
						int value = it.second[1].as<int>();
						field.SetStoredValue<int>(value);
						break;
					}
					case FieldType::UnsignedInt:
					{
						unsigned int value = it.second[1].as<unsigned int>();
						field.SetStoredValue<unsigned int>(value);
						break;
					}
					case FieldType::Float:
					{
						float value = it.second[1].as<float>();
						field.SetStoredValue<float>(value);
						break;
					}
					case FieldType::String:
					{
						std::string value = it.second[1].as<std::string>();
						field.SetStoredValue<std::string>(value);
						break;
					}
					case FieldType::Vec2:
					{
						glm::vec2 value = it.second[1].as<glm::vec2>();
						field.SetStoredValue<glm::vec2>(value);
						break;
					}
					case FieldType::Vec3:
					case FieldType::Color3:
					{
						glm::vec3 value = it.second[1].as<glm::vec3>();
						field.SetStoredValue<glm::vec3>(value);
						break;
					}
					case FieldType::Vec4:
					case FieldType::Color4:
					{
						glm::vec4 value = it.second[1].as<glm::vec4>();
						field.SetStoredValue<glm::vec4>(value);
						break;
					}
					case FieldType::Bool:
					{
						bool value = it.second[1].as<bool>();
						field.SetStoredValue<bool>(value);
						break;
					}
				}
			}
		}
	}

	bool Serializer::HasSerializableType(const PublicField& field)
	{
		switch (field.Type)
		{
			case FieldType::Int: return true;
			case FieldType::UnsignedInt: return true;
			case FieldType::Float: return true;
			case FieldType::String: return true;
			case FieldType::Vec2: return true;
			case FieldType::Vec3: return true;
			case FieldType::Vec4: return true;
			case FieldType::Bool: return true;
			case FieldType::Color3: return true;
			case FieldType::Color4: return true;
			default: return false;
		}
	}

}
