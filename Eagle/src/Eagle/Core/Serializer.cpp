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
		SerializeTexture(out, material->GetOpacityTexture(), "OpacityTexture");
		SerializeTexture(out, material->GetOpacityMaskTexture(), "OpacityMaskTexture");

		out << YAML::Key << "TintColor" << YAML::Value << material->GetTintColor();
		out << YAML::Key << "EmissiveIntensity" << YAML::Value << material->GetEmissiveIntensity();
		out << YAML::Key << "TilingFactor" << YAML::Value << material->GetTilingFactor();
		out << YAML::Key << "BlendMode" << YAML::Value << Utils::GetEnumName(material->GetBlendMode());
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

	void Serializer::SerializeTexture(YAML::Emitter& out, const Ref<Texture2D>& texture, const std::string& textureName)
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
			out << YAML::Key << "Anisotropy" << YAML::Value << texture->GetAnisotropy();
			out << YAML::Key << "FilterMode" << YAML::Value << Utils::GetEnumName(texture->GetFilterMode());
			out << YAML::Key << "AddressMode" << YAML::Value << Utils::GetEnumName(texture->GetAddressMode());
			out << YAML::Key << "MipsCount" << YAML::Value << texture->GetMipsCount();
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
			out << YAML::Key << "Preset" << YAML::Value << Utils::GetEnumName(reverb->GetPreset());
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
		DeserializeTexture2D(materialNode, temp, "AlbedoTexture");      material->SetAlbedoTexture(temp);
		DeserializeTexture2D(materialNode, temp, "MetallnessTexture");  material->SetMetallnessTexture(temp);
		DeserializeTexture2D(materialNode, temp, "NormalTexture");      material->SetNormalTexture(temp);
		DeserializeTexture2D(materialNode, temp, "RoughnessTexture");   material->SetRoughnessTexture(temp);
		DeserializeTexture2D(materialNode, temp, "AOTexture");          material->SetAOTexture(temp);
		DeserializeTexture2D(materialNode, temp, "EmissiveTexture");    material->SetEmissiveTexture(temp);
		DeserializeTexture2D(materialNode, temp, "OpacityTexture");     material->SetOpacityTexture(temp);
		DeserializeTexture2D(materialNode, temp, "OpacityMaskTexture"); material->SetOpacityMaskTexture(temp);

		if (auto node = materialNode["TintColor"])
			material->SetTintColor(node.as<glm::vec4>());

		if (auto node = materialNode["EmissiveIntensity"])
			material->SetEmissiveIntensity(node.as<glm::vec3>());

		if (auto node = materialNode["TilingFactor"])
			material->SetTilingFactor(node.as<float>());

		if (auto node = materialNode["BlendMode"])
			material->SetBlendMode(Utils::GetEnumFromName<Material::BlendMode>(node.as<std::string>()));
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
			else if (path == "Gray")
				texture = Texture2D::GrayTexture;
			else if (path == "Red")
				texture = Texture2D::RedTexture;
			else if (path == "Green")
				texture = Texture2D::GreenTexture;
			else if (path == "Blue")
				texture = Texture2D::BlueTexture;
			else
			{
				Ref<Texture> libTexture;
				if (TextureLibrary::Get(path, &libTexture))
				{
					texture = Cast<Texture2D>(libTexture);
				}
				else
				{
					Texture2DSpecifications specs{};
					if (auto node = textureNode["Anisotropy"])
						specs.MaxAnisotropy = node.as<float>();
					if (auto node = textureNode["FilterMode"])
						specs.FilterMode = Utils::GetEnumFromName<FilterMode>(node.as<std::string>());
					if (auto node = textureNode["AddressMode"])
						specs.AddressMode = Utils::GetEnumFromName<AddressMode>(node.as<std::string>());
					if (auto node = textureNode["MipsCount"])
						specs.MipsCount = node.as<uint32_t>();

					texture = Texture2D::Create(path, specs);
				}
			}
		}
		else
			texture.reset();
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
			StaticMesh::Create(smPath, true, bImportAsSingleFileIfPossible, false);
			StaticMeshLibrary::Get(smPath, &staticMesh, meshIndex);
		}
	}

	void Serializer::DeserializeSound(YAML::Node& audioNode, Path& outSoundPath)
	{
		outSoundPath = audioNode["Path"].as<std::string>();
	}

	void Serializer::DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb)
	{
		float minDistance = reverbNode["MinDistance"].as<float>();
		float maxDistance = reverbNode["MaxDistance"].as<float>();
		reverb.SetMinMaxDistance(minDistance, maxDistance);
		reverb.SetPreset(Utils::GetEnumFromName<ReverbPreset>(reverbNode["Preset"].as<std::string>()));
		reverb.SetActive(reverbNode["IsActive"].as<bool>());
	}

	void Serializer::DeserializeFont(YAML::Node& fontNode, Ref<Font>& font)
	{
		Path path = fontNode["Path"].as<std::string>();
		if (FontLibrary::Get(path, &font) == false)
			font = Font::Create(path);
	}

	template<typename T>
	void SerializeField(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Value << YAML::BeginSeq << Utils::GetEnumName(field.Type) << field.GetStoredValue<T>() << YAML::EndSeq;
	}

	void Serializer::SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Key << field.Name;
		switch (field.Type)
		{
			case FieldType::Int:
				SerializeField<int>(out, field);
				break;
			case FieldType::UnsignedInt:
				SerializeField<unsigned int>(out, field);
				break;
			case FieldType::Float:
				SerializeField<float>(out, field);
				break;
			case FieldType::String:
				SerializeField<const std::string&>(out, field);
				break;
			case FieldType::Vec2:
				SerializeField<glm::vec2>(out, field);
				break;
			case FieldType::Vec3:
			case FieldType::Color3:
				SerializeField<glm::vec3>(out, field);
				break;
			case FieldType::Vec4:
			case FieldType::Color4:
				SerializeField<glm::vec4>(out, field);
				break;
			case FieldType::Bool:
				SerializeField<bool>(out, field);
				break;
			case FieldType::Enum:
				SerializeField<int>(out, field);
				break;
		}
	}

	template<typename T>
	void SetStoredValue(YAML::Node& node, PublicField& field)
	{
		T value = node.as<T>();
		field.SetStoredValue<T>(value);
	}

	void Serializer::DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent)
	{
		auto& publicFields = scriptComponent.PublicFields;
		for (auto& it : publicFieldsNode)
		{
			std::string fieldName = it.first.as<std::string>();
			FieldType fieldType = Utils::GetEnumFromName<FieldType>(it.second[0].as<std::string>());

			auto& fieldIt = publicFields.find(fieldName);
			if ((fieldIt != publicFields.end()) && (fieldType == fieldIt->second.Type))
			{
				PublicField& field = fieldIt->second;
				auto& node = it.second[1];
				switch (fieldType)
				{
					case FieldType::Int:
						SetStoredValue<int>(node, field);
						break;
					case FieldType::UnsignedInt:
						SetStoredValue<unsigned int>(node, field);
						break;
					case FieldType::Float:
						SetStoredValue<float>(node, field);
						break;
					case FieldType::String:
						SetStoredValue<std::string>(node, field);
						break;
					case FieldType::Vec2:
						SetStoredValue<glm::vec2>(node, field);
						break;
					case FieldType::Vec3:
					case FieldType::Color3:
						SetStoredValue<glm::vec3>(node, field);
						break;
					case FieldType::Vec4:
					case FieldType::Color4:
						SetStoredValue<glm::vec4>(node, field);
						break;
					case FieldType::Bool:
						SetStoredValue<bool>(node, field);
						break;
					case FieldType::Enum:
						SetStoredValue<int>(node, field);
						break;
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
			case FieldType::Enum: return true;
			default: return false;
		}
	}

}
