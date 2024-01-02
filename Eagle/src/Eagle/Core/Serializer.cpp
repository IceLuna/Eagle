#include "egpch.h"
#include "Serializer.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Components/Components.h"
#include "Eagle/UI/Font.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Compressor.h"

#include <stb_image.h>

namespace Eagle
{
	static Ref<AssetTexture2D> GetAssetTexture2D(const YAML::Node& node)
	{
		Ref<AssetTexture2D> result;
		if (node)
		{
			Ref<Asset> asset;
			if (AssetManager::Get(node.as<GUID>(), &asset))
				result = Cast<AssetTexture2D>(asset);
		}
		return result;
	}

	static bool SanitaryAssetChecks(const YAML::Node& baseNode, const Path& path, AssetType expectedType)
	{
		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to deserialize an asset: {}", path);
			return false;
		}

		AssetType actualType = AssetType::None;
		if (auto node = baseNode["Type"])
			actualType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		if (actualType != expectedType)
		{
			EG_CORE_ERROR("Failed to load an asset. It's not a {}: {}", Utils::GetEnumName(actualType), path);
			return false;
		}

		return true;
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

	void Serializer::SerializeAsset(YAML::Emitter& out, const Ref<Asset>& asset)
	{
		switch (asset->GetAssetType())
		{
			case AssetType::Texture2D:
				SerializeAssetTexture2D(out, Cast<AssetTexture2D>(asset));
				break;
			case AssetType::TextureCube:
				SerializeAssetTextureCube(out, Cast<AssetTextureCube>(asset));
				break;
			case AssetType::Mesh:
				SerializeAssetMesh(out, Cast<AssetMesh>(asset));
				break;
			case AssetType::Sound:
				SerializeAssetSound(out, Cast<AssetSound>(asset));
				break;
			case AssetType::Font:
				SerializeAssetFont(out, Cast<AssetFont>(asset));
				break;
			case AssetType::Material:
				SerializeAssetMaterial(out, Cast<AssetMaterial>(asset));
				break;
			case AssetType::PhysicsMaterial:
				SerializeAssetPhysicsMaterial(out, Cast<AssetPhysicsMaterial>(asset));
				break;
			default: EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
		}
	}

	void Serializer::SerializeAssetTexture2D(YAML::Emitter& out, const Ref<AssetTexture2D>& asset)
	{
		const auto& texture = asset->GetTexture();
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Texture2D);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "FilterMode" << YAML::Value << Utils::GetEnumName(texture->GetFilterMode());
		out << YAML::Key << "AddressMode" << YAML::Value << Utils::GetEnumName(texture->GetAddressMode());
		out << YAML::Key << "Anisotropy" << YAML::Value << texture->GetAnisotropy();
		out << YAML::Key << "MipsCount" << YAML::Value << texture->GetMipsCount();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(asset->GetFormat());

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetTextureCube(YAML::Emitter& out, const Ref<AssetTextureCube>& asset)
	{
		const auto& textureCube = asset->GetTexture();
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::TextureCube);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(asset->GetFormat());
		out << YAML::Key << "LayerSize" << YAML::Value << textureCube->GetSize().x;

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetMesh(YAML::Emitter& out, const Ref<AssetMesh>& asset)
	{
		const auto& mesh = asset->GetMesh();
		DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(Vertex) };
		DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(), mesh->GetIndicesCount() * sizeof(Vertex) };
		const size_t origVerticesDataSize = verticesBuffer.Size; // Required for decompression
		const size_t origIndicesDataSize = indicesBuffer.Size; // Required for decompression

		ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));
		ScopedDataBuffer compressedIndices(Compressor::Compress(indicesBuffer));

		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Mesh);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Index" << YAML::Value << asset->GetMeshIndex(); // Index of a mesh if a mesh-file (fbx) contains multiple meshes

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "SizeVertices" << YAML::Value << origVerticesDataSize;
		out << YAML::Key << "SizeIndices" << YAML::Value << origIndicesDataSize;
		out << YAML::Key << "Vertices" << YAML::Value << YAML::Binary((uint8_t*)compressedVertices.Data(), compressedVertices.Size());
		out << YAML::Key << "Indices" << YAML::Value << YAML::Binary((uint8_t*)compressedIndices.Data(), compressedIndices.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetSound(YAML::Emitter& out, const Ref<AssetSound>& asset)
	{

	}

	void Serializer::SerializeAssetFont(YAML::Emitter& out, const Ref<AssetFont>& asset)
	{

	}

	void Serializer::SerializeAssetMaterial(YAML::Emitter& out, const Ref<AssetMaterial>& asset)
	{
		const auto& material = asset->GetMaterial();

		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Material);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		if (const auto& textureAsset = material->GetAlbedoTexture())
			out << YAML::Key << "AlbedoTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetMetallnessTexture())
			out << YAML::Key << "MetallnessTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetNormalTexture())
			out << YAML::Key << "NormalTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetRoughnessTexture())
			out << YAML::Key << "RoughnessTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetAOTexture())
			out << YAML::Key << "AOTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetEmissiveTexture())
			out << YAML::Key << "EmissiveTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetOpacityTexture())
			out << YAML::Key << "OpacityTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetOpacityMaskTexture())
			out << YAML::Key << "OpacityMaskTexture" << YAML::Value << textureAsset->GetGUID();

		out << YAML::Key << "TintColor" << YAML::Value << material->GetTintColor();
		out << YAML::Key << "EmissiveIntensity" << YAML::Value << material->GetEmissiveIntensity();
		out << YAML::Key << "TilingFactor" << YAML::Value << material->GetTilingFactor();
		out << YAML::Key << "BlendMode" << YAML::Value << Utils::GetEnumName(material->GetBlendMode());
		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetPhysicsMaterial(YAML::Emitter& out, const Ref<AssetPhysicsMaterial>& asset)
	{

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

	Ref<Asset> Serializer::DeserializeAsset(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		AssetType assetType;
		auto typeNode = baseNode["Type"];
		if (!typeNode)
		{
			EG_CORE_ERROR("Failed to load an asset. It's not an eagle asset");
			return {};
		}
		assetType = Utils::GetEnumFromName<AssetType>(typeNode.as<std::string>());
		
		switch (assetType)
		{
		case AssetType::Texture2D:
			return DeserializeAssetTexture2D(baseNode, pathToAsset, bReloadRaw);
		case AssetType::TextureCube:
			return DeserializeAssetTextureCube(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Mesh:
			return DeserializeAssetMesh(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Sound:
			return DeserializeAssetSound(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Font:
			return DeserializeAssetFont(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Material:
			return DeserializeAssetMaterial(baseNode, pathToAsset);
		case AssetType::PhysicsMaterial:
			return DeserializeAssetPhysicsMaterial(baseNode, pathToAsset);
		default:
			EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
			return {};
		}
	}

	Ref<AssetTexture2D> Serializer::DeserializeAssetTexture2D(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Texture2D))
			return {};
		
		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();

		Texture2DSpecifications specs{};
		specs.FilterMode = Utils::GetEnumFromName<FilterMode>(baseNode["FilterMode"].as<std::string>());
		specs.AddressMode = Utils::GetEnumFromName<AddressMode>(baseNode["AddressMode"].as<std::string>());
		specs.MaxAnisotropy = baseNode["Anisotropy"].as<float>();
		specs.MipsCount = baseNode["MipsCount"].as<uint32_t>();

		AssetTexture2DFormat assetFormat = Utils::GetEnumFromName<AssetTexture2DFormat>(baseNode["Format"].as<std::string>());

		int width, height, channels;
		const int desiredChannels = AssetTextureFormatToChannels(assetFormat);
		void* imageData = nullptr;

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinary;
		YAML::Binary yamlBinary;

		void* binaryData = nullptr;
		size_t binaryDataSize = 0ull;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			binaryData = binary.Data();
			binaryDataSize = binary.Size();
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				size_t origSize = 0u;
				bool bCompressed = false;
				if (auto node = baseDataNode["Compressed"])
				{
					bCompressed = node.as<bool>();
					if (bCompressed)
						origSize = baseDataNode["Size"].as<size_t>();
				}

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
					binaryData = decompressedBinary.Data();
					binaryDataSize = decompressedBinary.Size();
				}
				else
				{
					binaryData = (void*)yamlBinary.data();
					binaryDataSize = yamlBinary.size();
				}
			}
		}
		imageData = stbi_load_from_memory((uint8_t*)binaryData, (int)binaryDataSize, &width, &height, &channels, desiredChannels);

		if (!imageData)
		{
			EG_CORE_ERROR("Import failed. stbi_load failed: {}", Utils::GetEnumName(assetFormat));
			return {};
		}

		const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);

		class LocalAssetTexture2D : public AssetTexture2D
		{
		public:
			LocalAssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Texture2D>& texture, AssetTexture2DFormat format)
				: AssetTexture2D(path, pathToRaw, guid, rawData, texture, format) {}
		};

		Ref<AssetTexture2D> asset = MakeRef<LocalAssetTexture2D>(pathToAsset, pathToRaw, guid, DataBuffer(binaryData, binaryDataSize),
			Texture2D::Create(pathToAsset.stem().u8string(), imageFormat, glm::uvec2(width, height), imageData, specs), assetFormat);

		stbi_image_free(imageData);

		return asset;
	}

	Ref<AssetTextureCube> Serializer::DeserializeAssetTextureCube(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::TextureCube))
			return {}; // Failed

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const AssetTextureCubeFormat assetFormat = Utils::GetEnumFromName<AssetTextureCubeFormat>(baseNode["Format"].as<std::string>());
		const uint32_t layerSize = baseNode["LayerSize"].as<uint32_t>();

		int width, height, channels;
		const int desiredChannels = AssetTextureFormatToChannels(assetFormat);
		void* imageData = nullptr;

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinary;
		YAML::Binary yamlBinary;

		void* binaryData = nullptr;
		size_t binaryDataSize = 0ull;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			binaryData = binary.Data();
			binaryDataSize = binary.Size();
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				size_t origSize = 0u;
				bool bCompressed = false;
				if (auto node = baseDataNode["Compressed"])
				{
					bCompressed = node.as<bool>();
					if (bCompressed)
						origSize = baseDataNode["Size"].as<size_t>();
				}

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
					binaryData = decompressedBinary.Data();
					binaryDataSize = decompressedBinary.Size();
				}
				else
				{
					binaryData = (void*)yamlBinary.data();
					binaryDataSize = yamlBinary.size();
				}
			}
		}

		imageData = stbi_loadf_from_memory((uint8_t*)binaryData, (int)binaryDataSize, &width, &height, &channels, desiredChannels);

		if (!imageData)
		{
			EG_CORE_ERROR("Import failed. stbi_loadf failed: {} - {}", pathToAsset, Utils::GetEnumName(assetFormat));
			return {};
		}
		const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);

		class LocalAssetTextureCube : public AssetTextureCube
		{
		public:
			LocalAssetTextureCube(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<TextureCube>& texture, AssetTextureCubeFormat format)
				: AssetTextureCube(path, pathToRaw, guid, rawData, texture, format) {}
		};

		Ref<AssetTextureCube> asset = MakeRef<LocalAssetTextureCube>(pathToAsset, pathToRaw, guid, DataBuffer(binaryData, binaryDataSize),
			TextureCube::Create(pathToAsset.stem().u8string(), imageFormat, imageData, glm::uvec2(width, height), layerSize), assetFormat);

		stbi_image_free(imageData);

		return asset;
	}

	Ref<AssetMesh> Serializer::DeserializeAssetMesh(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetMesh : public AssetMesh
		{
		public:
			LocalAssetMesh(const Path& path, GUID guid, const Ref<StaticMesh>& mesh, uint32_t meshIndex)
				: AssetMesh(path, guid, mesh, meshIndex) {}
		};

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Mesh))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();
		const uint32_t meshIndex = baseNode["Index"].as<uint32_t>();

		if (bReloadRaw)
		{
			auto meshes = Utils::ImportMeshes(pathToRaw);
			if (meshes.size() < meshIndex)
			{
				EG_CORE_ERROR("Failed to reload a mesh asset at index {}. File {} contains {} meshes", meshIndex, pathToRaw, meshes.size());
				return {};
			}

			return MakeRef<LocalAssetMesh>(pathToAsset, guid, meshes[meshIndex], meshIndex);
		}

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinaryVertices;
		ScopedDataBuffer decompressedBinaryIndices;
		YAML::Binary yamlBinaryVertices;
		YAML::Binary yamlBinaryIndices;

		void* binaryVerticesData = nullptr;
		size_t binaryVerticesDataSize = 0ull;
		void* binaryIndicesData = nullptr;
		size_t binaryIndicesDataSize = 0ull;

		if (auto baseDataNode = baseNode["Data"])
		{
			size_t origVerticesSize = 0u;
			size_t origIndicesSize = 0u;
			bool bCompressed = false;
			if (auto node = baseDataNode["Compressed"])
			{
				bCompressed = node.as<bool>();
				if (bCompressed)
				{
					origVerticesSize = baseDataNode["SizeVertices"].as<size_t>();
					origIndicesSize = baseDataNode["SizeIndices"].as<size_t>();
				}
			}

			// Vertices
			{
				yamlBinaryVertices = baseDataNode["Vertices"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinaryVertices = Compressor::Decompress(DataBuffer{ (void*)yamlBinaryVertices.data(), yamlBinaryVertices.size() }, origVerticesSize);
					binaryVerticesData = decompressedBinaryVertices.Data();
					binaryVerticesDataSize = decompressedBinaryVertices.Size();
				}
				else
				{
					binaryVerticesData = (void*)yamlBinaryVertices.data();
					binaryVerticesDataSize = yamlBinaryVertices.size();
				}
			}

			// Indices
			{
				yamlBinaryIndices = baseDataNode["Indices"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinaryIndices = Compressor::Decompress(DataBuffer{ (void*)yamlBinaryIndices.data(), yamlBinaryIndices.size() }, origIndicesSize);
					binaryIndicesData = decompressedBinaryIndices.Data();
					binaryIndicesDataSize = decompressedBinaryIndices.Size();
				}
				else
				{
					binaryIndicesData = (void*)yamlBinaryIndices.data();
					binaryIndicesDataSize = yamlBinaryIndices.size();
				}
			}
		}

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// Vertices
		{
			const size_t verticesCount = binaryVerticesDataSize / sizeof(Vertex);
			vertices.resize(verticesCount);
			memcpy(vertices.data(), binaryVerticesData, binaryVerticesDataSize);
		}

		// Indices
		{
			const size_t indicesCount = binaryIndicesDataSize / sizeof(uint32_t);
			indices.resize(indicesCount);
			memcpy(indices.data(), binaryIndicesData, binaryIndicesDataSize);
		}

		return MakeRef<LocalAssetMesh>(pathToAsset, guid, StaticMesh::Create(vertices, indices), meshIndex);
	}

	Ref<AssetSound> Serializer::DeserializeAssetSound(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		return {};
	}

	Ref<AssetFont> Serializer::DeserializeAssetFont(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		return {};
	}

	Ref<AssetMaterial> Serializer::DeserializeAssetMaterial(YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Material))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

		Ref<Material> material = Material::Create();

		material->SetAlbedoTexture(GetAssetTexture2D(baseNode["AlbedoTexture"]));
		material->SetMetallnessTexture(GetAssetTexture2D(baseNode["MetallnessTexture"]));
		material->SetNormalTexture(GetAssetTexture2D(baseNode["NormalTexture"]));
		material->SetRoughnessTexture(GetAssetTexture2D(baseNode["RoughnessTexture"]));
		material->SetAOTexture(GetAssetTexture2D(baseNode["AOTexture"]));
		material->SetEmissiveTexture(GetAssetTexture2D(baseNode["EmissiveTexture"]));
		material->SetOpacityTexture(GetAssetTexture2D(baseNode["OpacityTexture"]));
		material->SetOpacityMaskTexture(GetAssetTexture2D(baseNode["OpacityMaskTexture"]));

		if (auto node = baseNode["TintColor"])
			material->SetTintColor(node.as<glm::vec4>());

		if (auto node = baseNode["EmissiveIntensity"])
			material->SetEmissiveIntensity(node.as<glm::vec3>());

		if (auto node = baseNode["TilingFactor"])
			material->SetTilingFactor(node.as<float>());

		if (auto node = baseNode["BlendMode"])
			material->SetBlendMode(Utils::GetEnumFromName<Material::BlendMode>(node.as<std::string>()));

		class LocalAssetMaterial : public AssetMaterial
		{
		public:
			LocalAssetMaterial(const Path& path, GUID guid, const Ref<Material>& material)
				: AssetMaterial(path, guid, material) {}
		};

		return MakeRef<LocalAssetMaterial>(pathToAsset, guid, material);
	}

	Ref<AssetPhysicsMaterial> Serializer::DeserializeAssetPhysicsMaterial(YAML::Node& baseNode, const Path& pathToAsset)
	{
		return {};
	}

	AssetType Serializer::GetAssetType(const Path& pathToAsset)
	{
		YAML::Node baseNode = YAML::LoadFile(pathToAsset.string());

		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to get an asset type: {}", pathToAsset);
			return AssetType::None;
		}

		AssetType actualType = AssetType::None;
		if (auto node = baseNode["Type"])
			actualType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		return actualType;
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
