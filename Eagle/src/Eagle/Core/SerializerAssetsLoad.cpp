#include "egpch.h"
#include "Serializer.h"
#include "SerializerInternal.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Classes/Font.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Utils/AssimpImporter.h"
#include "Eagle/Utils/SerializerUtils.h"

#include <stb_image.h>

namespace Eagle
{
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

	static void DeserializeAIBehaviorNode(const YAML::Node& yamlNode, AIBehaviorNode& node)
	{
		const std::string fullName = yamlNode["FullName"].as<std::string>();
		node.Data = ScriptEngine::GetAIClassData(fullName);
		node.Data.ID = yamlNode["ID"].as<GUID>();

		if (auto publicFieldsNode = yamlNode["PublicFields"])
			Serializer::DeserializePublicFieldValues(publicFieldsNode, node.Data.ClassData.Fields);

		if (auto decoratorsNode = yamlNode["Decorators"])
		{
			for (auto decoratorNode : decoratorsNode)
			{
				const std::string fullName = decoratorNode["FullName"].as<std::string>();
				auto& decorator = node.AttachedDecorators.emplace_back();
				decorator = ScriptEngine::GetAIClassData(fullName);

				if (auto publicFieldsNode = decoratorNode["PublicFields"])
					Serializer::DeserializePublicFieldValues(publicFieldsNode, decorator.ClassData.Fields);
			}
		}

		if (auto childrenNode = yamlNode["Children"])
		{
			for (auto childNode : childrenNode)
			{
				auto& child = node.Children.emplace_back();
				DeserializeAIBehaviorNode(childNode, child);
			}
		}
	}

	static void DeserializeGraph(const YAML::Node& baseGraphNode, GraphSerializationData& data)
	{
		data.Name = baseGraphNode["Name"].as<std::string>();
		data.ScrollOffset = baseGraphNode["Scroll"].as<glm::vec2>();
		if (auto node = baseGraphNode["ID"])
			data.ID = node.as<GUID>();
		data.Zoom = baseGraphNode["Zoom"].as<float>();

		auto nodesNode = baseGraphNode["Nodes"];
		for (const auto& nodeNode : nodesNode)
		{
			auto& nodeData = data.Nodes.emplace_back();
			if (auto ownerNode = nodeNode["OwnerID"])
				nodeData.OwnerID = ownerNode.as<GUID>();
			nodeData.Name = nodeNode["Name"].as<std::string>();
			nodeData.Position = nodeNode["Position"].as<glm::vec2>();
			if (auto sizeNode = nodeNode["Size"])
				nodeData.Size = sizeNode.as<glm::vec2>();
			nodeData.NodeID = nodeNode["NodeID"].as<GUID>();
			if (auto isOutputNode = nodeNode["IsOutputNode"])
				nodeData.bOutputNode = isOutputNode.as<bool>();
			if (auto cachedNode = nodeNode["CachedOwnerID"])
				nodeData.CachedOwnerID = cachedNode.as<GUID>();
			if (auto cachedNode = nodeNode["CachedNodeID"])
				nodeData.CachedNodeID = cachedNode.as<GUID>();
			if (auto typeNode = nodeNode["Type"])
				nodeData.Type = Utils::GetEnumFromName<GraphNodeType>(typeNode.as<std::string>());
			if (auto bsNode = nodeNode["BlendSpace"])
				nodeData.BlendSpace = GetAsset<AssetAnimationBlendSpace>(bsNode);
			if (auto counterNode = nodeNode["AddedCounter"])
				nodeData.AddedCounter = counterNode.as<uint32_t>();
			if (auto userDataNode = nodeNode["UserData"])
				nodeData.UserData = userDataNode.as<std::string>();
			if (auto node = nodeNode["BehaviorNodeData"])
			{
				AIBehaviorNode behaviorNode;
				DeserializeAIBehaviorNode(node, behaviorNode);
				nodeData.BehaviorClassData = std::move(behaviorNode.Data);
				nodeData.AttachedDecorators = std::move(behaviorNode.AttachedDecorators);
			}

			{
				const auto inputPinsNode = nodeNode["InputPins"];
				nodeData.InputPins.reserve(inputPinsNode.size());
				for (const auto& inputPinNode : inputPinsNode)
				{
					auto& pinData = nodeData.InputPins.emplace_back();
					pinData.Index = inputPinNode["ID"].as<uint32_t>();
					if (auto defaultValNode = inputPinNode["DefaultValue"])
						pinData.DefaultValue = DeserializeGraphVar(defaultValNode);
				}
			}

			auto connectionsNode = nodeNode["Connections"];
			for (const auto& connectionNode : connectionsNode)
			{
				GraphConnectionData& connectionData = nodeData.OutputConnections.emplace_back();
				connectionData.NodeID = connectionNode["NodeID"].as<GUID>();
				connectionData.PinIndex = connectionNode["PinIndex"].as<uint32_t>();
			}
		}

		if (auto subgraphsNode = baseGraphNode["Subgraphs"])
		{
			for (const auto& subgraphNode : subgraphsNode)
			{
				DeserializeGraph(subgraphNode, data.Subgraphs.emplace_back());
			}
		}
	}

	static RootMotionMode GetRootMotionMode(const YAML::Node& node)
	{
		const auto baseNode = node["Animation"];
		if (!baseNode)
			return RootMotionMode::Disabled;

		RootMotionMode mode = RootMotionMode::Disabled;
		if (auto rootMotionModeNode = baseNode["RootMotionMode"])
		{
			mode = Utils::GetEnumFromName<RootMotionMode>(rootMotionModeNode.as<std::string>());
		}

		return mode;
	}

	template <typename T>
	static void DeserializeCurve(const YAML::Node& parentNode, const char* name, Curve<T>& channel)
	{
		channel.Clear();

		auto channelNode = parentNode[name];
		if (!channelNode)
			return;

		auto& keys = channel.GetKeys();
		keys.reserve(channelNode.size());

		for (auto keyNode : channelNode)
		{
			CurveKey<T> key;

			if (auto node = keyNode["ID"])
				key.ID = node.as<GUID>();

			if (auto node = keyNode["Time"])
				key.Time = node.as<float>();

			if (auto node = keyNode["Value"])
				key.Value = node.as<T>();

			if (auto node = keyNode["Interpolation"])
				key.Interpolation = Eagle::Utils::GetEnumFromName<CurveInterpolation>(node.as<std::string>());

			if (auto node = keyNode["InTangent"])
				key.InTangent = node.as<T>();

			if (auto node = keyNode["OutTangent"])
				key.OutTangent = node.as<T>();

			keys.push_back(key);
		}

		// Keys are written sorted, but still sort them just in case
		channel.SortKeys();
	}

	// Returns false if `parentNode` has no such property
	template <typename T>
	static bool DeserializeCurveProperty(const YAML::Node& parentNode, const char* name, CurveProperty<T>& property)
	{
		auto propertyNode = parentNode[name];
		if (!propertyNode)
			return false;

		if (auto node = propertyNode["Mode"])
			property.Mode = Eagle::Utils::GetEnumFromName<CurvePropertyMode>(node.as<std::string>());
		if (auto node = propertyNode["Constant"])
			property.Constant = node.as<T>();
		DeserializeCurve(propertyNode, "Keys", property.Keys);
		return true;
	}

	static Ref<SequenceTrack> DeserializeSequenceTrack(const YAML::Node& trackNode)
	{
		auto typeNode = trackNode["Type"];
		if (!typeNode)
			return {};

		const SequenceTrackType type = Eagle::Utils::GetEnumFromName<SequenceTrackType>(typeNode.as<std::string>());
		Ref<SequenceTrack> track = SequenceTrack::Create(type);
		if (!track)
			return {};

		if (auto node = trackNode["ID"])
			track->SetID(node.as<GUID>());

		if (auto node = trackNode["Name"])
			track->SetName(node.as<std::string>());

		if (auto node = trackNode["Enabled"])
			track->SetEnabled(node.as<bool>());

		switch (type)
		{
			case SequenceTrackType::Camera:
			{
				SequenceCameraTrack& cameraTrack = (SequenceCameraTrack&)(*track);

				if (auto node = trackNode["DefaultFOV"])
					cameraTrack.SetDefaultFOVDegrees(node.as<float>());

				if (auto node = trackNode["NearClip"])
					cameraTrack.SetNearClip(node.as<float>());

				if (auto node = trackNode["FarClip"])
					cameraTrack.SetFarClip(node.as<float>());

				DeserializeCurve(trackNode, "Location", cameraTrack.GetLocationChannel());
				DeserializeCurve(trackNode, "Rotation", cameraTrack.GetRotationChannel());
				DeserializeCurve(trackNode, "FOV", cameraTrack.GetFOVChannel());
				break;
			}
			case SequenceTrackType::CameraCuts:
			{
				SequenceCameraCutTrack& cutTrack = (SequenceCameraCutTrack&)(*track);
				DeserializeCurve(trackNode, "Cuts", cutTrack.GetCutsChannel());
				break;
			}
			case SequenceTrackType::PostProcess:
			{
				SequencePostProcessTrack& postProcessTrack = (SequencePostProcessTrack&)(*track);

				if (auto node = trackNode["CameraTrack"])
					postProcessTrack.SetCameraTrackID(node.as<GUID>());
				if (auto node = trackNode["ApplyWholeSequence"])
					postProcessTrack.SetApplyWholeSequence(node.as<bool>());

				if (auto propertiesNode = trackNode["Properties"])
				{
					for (auto propertyNode : propertiesNode)
					{
						auto nameNode = propertyNode["Property"];
						if (!nameNode)
							continue;

						const std::string name = nameNode.as<std::string>();
						const auto property = magic_enum::enum_cast<PostProcessProperty>(name);
						if (!property.has_value() || !FindPostProcessProperty(*property))
						{
							EG_CORE_WARN("[Serializer] Skipping unknown post process property: {}", name);
							continue;
						}

						auto& channel = postProcessTrack.AddProperty(*property);
						if (auto enabledNode = propertyNode["Enabled"])
							channel.bEnabled = enabledNode.as<bool>();
						std::visit([&propertyNode](auto& data) { DeserializeCurve(propertyNode, "Keys", data); }, channel.Data);
					}
				}
				break;
			}
			case SequenceTrackType::Event:
			{
				SequenceEventTrack& eventTrack = (SequenceEventTrack&)(*track);
				DeserializeCurve(trackNode, "Events", eventTrack.GetEventsChannel());
				break;
			}
		}

		return track;
	}

	Ref<Asset> Serializer::DeserializeAsset(const Path& pathToAsset, bool bReloadRaw)
	{
		ScopedDataBuffer data = FileSystem::Read(pathToAsset);
		if (!data)
		{
			EG_CORE_ERROR("Failed to deserialize an asset. Couldn't read the file: {}", pathToAsset);
			return {};
		}
		return DeserializeAsset(data.GetDataBuffer(), pathToAsset, bReloadRaw);
	}

	Ref<Asset> Serializer::DeserializeAsset(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		const AssetType assetType = GetAssetType(data);

		switch (assetType)
		{
		case AssetType::Texture2D:
			return DeserializeAssetTexture2D(data, pathToAsset, bReloadRaw);
		case AssetType::TextureCube:
			return DeserializeAssetTextureCube(data, pathToAsset, bReloadRaw);
		case AssetType::StaticMesh:
			return DeserializeAssetStaticMesh(data, pathToAsset, bReloadRaw);
		case AssetType::SkeletalMesh:
			return DeserializeAssetSkeletalMesh(data, pathToAsset, bReloadRaw);
		case AssetType::Audio:
			return DeserializeAssetAudio(data, pathToAsset, bReloadRaw);
		case AssetType::Font:
			return DeserializeAssetFont(data, pathToAsset, bReloadRaw);
		case AssetType::Material:
			return DeserializeAssetMaterial(data, pathToAsset);
		case AssetType::PhysicsMaterial:
			return DeserializeAssetPhysicsMaterial(data, pathToAsset);
		case AssetType::SoundGroup:
			return DeserializeAssetSoundGroup(data, pathToAsset);
		case AssetType::Entity:
			return DeserializeAssetEntity(data, pathToAsset);
		case AssetType::Scene:
			return DeserializeAssetScene(data, pathToAsset);
		case AssetType::Animation:
			return DeserializeAssetAnimation(data, pathToAsset, bReloadRaw);
		case AssetType::AnimationGraph:
			return DeserializeAssetAnimationGraph(data, pathToAsset);
		case AssetType::ParticleSystem:
			return DeserializeAssetParticleSystem(data, pathToAsset);
		case AssetType::AnimationBlendSpace:
			return DeserializeAssetAnimationBlendSpace(data, pathToAsset);
		case AssetType::BehaviorGraph:
			return DeserializeAssetBehaviorGraph(data, pathToAsset);
		case AssetType::SceneSequence:
			return DeserializeAssetSceneSequence(data, pathToAsset);
		default:
			EG_CORE_ASSERT(false);
			EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
			return {};
		}
	}

	Ref<AssetTexture2D> Serializer::DeserializeAssetTexture2D(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Texture2D))
			return {};
		
		const Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;
		int width = 0, height = 0, channels = 0;

		Texture2DSpecifications specs{};
		specs.FilterMode = Utils::GetEnumFromName<FilterMode>(baseNode["FilterMode"].as<std::string>());
		specs.AddressMode = Utils::GetEnumFromName<AddressMode>(baseNode["AddressMode"].as<std::string>());
		specs.MaxAnisotropy = baseNode["Anisotropy"].as<float>();
		specs.MipsCount = baseNode["MipsCount"].as<uint32_t>();
		if (auto node = baseNode["Width"])
			width = node.as<int>();
		if (auto node = baseNode["Height"])
			height = node.as<int>();

		AssetTexture2DFormat assetFormat = Utils::GetEnumFromName<AssetTexture2DFormat>(baseNode["Format"].as<std::string>());

		const bool bNormalMap = baseNode["IsNormalMap"].as<bool>();
		TextureCompressor::Quality compression = TextureCompressor::Quality::Disabled;
		if (auto node = baseNode["Compression"])
			compression = Utils::GetEnumFromName<TextureCompressor::Quality>(node.as<std::string>());

		ScopedDataBuffer binary;
		ImageFormat compressedFormat = ImageFormat::Unknown;
		std::vector<ScopedDataBuffer> compressedTextures;

		// Reload the data from disk
		// Or get the data from the asset file.
		// If the texture is compressed, an asset will contain compressed texture data that's ready to go.
		// Otherwise, it'll contain the data that needs to be loaded by stb_image
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			stbi_info(Utils::AsString(pathToRaw).c_str(), &width, &height, &channels);
			assetFormat = ChannelsToAssetTexture2DFormat(channels);
			if (!binary)
			{
				EG_CORE_ERROR("Failed to reload from raw texture 2D: {}", pathToAsset);
				return {};
			}
		}
		else if (auto baseDataNode = baseNode["Data"])
		{
			const size_t dataSize = baseDataNode["Size"].as<size_t>();
			const size_t dataOffset = baseDataNode["Offset"].as<size_t>();

			if (!Utils::ReadCompressedBinary(data, dataSize, dataOffset, &binary))
			{
				EG_CORE_ERROR("Failed to deserialize texture 2D. Texture data is corrupted: {}", pathToAsset);
				return {};
			}

			if (auto node = baseDataNode["CompressedFormat"])
			{
				compressedFormat = Utils::GetEnumFromName<ImageFormat>(node.as<std::string>());
			}
			if (auto compressedNode = baseDataNode["Compressed"])
			{
				for (auto compressed : compressedNode)
				{
					const size_t size = compressed["Size"].as<size_t>();
					const size_t offset = compressed["Offset"].as<size_t>();

					auto& buffer = compressedTextures.emplace_back();
					if (!Utils::ReadBinary(data, size, offset, &buffer))
					{
						EG_CORE_WARN("Compressed mips of a texture are corrupted. They'll be regenerated: {}", pathToAsset);
						compressedTextures.clear();
						break;
					}
				}
			}
		}
		else
		{
			EG_CORE_ERROR("Failed to deserialize texture 2D: {}", pathToAsset);
			return {};
		}

		TextureCompressor::Result compressedData{};
		Ref<Texture2D> texture;
		if (compression != TextureCompressor::Quality::Disabled)
		{
			if (!compressedTextures.empty() && compressedFormat != ImageFormat::Unknown && TextureCompressor::IsCompressionFormatSupported(compressedFormat))
			{
				texture = Texture2D::Create(Utils::AsString(pathToAsset.stem()), compressedFormat, glm::uvec2(width, height), compressedTextures, specs);
			}
			else
			{
				// Try to compress it on the current system
				const uint32_t targetNumChannels = AssetTextureFormatToChannels(assetFormat, compression);
				compressedData = TextureCompressor::Compress(binary.GetDataBuffer(), targetNumChannels, specs.MipsCount, compression, bNormalMap);
				if (compressedData)
				{
					texture = Texture2D::Create(Utils::AsString(pathToAsset.stem()), compressedData.Format, glm::uvec2(width, height), compressedData.DataPerMip, specs);
				}
				else
				{
					EG_CORE_ERROR("Failed to load the compressed texture. Falling back to loading raw data: {}", pathToAsset);
					compression = TextureCompressor::Quality::Disabled;
				}
			}
		}

		// If non-compressed requested or compression failed, simply upload the raw data
		if (compression == TextureCompressor::Quality::Disabled)
		{
			const int desiredChannels = AssetTextureFormatToChannels(assetFormat, compression);
			ScopedDataBuffer imageData = Utils::LoadTextureFromMemory(binary, &width, &height, &channels, desiredChannels);
			if (!imageData)
			{
				EG_CORE_ERROR("Deserialization failed. `LoadTextureFromMemory` failed: {}", pathToAsset);
				return {};
			}

			const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);
			texture = Texture2D::Create(Utils::AsString(pathToAsset.stem()), imageFormat, glm::uvec2(width, height), imageData.Data(), specs);
		}

		class LocalAssetTexture2D : public AssetTexture2D
		{
		public:
			LocalAssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData, std::vector<ScopedDataBuffer>&& compressedDataPerMip,
				const Ref<Texture2D>& texture, AssetTexture2DFormat format, TextureCompressor::Quality compression, bool bNormalMap)
				: AssetTexture2D(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, rawData, std::move(compressedDataPerMip), texture, format, compression, bNormalMap) {}
		};

		Ref<AssetTexture2D> asset = MakeRef<LocalAssetTexture2D>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds,
			binary.GetDataBuffer(), std::move(compressedData.DataPerMip), texture, assetFormat, compression, bNormalMap);

		return asset;
	}

	Ref<AssetTextureCube> Serializer::DeserializeAssetTextureCube(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::TextureCube))
			return {}; // Failed

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;
		const AssetTextureCubeFormat assetFormat = Utils::GetEnumFromName<AssetTextureCubeFormat>(baseNode["Format"].as<std::string>());
		const uint32_t layerSize = baseNode["LayerSize"].as<uint32_t>();
		uint32_t prefilterSize = layerSize;
		bool bCompress = true;
		if (auto prefilterNode = baseNode["PrefilterSize"])
			prefilterSize = prefilterNode.as<uint32_t>();
		if (auto node = baseNode["Compress"])
			bCompress = node.as<bool>();

		ScopedDataBuffer binary;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				const size_t dataSize = baseDataNode["Size"].as<size_t>();
				const size_t dataOffset = baseDataNode["Offset"].as<size_t>();
				if (!Utils::ReadCompressedBinary(data, dataSize, dataOffset, &binary))
				{
					EG_CORE_ERROR("Failed to deserialize texture cube. Texture data is corrupted: {}", pathToAsset);
					return {};
				}
			}
		}

		int width, height, channels;
		const ImageFormat desiredFormat = AssetTextureFormatToImageFormat(assetFormat);
		ScopedDataBuffer imageData = Utils::LoadHDRTextureFromMemory(binary, &width, &height, &channels, desiredFormat);
		if (!imageData)
		{
			EG_CORE_ERROR("Import failed. LoadHDRTextureFromMemory failed: {} - {}", pathToAsset, Utils::GetEnumName(assetFormat));
			return {};
		}

		class LocalAssetTextureCube : public AssetTextureCube
		{
		public:
			LocalAssetTextureCube(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData, const Ref<TextureCube>& texture, AssetTextureCubeFormat format)
				: AssetTextureCube(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, rawData, texture, format) {}
		};

		Ref<AssetTextureCube> asset = MakeRef<LocalAssetTextureCube>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, binary.GetDataBuffer(),
			TextureCube::Create(Utils::AsString(pathToAsset.stem()), desiredFormat, imageData.Data(), glm::uvec2(width, height), layerSize, prefilterSize, bCompress), assetFormat);

		return asset;
	}

	Ref<AssetStaticMesh> Serializer::DeserializeAssetStaticMesh(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		class LocalAssetMesh : public AssetStaticMesh
		{
		public:
			LocalAssetMesh(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<StaticMesh>& mesh)
				: AssetStaticMesh(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, mesh) {}
		};

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::StaticMesh))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		bool bCombinedMesh = true;
		if (auto node = baseNode["CombinedMesh"])
			bCombinedMesh = node.as<bool>();

		std::string sourceMeshName;
		if (auto node = baseNode["SourceMeshName"])
			sourceMeshName = node.as<std::string>();

		uint32_t sourceMeshIndex = 0;
		if (auto node = baseNode["SourceMeshIndex"])
			sourceMeshIndex = node.as<uint32_t>();

		bool bResetLocation = false;
		if (auto node = baseNode["ResetLocation"])
			bResetLocation = node.as<bool>();

		if (bReloadRaw)
		{
			std::vector<Utils::StaticMeshImportData> importedMeshes = Utils::ImportStaticMesh(pathToRaw, bCombinedMesh, bResetLocation);
			if (importedMeshes.empty())
			{
				EG_CORE_ERROR("Failed to reload a mesh asset: {}", pathToRaw);
				return {};
			}

			// When the asset isn't a whole-file combine, it corresponds to one specific mesh in `importedMeshes`.
			// Match it by name first, falling back to the recorded index if the name isn't found
			// (for example, the mesh got renamed since the last import).
			Utils::StaticMeshImportData* match = &importedMeshes[0];
			if (!bCombinedMesh)
			{
				match = nullptr;
				if (!sourceMeshName.empty())
				{
					for (auto& importedMesh : importedMeshes)
					{
						if (importedMesh.Name == sourceMeshName)
						{
							match = &importedMesh;
							break;
						}
					}
				}
				if (!match && sourceMeshIndex < importedMeshes.size())
					match = &importedMeshes[sourceMeshIndex];

				if (!match)
				{
					EG_CORE_ERROR("Failed to find source mesh '{}' while reloading '{}'. It might've been renamed, removed, or reordered in the source file", sourceMeshName, pathToRaw);
					return {};
				}
			}

			Ref<AssetStaticMesh> asset = MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, match->Mesh);
			asset->SetSourceMeshData(bCombinedMesh, sourceMeshName, sourceMeshIndex, bResetLocation);
			return asset;
		}

		AABB aabb{};
		if (auto aabbNode = baseNode["AABB"])
		{
			aabb.Min = aabbNode["Min"].as<glm::vec3>();
			aabb.Max = aabbNode["Max"].as<glm::vec3>();
		}

		auto materialsNode = baseNode["Materials"];

		std::vector<Vertex> vertices;
		std::vector<std::vector<Index>> indicesPerMaterial;

		if (auto baseDataNode = baseNode["Data"])
		{
			// Vertices
			{
				const size_t verticesSize = baseDataNode["VerticesSize"].as<size_t>();
				const size_t verticesOffset = baseDataNode["VerticesOffset"].as<size_t>();
				if (!Utils::ReadCompressedBinary(data, verticesSize, verticesOffset, &vertices))
				{
					EG_CORE_ERROR("Failed to deserialize a mesh. Vertex data is corrupted: {}", pathToAsset);
					return {};
				}
			}

			// Indices
			{
				auto indicesPerMaterialNode = baseDataNode["IndicesPerMaterial"];
				for (const auto& node : indicesPerMaterialNode)
				{
					const size_t indicesSize = node["IndicesSize"].as<size_t>();
					const size_t indicesOffset = node["IndicesOffset"].as<size_t>();

					auto& indices = indicesPerMaterial.emplace_back();
					if (!Utils::ReadCompressedBinary(data, indicesSize, indicesOffset, &indices))
					{
						EG_CORE_ERROR("Failed to deserialize a mesh. Index data is corrupted: {}", pathToAsset);
						return {};
					}
				}
			}
		}

		Ref<StaticMesh> staticMesh = StaticMesh::Create(vertices, indicesPerMaterial, aabb);
		if (materialsNode)
			for (const auto& matNode : materialsNode)
				staticMesh->SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));

		Ref<AssetStaticMesh> asset = MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, staticMesh);
		asset->SetSourceMeshData(bCombinedMesh, sourceMeshName, sourceMeshIndex, bResetLocation);
		return asset;
	}

	Ref<AssetSkeletalMesh> Serializer::DeserializeAssetSkeletalMesh(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetMesh : public AssetSkeletalMesh
		{
		public:
			LocalAssetMesh(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<SkeletalMesh>& mesh)
				: AssetSkeletalMesh(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, mesh) {}
		};

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::SkeletalMesh))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		bool bCombinedMesh = true;
		if (auto node = baseNode["CombinedMesh"])
			bCombinedMesh = node.as<bool>();

		std::string sourceMeshName;
		if (auto node = baseNode["SourceMeshName"])
			sourceMeshName = node.as<std::string>();

		uint32_t sourceMeshIndex = 0;
		if (auto node = baseNode["SourceMeshIndex"])
			sourceMeshIndex = node.as<uint32_t>();

		bool bResetLocation = false;
		if (auto node = baseNode["ResetLocation"])
			bResetLocation = node.as<bool>();

		if (bReloadRaw)
		{
			std::vector<Utils::SkeletalMeshImportData> importedMeshes = Utils::ImportSkeletalMesh(pathToRaw, bCombinedMesh, bResetLocation);
			if (importedMeshes.empty())
			{
				EG_CORE_ERROR("Failed to reload a skeletal mesh asset: {}", pathToRaw);
				return {};
			}

			Utils::SkeletalMeshImportData* match = &importedMeshes[0];
			if (!bCombinedMesh)
			{
				match = nullptr;
				if (!sourceMeshName.empty())
				{
					for (auto& importedMesh : importedMeshes)
					{
						if (importedMesh.Name == sourceMeshName)
						{
							match = &importedMesh;
							break;
						}
					}
				}
				if (!match && sourceMeshIndex < importedMeshes.size())
					match = &importedMeshes[sourceMeshIndex];

				if (!match)
				{
					EG_CORE_WARN("Failed to find source mesh '{}' while reloading '{}'. It might've been renamed, removed, or reordered in the source file", sourceMeshName, pathToRaw);
					return {};
				}
			}

			Ref<AssetSkeletalMesh> asset = MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, match->Mesh);
			asset->SetSourceMeshData(bCombinedMesh, sourceMeshName, sourceMeshIndex, bResetLocation);
			return asset;
		}

		AABB aabb{};
		if (auto aabbNode = baseNode["AABB"])
		{
			aabb.Min = aabbNode["Min"].as<glm::vec3>();
			aabb.Max = aabbNode["Max"].as<glm::vec3>();
		}

		float minRagdollBoneSize = 0.1f;
		float maxRagdollTwist = 22.5f;
		float maxRagdollSwing = 45.0f;
		CollisionDetectionType collisionDetectionType = CollisionDetectionType::Discrete;
		CollisionGroup collisionGroup = s_DefaultCollisionGroup;
		CollisionGroup interactingCollisionGroup = s_DefaultInteractingCollisionGroup;

		if (auto node = baseNode["MinRagdollBoneSize"])
			minRagdollBoneSize = node.as<float>();
		if (auto node = baseNode["MaxRagdollTwist"])
			maxRagdollTwist = node.as<float>();
		if (auto node = baseNode["MaxRagdollSwing"])
			maxRagdollSwing = node.as<float>();
		if (auto node = baseNode["CollisionDetectionType"])
			collisionDetectionType = Utils::GetEnumFromName<CollisionDetectionType>(node.as<std::string>());

		const uint32_t collisionGroupValidMasks = Serializer::DeserializeProjectCollisionGroupGUIDs(baseNode);

		if (auto node = baseNode["CollisionGroupMask"])
			collisionGroup = CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks);
		if (auto node = baseNode["InteractingCollisionGroupMask"])
			interactingCollisionGroup = CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks);

		std::unordered_map<std::string, SkeletalRagdollBone::UserSettings> ragdollPerBoneData;
		if (auto node = baseNode["RagdollBonesData"])
		{
			ragdollPerBoneData.reserve(node.size());
			for (const auto& dataNode : node)
			{
				auto& data = ragdollPerBoneData[dataNode["Name"].as<std::string>()];
				data.UserOffset = Math::DecomposeTransformMatrix(dataNode["Offset"].as<glm::mat4>());
				if (auto iterationsNode = dataNode["PositionSolverIterations"])
					data.PositionSolverIterations = iterationsNode.as<uint32_t>();
				if (auto iterationsNode = dataNode["VelocitySolverIterations"])
					data.VelocitySolverIterations = iterationsNode.as<uint32_t>();
				data.LinearDamping = dataNode["LinearDamping"].as<float>();
				data.AngularDamping = dataNode["AngularDamping"].as<float>();
				if (auto simulateNode = dataNode["bEnableSimulation"])
					data.bEnableSimulation = simulateNode.as<bool>();
				if (auto collisionNode = dataNode["bEnableCollision"])
					data.bEnableCollision = collisionNode.as<bool>();
				data.Mass = dataNode["Mass"].as<float>();
				data.Material = GetAsset<AssetPhysicsMaterial>(dataNode["Material"]);
				data.Shape = Utils::GetEnumFromName<SkeletalRagdollBone::UserSettings::ShapeType>(dataNode["Shape"].as<std::string>());
			}
		}

		SkeletalMeshInfo skeletalInfo;
		skeletalInfo.InverseTransform = baseNode["InverseTransform"].as<glm::mat4>();
		if (auto node = baseNode["CoordCorrection"])
			skeletalInfo.CoordCorrection = node.as<glm::mat4>();
		ReadBoneNode(baseNode["Skeletal"], skeletalInfo.RootBone);

		// BoneInfoMap
		{
			BonesMap boneMap;

			auto boneNodes = baseNode["BoneInfoMap"];
			for (const auto& boneNode : boneNodes)
			{
				std::string name = boneNode["Name"].as<std::string>();
				BoneInfo info;
				info.Offset = boneNode["Matrix"].as<glm::mat4>();
				info.BoneID = boneNode["ID"].as<int>();
				boneMap.emplace(std::move(name), std::move(info));
			}

			skeletalInfo.SetBonesInfoMap(std::move(boneMap));
		}

		std::vector<SkeletalVertex> vertices;
		std::vector<std::vector<Index>> indicesPerMaterial;

		if (auto baseDataNode = baseNode["Data"])
		{
			// Vertices
			{
				const size_t verticesSize = baseDataNode["VerticesSize"].as<size_t>();
				const size_t verticesOffset = baseDataNode["VerticesOffset"].as<size_t>();
				if (!Utils::ReadCompressedBinary(data, verticesSize, verticesOffset, &vertices))
				{
					EG_CORE_ERROR("Failed to deserialize a mesh. Vertex data is corrupted: {}", pathToAsset);
					return {};
				}
			}

			// Indices
			{
				auto indicesPerMaterialNode = baseDataNode["IndicesPerMaterial"];
				for (const auto& node : indicesPerMaterialNode)
				{
					const size_t indicesSize = node["IndicesSize"].as<size_t>();
					const size_t indicesOffset = node["IndicesOffset"].as<size_t>();

					auto& indices = indicesPerMaterial.emplace_back();
					if (!Utils::ReadCompressedBinary(data, indicesSize, indicesOffset, &indices))
					{
						EG_CORE_ERROR("Failed to deserialize a mesh. Index data is corrupted: {}", pathToAsset);
						return {};
					}
				}
			}
		}

		Ref<SkeletalMesh> skeletalMesh = SkeletalMesh::Create(vertices, indicesPerMaterial, skeletalInfo, aabb, ragdollPerBoneData,
			minRagdollBoneSize, maxRagdollTwist, maxRagdollSwing, collisionDetectionType, collisionGroup, interactingCollisionGroup);
		if (auto materialsNode = baseNode["Materials"])
		{
			for (const auto& matNode : materialsNode)
				skeletalMesh->SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));
		}

		Ref<AssetSkeletalMesh> asset = MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, skeletalMesh);
		asset->SetSourceMeshData(bCombinedMesh, sourceMeshName, sourceMeshIndex, bResetLocation);
		return asset;
	}

	Ref<AssetAudio> Serializer::DeserializeAssetAudio(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Audio))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		const float volume = baseNode["Volume"].as<float>();
		float pitch = 1.f;
		if (auto node = baseNode["Pitch"])
			pitch = node.as<float>();

		float pan = 0.f;
		if (auto node = baseNode["Pan"])
			pan = node.as<float>();

		Ref<AssetSoundGroup> soundGroup = GetAsset<AssetSoundGroup>(baseNode["SoundGroup"]);

		ScopedDataBuffer binary;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			if (!binary)
			{
				EG_CORE_ERROR("Failed to reload a raw asset: {}", pathToRaw);
				return {};
			}
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				const size_t dataSize = baseDataNode["Size"].as<size_t>();
				const size_t dataOffset = baseDataNode["Offset"].as<size_t>();
				Utils::ReadCompressedBinary(data, dataSize, dataOffset, &binary);
			}
			if (!binary)
			{
				EG_CORE_ERROR("Failed to load the asset: {}", pathToAsset);
				return {};
			}
		}

		class LocalAssetAudio : public AssetAudio
		{
		public:
			LocalAssetAudio(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData, const Ref<Audio>& audio, const Ref<AssetSoundGroup>& soundGroup)
				: AssetAudio(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, rawData, audio, soundGroup) {}
		};

		Ref<Audio> audio = Audio::Create(binary);
		if (!audio->IsValid())
		{
			if (bReloadRaw)
			{
				// Keep the currently loaded asset instead of replacing it with an unplayable one
				const std::string errorMessage = "Failed to reload an asset. Unsupported or corrupted audio file: " + Utils::AsString(pathToRaw);
				EG_CORE_ERROR("{}", errorMessage);
				Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
				return {};
			}

			// Still loaded, so that references to it stay valid and it can be reloaded from a fixed raw file
			EG_CORE_ERROR("Audio asset contains audio data that can't be decoded, unsupported or corrupted audio file: {}. Re-import it from a supported format", pathToAsset);
		}
		audio->SetVolume(volume);
		audio->SetPitch(pitch);
		audio->SetPan(pan);
		return MakeRef<LocalAssetAudio>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, binary.GetDataBuffer(), audio, soundGroup);
	}

	Ref<AssetFont> Serializer::DeserializeAssetFont(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetFont : public AssetFont
		{
		public:
			LocalAssetFont(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData, const Ref<Font>& font)
				: AssetFont(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, rawData, font) {
			}
		};

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Font))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;
		if (bReloadRaw)
		{
			ScopedDataBuffer binary = FileSystem::Read(pathToRaw);
			if (!binary)
			{
				EG_CORE_ERROR("Failed to reload a raw asset: {}", pathToRaw);
				return {};
			}

			return MakeRef<LocalAssetFont>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, binary.GetDataBuffer(), Font::Create(binary.GetDataBuffer(), Utils::AsString(pathToAsset.stem())));
		}
		else
		{
			ScopedDataBuffer binary;
			ScopedDataBuffer atlasBinary;
			glm::uvec2 size = glm::uvec2(0);

			if (auto baseDataNode = baseNode["Data"])
			{
				const size_t dataSize = baseDataNode["Size"].as<size_t>();
				const size_t dataOffset = baseDataNode["Offset"].as<size_t>();
				Utils::ReadCompressedBinary(data, dataSize, dataOffset, &binary);
			}
			if (!binary)
			{
				EG_CORE_ERROR("Failed to load the asset: {}", pathToAsset);
				return {};
			}

			bool bOutdatedAtlas = false;
			if (auto baseDataNode = baseNode["AtlasData"])
			{
				const uint32_t atlasVersion = baseDataNode["AtlasVersion"] ? baseDataNode["AtlasVersion"].as<uint32_t>() : 1u;
				bOutdatedAtlas = atlasVersion != Font::AtlasVersion;
				if (!bOutdatedAtlas)
				{
					const size_t dataSize = baseDataNode["Size"].as<size_t>();
					const size_t dataOffset = baseDataNode["Offset"].as<size_t>();
					size = baseDataNode["AtlasSize"].as<glm::uvec2>();

					Utils::ReadCompressedBinary(data, dataSize, dataOffset, &atlasBinary);
				}
			}

			if (atlasBinary)
			{
				return MakeRef<LocalAssetFont>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, binary.GetDataBuffer(), Font::Create(atlasBinary.GetDataBuffer(), size, binary.GetDataBuffer(), Utils::AsString(pathToAsset.stem())));
			}
			else
			{
				if (bOutdatedAtlas)
					EG_CORE_WARN("Font atlas of an asset is outdated. Regenerating it... Please, resave the asset: {}", pathToAsset);
				else
					EG_CORE_WARN("Failed to load font atlas from an asset. Generating it... Please, resave the asset: {}", pathToAsset);
				return MakeRef<LocalAssetFont>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, binary.GetDataBuffer(), Font::Create(binary.GetDataBuffer(), Utils::AsString(pathToAsset.stem())));
			}
		}
	}

	Ref<AssetMaterial> Serializer::DeserializeAssetMaterial(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Material))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		Ref<Material> material = Material::Create();

		material->SetAlbedoAsset(GetAsset<AssetTexture2D>(baseNode["AlbedoTexture"]));
		if (auto node = baseNode["Albedo"])
		{
			material->SetAlbedo(node.as<glm::vec3>());
			material->SetRawAlbedoUsed(baseNode["IsRawAlbedoUsed"].as<bool>());
		}

		material->SetMetalnessAsset(GetAsset<AssetTexture2D>(baseNode["MetalnessTexture"]));
		if (auto node = baseNode["Metalness"])
		{
			material->SetMetalness(node.as<float>());
			material->SetRawMetalnessUsed(baseNode["IsRawMetalnessUsed"].as<bool>());
		}
		if (auto node = baseNode["MetalnessTextureChannel"])
		{
			const Material::TextureChannel channel = Utils::GetEnumFromName<Material::TextureChannel>(node.as<std::string>());
			material->SetMetalnessTextureChannel(channel);
		}

		material->SetNormalAsset(GetAsset<AssetTexture2D>(baseNode["NormalTexture"]));

		material->SetRoughnessAsset(GetAsset<AssetTexture2D>(baseNode["RoughnessTexture"]));
		if (auto node = baseNode["Roughness"])
		{
			material->SetRoughness(node.as<float>());
			material->SetRawRoughnessUsed(baseNode["IsRawRoughnessUsed"].as<bool>());
		}
		if (auto node = baseNode["RoughnessTextureChannel"])
		{
			const Material::TextureChannel channel = Utils::GetEnumFromName<Material::TextureChannel>(node.as<std::string>());
			material->SetRoughnessTextureChannel(channel);
		}

		material->SetAOAsset(GetAsset<AssetTexture2D>(baseNode["AOTexture"]));
		if (auto node = baseNode["AO"])
		{
			material->SetAO(node.as<float>());
			material->SetRawAOUsed(baseNode["IsRawAOUsed"].as<bool>());
		}
		if (auto node = baseNode["AOTextureChannel"])
		{
			const Material::TextureChannel channel = Utils::GetEnumFromName<Material::TextureChannel>(node.as<std::string>());
			material->SetAOTextureChannel(channel);
		}

		material->SetEmissiveAsset(GetAsset<AssetTexture2D>(baseNode["EmissiveTexture"]));
		if (auto node = baseNode["Emissive"])
		{
			material->SetEmissive(node.as<glm::vec3>());
			material->SetRawEmissiveUsed(baseNode["IsRawEmissiveUsed"].as<bool>());
		}

		material->SetOpacityAsset(GetAsset<AssetTexture2D>(baseNode["OpacityTexture"]));
		if (auto node = baseNode["Opacity"])
		{
			material->SetOpacity(node.as<float>());
			material->SetRawOpacityUsed(baseNode["IsRawOpacityUsed"].as<bool>());
		}
		if (auto node = baseNode["OpacityTextureChannel"])
		{
			const Material::TextureChannel channel = Utils::GetEnumFromName<Material::TextureChannel>(node.as<std::string>());
			material->SetOpacityTextureChannel(channel);
		}

		material->SetOpacityMaskAsset(GetAsset<AssetTexture2D>(baseNode["OpacityMaskTexture"]));
		if (auto node = baseNode["OpacityMask"])
		{
			material->SetOpacityMask(node.as<float>());
			material->SetRawOpacityMaskUsed(baseNode["IsRawOpacityMaskUsed"].as<bool>());
		}
		if (auto node = baseNode["OpacityMaskTextureChannel"])
		{
			const Material::TextureChannel channel = Utils::GetEnumFromName<Material::TextureChannel>(node.as<std::string>());
			material->SetOpacityMaskTextureChannel(channel);
		}

		if (auto node = baseNode["TintColor"])
			material->SetTintColor(node.as<glm::vec4>());

		if (auto node = baseNode["EmissiveIntensity"])
			material->SetEmissiveIntensity(node.as<glm::vec3>());

		if (auto node = baseNode["TilingFactor"])
		{
			const glm::vec2 tiling = node.IsScalar() ? glm::vec2(node.as<float>()) : node.as<glm::vec2>();
			material->SetTilingFactor(tiling);
		}

		if (auto node = baseNode["BlendMode"])
			material->SetBlendMode(Utils::GetEnumFromName<MaterialBlendMode>(node.as<std::string>()));

		if (auto node = baseNode["IsDoubleSided"])
			material->SetDoubleSided(node.as<bool>());

		class LocalAssetMaterial : public AssetMaterial
		{
		public:
			LocalAssetMaterial(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<Material>& material)
				: AssetMaterial(path, guid, bAlwaysIncludeInGameBuilds, material) {}
		};

		return MakeRef<LocalAssetMaterial>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, material);
	}

	Ref<AssetPhysicsMaterial> Serializer::DeserializeAssetPhysicsMaterial(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::PhysicsMaterial))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		float staticFriction = 0.6f;
		float dynamicFriction = 0.6f;
		float bounciness = 0.5f;

		if (auto node = baseNode["StaticFriction"])
			staticFriction = node.as<float>();

		if (auto node = baseNode["DynamicFriction"])
			dynamicFriction = node.as<float>();

		if (auto node = baseNode["Bounciness"])
			bounciness = node.as<float>();

		class LocalAssetPhysicsMaterial : public AssetPhysicsMaterial
		{
		public:
			LocalAssetPhysicsMaterial(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<PhysicsMaterial>& material)
				: AssetPhysicsMaterial(path, guid, bAlwaysIncludeInGameBuilds, material) {}
		};

		return MakeRef<LocalAssetPhysicsMaterial>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, PhysicsMaterial::Create(staticFriction, dynamicFriction, bounciness));
	}

	Ref<AssetSoundGroup> Serializer::DeserializeAssetSoundGroup(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::SoundGroup))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;
		Ref<SoundGroup> soundGroup = SoundGroup::Create();

		const float volume = baseNode["Volume"].as<float>();
		const float pitch = baseNode["Pitch"].as<float>();
		const bool bPaused = baseNode["IsPaused"].as<bool>();
		const bool bMuted = baseNode["IsMuted"].as<bool>();
		soundGroup->SetVolume(volume);
		soundGroup->SetPitch(pitch);
		soundGroup->SetPaused(bPaused);
		soundGroup->SetMuted(bMuted);

		class LocalAssetSoundGroup : public AssetSoundGroup
		{
		public:
			LocalAssetSoundGroup(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<SoundGroup>& soundGroup)
				: AssetSoundGroup(path, guid, bAlwaysIncludeInGameBuilds, soundGroup) {}
		};

		return MakeRef<LocalAssetSoundGroup>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, soundGroup);
	}

	Ref<AssetEntity> Serializer::DeserializeAssetEntity(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Entity))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		Entity rootEntity;
		const auto& scene = AssetEntity::GetScene();
		if (auto entitiesNode = baseNode["Entities"])
		{
			//uint32_t - Entity's ID in the asset; Real entity ID; 
			std::unordered_map<uint32_t, Entity> allEntities;

			//uint32_t - entity that has an parent, uint32_t - parent id
			std::unordered_map<uint32_t, uint32_t> childs;

			const uint32_t collisionGroupValidMasks = Serializer::DeserializeProjectCollisionGroupGUIDs(baseNode);
			for (auto entityNode : entitiesNode)
			{
				uint32_t id;
				int parentID = -1;
				Entity deserializedEntity = Serializer::DeserializeEntity(scene, entityNode, collisionGroupValidMasks, &id, &parentID);

				allEntities[id] = deserializedEntity;
				if (parentID != -1)
				{
					childs[deserializedEntity.GetID()] = parentID;
				}
				else
				{
					EG_CORE_ASSERT(!rootEntity);
					rootEntity = deserializedEntity;
				}
			}

			for (const auto& element : childs)
			{
				Entity& parent = allEntities[element.second];
				Entity child((entt::entity)element.first, scene.get());
				child.SetParent(parent);
			}
		}
		else
		{
			rootEntity = AssetEntity::CreateEntity(guid);
		}

		class LocalAssetEntity: public AssetEntity
		{
		public:
			LocalAssetEntity(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<Entity>& entity)
				: AssetEntity(path, guid, bAlwaysIncludeInGameBuilds, entity) {}
		};

		return MakeRef<LocalAssetEntity>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, MakeRef<Entity>(rootEntity));
	}

	Ref<AssetAnimation> Serializer::DeserializeAssetAnimation(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetAnimation : public AssetAnimation
		{
		public:
			LocalAssetAnimation(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<SkeletalMeshAnimation>& anim, const Ref<AssetSkeletalMesh>& skeletal, uint32_t index)
				: AssetAnimation(path, pathToRaw, guid, bAlwaysIncludeInGameBuilds, anim, skeletal, index) {}
		};

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Animation))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + Utils::AsString(pathToRaw);
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const uint32_t animIndex = baseNode["Index"].as<uint32_t>();
		Ref<AssetSkeletalMesh> skeletal = GetAsset<AssetSkeletalMesh>(baseNode["Skeletal"]);
		if (!skeletal)
		{
			EG_CORE_ERROR("Failed to load {}. Its skeletal mesh wasn't found!", pathToAsset);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;
		if (bReloadRaw)
		{
			std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), GetRootMotionMode(baseNode));
			if (animIndex >= animations.size())
			{
				const std::string errorMessage = "Failed to reload an animation asset. The asset was initially imported at index " + 
					std::to_string(animIndex) + ", but now the file doesn't contains an animation at that index: " + Utils::AsString(pathToRaw);
				EG_CORE_ERROR("{}", errorMessage);
				Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
				return {};
			}

			return MakeRef<LocalAssetAnimation>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, MakeRef<SkeletalMeshAnimation>(std::move(animations[animIndex])), skeletal, animIndex);
		}

		SkeletalMeshAnimation animation;
		// Deserialize animation
		{
			const auto baseAnimNode = baseNode["Animation"];
			if (!baseAnimNode)
			{
				EG_CORE_ERROR("Failed to deserialize animation");
				return {};
			}

			animation.Duration = baseAnimNode["Duration"].as<float>();
			animation.TicksPerSecond = baseAnimNode["TicksPerSecond"].as<float>();
			if (auto inPlaceNode = baseAnimNode["bInPlace"])
				animation.bInPlace = inPlaceNode.as<bool>();

			// Root Motion
			if (auto rootMotionNode = baseAnimNode["RootMotion"])
			{
				if (auto rootMotionModeNode = baseAnimNode["RootMotionMode"])
				{
					animation.RootMotionType = Utils::GetEnumFromName<RootMotionMode>(rootMotionModeNode.as<std::string>());
				}

				// Locations
				{
					const size_t size = rootMotionNode["LocationsSize"].as<size_t>();
					const size_t offset = rootMotionNode["LocationsOffset"].as<size_t>();
					if (!Utils::ReadBinary(data, size, offset, &animation.RootMotion.Locations))
					{
						EG_CORE_ERROR("Failed to deserialize animation. Root motion data is corrupted: {}", pathToAsset);
						return {};
					}
				}

				// Rotations
				{
					const size_t size = rootMotionNode["RotationsSize"].as<size_t>();
					const size_t offset = rootMotionNode["RotationsOffset"].as<size_t>();
					if (!Utils::ReadBinary(data, size, offset, &animation.RootMotion.Rotations))
					{
						EG_CORE_ERROR("Failed to deserialize animation. Root motion data is corrupted: {}", pathToAsset);
						return {};
					}
				}

				// Scales
				{
					const size_t size = rootMotionNode["ScalesSize"].as<size_t>();
					const size_t offset = rootMotionNode["ScalesOffset"].as<size_t>();
					if (!Utils::ReadBinary(data, size, offset, &animation.RootMotion.Scales))
					{
						EG_CORE_ERROR("Failed to deserialize animation. Root motion data is corrupted: {}", pathToAsset);
						return {};
					}
				}

				// Pre RM locations
				{
					const size_t size = rootMotionNode["PreRMLocationsSize"].as<size_t>();
					const size_t offset = rootMotionNode["PreRMLocationsOffset"].as<size_t>();
					if (!Utils::ReadBinary(data, size, offset, &animation.PreRootMotionLocations))
					{
						EG_CORE_ERROR("Failed to deserialize animation. Root motion data is corrupted: {}", pathToAsset);
						return {};
					}
				}
			}

			// Events
			if (auto eventsNode = baseAnimNode["Events"])
			{
				for (const auto& eventNode : eventsNode)
				{
					auto& event = animation.Events.emplace_back();
					event.Name = eventNode["Name"].as<std::string>();
					event.Time = eventNode["Time"].as<float>();
				}
			}

			// Bones
			{
				BonesAnimMap animBones;
				const size_t bonesDataSize = baseAnimNode["BonesSize"].as<size_t>();
				const size_t bonesDataOffset = baseAnimNode["BonesOffset"].as<size_t>();
				ScopedDataBuffer bonesData;
				Utils::ReadBinary(data, bonesDataSize, bonesDataOffset, &bonesData);
				
				size_t offset = 0;
				while (offset < bonesDataSize)
				{
					BoneAnimation animData;
				
					const size_t nameSize = bonesData.Read<size_t>(offset);
					offset += sizeof(size_t);
					std::vector<char> boneNameVec(nameSize, 0);
				
					Utils::ReadBinary(bonesData.GetDataBuffer(), nameSize, offset, &boneNameVec);
					offset += nameSize;
					std::string_view boneName = boneNameVec.data();
				
					const size_t locationsSize = bonesData.Read<size_t>(offset);
					offset += sizeof(size_t);
					Utils::ReadBinary(bonesData.GetDataBuffer(), locationsSize, offset, &animData.Locations);
					offset += locationsSize;
				
					const size_t rotationsSize = bonesData.Read<size_t>(offset);
					offset += sizeof(size_t);
					Utils::ReadBinary(bonesData.GetDataBuffer(), rotationsSize, offset, &animData.Rotations);
					offset += rotationsSize;
				
					const size_t scalesSize = bonesData.Read<size_t>(offset);
					offset += sizeof(size_t);
					Utils::ReadBinary(bonesData.GetDataBuffer(), scalesSize, offset, &animData.Scales);
					offset += scalesSize;
				
					animData.BoneID = bonesData.Read<uint32_t>(offset);
					offset += sizeof(uint32_t);
				
					animBones.emplace(boneName, std::move(animData));
				}
				EG_CORE_ASSERT(offset == bonesDataSize);
				animation.SetAnimationBones(std::move(animBones));
			}
		}

		return MakeRef<LocalAssetAnimation>(pathToAsset, pathToRaw, guid, bAlwaysIncludeInGameBuilds, MakeRef<SkeletalMeshAnimation>(std::move(animation)), skeletal, animIndex);
	}

	Ref<AssetAnimationGraph> Serializer::DeserializeAssetAnimationGraph(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::AnimationGraph))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		auto mesh = GetAsset<AssetSkeletalMesh>(baseNode["SkeletalMesh"]);
		if (!mesh)
		{
			EG_CORE_ERROR("Failed to deserialize animation graph at {}. Skeletal mesh wasn't found", pathToAsset);
			return {};
		}

		auto graph = MakeRef<AnimationGraph>(mesh);
		GraphEditorSerializationData graphEditorData;

		// Variables
		if (auto variablesNode = baseNode["Variables"])
		{
			for (const auto& varNode : variablesNode)
			{
				auto& var = graphEditorData.Variables.emplace_back();
				var.Name = varNode["Name"].as<std::string>();
				var.Value = DeserializeGraphVar(varNode);
			}
		}

		if (auto graphNode = baseNode["Graph"])
			DeserializeGraph(graphNode, graphEditorData.Graph);

		class LocalAssetAnimationGraph : public AssetAnimationGraph
		{
		public:
			LocalAssetAnimationGraph(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<AnimationGraph>& graph, const GraphEditorSerializationData& data)
				: AssetAnimationGraph(path, guid, bAlwaysIncludeInGameBuilds, graph, data) {}
		};

		auto result = MakeRef<LocalAssetAnimationGraph>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, graph, graphEditorData);
		result->Compile();

		const auto& usedVars = result->GetGraph()->GetVariables();
		for (const auto& [name, oldVar] : graphEditorData.Variables)
		{
			auto it = usedVars.find(name);
			if (it == usedVars.end())
				continue; // Var is not present in the newly compiled graph. Ignore it

			auto& usedVar = it->second;
			if (usedVar->GetType() == oldVar->GetType())
				usedVar->bShowInUI = oldVar->bShowInUI;
		}

		return result;
	}

	Ref<AssetParticleSystem> Serializer::DeserializeAssetParticleSystem(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::ParticleSystem))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		std::vector<ParticleEmitter> emitters;
		auto emittersNode = baseNode["Emitters"];
		for (const auto& node : emittersNode)
		{
			auto& emitter = emitters.emplace_back();

			if (auto n = node["Name"])
				emitter.Name = n.as<std::string>();
			emitter.Texture = GetAsset<AssetTexture2D>(node["Texture"]);
			DeserializeCurveProperty(node, "Color", emitter.Color);
			DeserializeCurveProperty(node, "ColorIntensity", emitter.ColorIntensity);

			if (auto n = node["VelocityMin"])
				emitter.VelocityMin = n.as<glm::vec3>();
			if (auto n = node["VelocityMax"])
				emitter.VelocityMax = n.as<glm::vec3>();

			DeserializeCurveProperty(node, "VelocityCoef", emitter.VelocityCoef);
			DeserializeCurveProperty(node, "VelocityOverLifetime", emitter.VelocityOverLifetime);
			if (auto n = node["VelocitySpace"])
				emitter.VelocitySpace = Utils::GetEnumFromName<ParticleEmitter::VelocitySpaceType>(n.as<std::string>());

			DeserializeCurveProperty(node, "RotationZ", emitter.RotationZ);
			DeserializeCurveProperty(node, "RotationSpeed", emitter.RotationSpeed);
			DeserializeCurveProperty(node, "Size", emitter.Size);
			emitter.ColliderSizeRatio = node["ColliderSizeRatio"].as<glm::vec2>();

			emitter.LifetimeMin = node["LifetimeMin"].as<float>();
			emitter.LifetimeMax = node["LifetimeMax"].as<float>();

			emitter.BouncinessMin = node["BouncinessMin"].as<float>();
			emitter.BouncinessMax = node["BouncinessMax"].as<float>();

			if (auto n = node["AnimationImagesNum"])
				emitter.AnimationImagesNum = n.as<glm::uvec2>();
			if (auto n = node["AnimationSpeed"])
				emitter.AnimationSpeed = n.as<float>();

			emitter.ID = node["ID"].as<GUID>();
			emitter.RelativeTransform.Location = node["RelativeLocation"].as<glm::vec3>();
			emitter.RelativeTransform.Rotation = node["RelativeRotation"].as<Rotator>();
			emitter.VisibilityAABB.Min = node["VisibilityAABBMin"].as<glm::vec3>();
			emitter.VisibilityAABB.Max = node["VisibilityAABBMax"].as<glm::vec3>();
			if (auto n = node["LoopCount"])
				emitter.LoopCount = n.as<uint32_t>();
			if (auto n = node["LoopDuration"])
				emitter.LoopDuration = n.as<float>();
			if (auto n = node["SpawnRate"])
				emitter.SpawnRate = n.as<uint32_t>();
			emitter.FastForwardTo = node["FastForwardTo"].as<float>();
			if (auto n = node["StartDelay"])
				emitter.StartDelay = n.as<float>();
			if (auto n = node["RandomTint"])
				emitter.bRandomTint = n.as<bool>();
			if (auto n = node["RandomTintA"])
				emitter.RandomTintA = n.as<glm::vec4>();
			if (auto n = node["RandomTintB"])
				emitter.RandomTintB = n.as<glm::vec4>();
			if (auto n = node["StartRotationRandomRange"])
				emitter.StartRotationRandomRange = n.as<glm::vec2>();
			if (auto n = node["StartRotationSpeedRandomRange"])
				emitter.StartRotationSpeedRandomRange = n.as<glm::vec2>();
			if (auto n = node["StartSizeMultiplierRandomRange"])
				emitter.StartSizeMultiplierRandomRange = n.as<glm::vec2>();
			if (auto n = node["Drag"])
				DeserializeCurveProperty(node, "Drag", emitter.Drag);
			if (auto n = node["TurbulenceStrength"])
				DeserializeCurveProperty(node, "TurbulenceStrength", emitter.TurbulenceStrength);
			if (auto n = node["TurbulenceScale"])
				DeserializeCurveProperty(node, "TurbulenceScale", emitter.TurbulenceScale);
			if (auto n = node["TurbulenceSpeed"])
				emitter.TurbulenceSpeed = n.as<float>();
			if (auto n = node["InheritVelocity"])
				emitter.InheritVelocity = n.as<float>();
			if (auto n = node["SpawnPerMeter"])
				emitter.SpawnPerMeter = n.as<uint32_t>();
			if (auto n = node["SimulationSpace"])
				emitter.SimulationSpace = Utils::GetEnumFromName<ParticleEmitter::SimulationSpaceType>(n.as<std::string>());
			if (auto n = node["SpawnOnlyFromEvents"])
				emitter.bSpawnOnlyFromEvents = n.as<bool>();
			if (auto n = node["Culling"])
				emitter.Culling = Utils::GetEnumFromName<ParticleEmitter::CullingType>(n.as<std::string>());
			if (auto subEmittersNode = node["SubEmitters"])
			{
				for (const auto& subEmitterNode : subEmittersNode)
				{
					if (emitter.SubEmitters.size() >= ParticleEmitter::MaxSubEmitters)
						break;

					auto& subEmitter = emitter.SubEmitters.emplace_back();
					if (auto n = subEmitterNode["EmitterID"])
						subEmitter.EmitterID = n.as<GUID>();
					if (auto n = subEmitterNode["Trigger"])
						subEmitter.Trigger = Utils::GetEnumFromName<ParticleEmitter::SubEmitterTrigger>(n.as<std::string>());
					if (auto n = subEmitterNode["UseEmitterSpawnSettings"])
						subEmitter.bUseEmitterSpawnSettings = n.as<bool>();
					if (auto n = subEmitterNode["CountRange"])
						subEmitter.CountRange = n.as<glm::uvec2>();
					if (auto n = subEmitterNode["Probability"])
						subEmitter.Probability = n.as<float>();
					if (auto n = subEmitterNode["InheritVelocity"])
						subEmitter.InheritVelocity = n.as<float>();
					if (auto n = subEmitterNode["InheritColor"])
						subEmitter.bInheritColor = n.as<bool>();
				}
			}
			emitter.RadialAcceleration = node["RadialAcceleration"].as<float>();
			emitter.TangentialAcceleration = node["TangentialAcceleration"].as<float>();
			if (auto n = node["NormalVelocityFactor"])
				emitter.NormalVelocityFactor = n.as<float>();
			emitter.EmissionShape = Utils::GetEnumFromName<ParticleEmitter::EmissionShapeType>(node["EmissionShape"].as<std::string>());
			if (auto n = node["SphereRadius"])
				emitter.SphereRadius = n.as<glm::vec3>();
			if (auto n = node["BoxMin"])
				emitter.BoxMin = n.as<glm::vec3>();
			if (auto n = node["BoxMax"])
				emitter.BoxMax = n.as<glm::vec3>();
			if (auto n = node["RingRadius"])
				emitter.RingRadius = n.as<glm::vec3>();
			if (auto n = node["RingThickness"])
				emitter.RingThickness = n.as<glm::vec3>();
			if (auto n = node["Mesh"])
				emitter.MeshAsset = GetAsset<AssetBaseMesh>(n);

			if (auto animationNode = node["AnimationClip"])
				emitter.MeshAnimationAsset = GetAsset<AssetAnimation>(animationNode);
			if (auto nodeSpeed = node["ClipPlaybackSpeed"])
				emitter.ClipPlaybackSpeed = nodeSpeed.as<float>();
			if (auto nodeLooping = node["ClipLooping"])
				emitter.bClipLooping = nodeLooping.as<bool>();
			if (auto nodeAnimEvents = node["TriggerAnimationEvents"])
				emitter.bTriggerAnimationEvents = nodeAnimEvents.as<bool>();

			emitter.CollisionMode = Utils::GetEnumFromName<ParticleEmitter::CollisionModeType>(node["CollisionMode"].as<std::string>());
			if (auto n = node["bDestroyImmediately"])
				emitter.bDestroyImmediately = n.as<bool>();
			emitter.bEmit = node["bEmit"].as<bool>();
			emitter.bExplode = node["bExplode"].as<bool>();
			if (auto n = node["DepthFade"])
				emitter.bDepthFade = n.as<bool>();
			if (auto n = node["DepthFadeDistance"])
				emitter.DepthFadeDistance = n.as<float>();
			if (auto n = node["CameraFade"])
				emitter.bCameraFade = n.as<bool>();
			if (auto n = node["CameraFadeDistance"])
				emitter.CameraFadeDistance = n.as<glm::vec2>();
			if (auto n = node["ApplyGravity"])
				DeserializeCurveProperty(node, "ApplyGravity", emitter.ApplyGravity);
			emitter.bAlphaBlending = node["bAlphaBlending"].as<bool>();
			if (auto n = node["bAdditive"])
				emitter.bAdditive = n.as<bool>();
			if (auto n = node["bBlendAnimation"])
				emitter.bBlendAnimation = n.as<bool>();
			if (auto n = node["bFaceDirection"])
				emitter.bFaceDirection = n.as<bool>();
		}

		class LocalAssetParticleSystem : public AssetParticleSystem
		{
		public:
			LocalAssetParticleSystem(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const std::vector<ParticleEmitter>& emitters)
				: AssetParticleSystem(path, guid, bAlwaysIncludeInGameBuilds, emitters) {}
		};

		return MakeRef<LocalAssetParticleSystem>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, emitters);
	}

	Ref<AssetAnimationBlendSpace> Serializer::DeserializeAssetAnimationBlendSpace(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::AnimationBlendSpace))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		auto mesh = GetAsset<AssetSkeletalMesh>(baseNode["SkeletalMesh"]);
		if (!mesh)
		{
			EG_CORE_ERROR("Failed to deserialize animation blend space at {}. Skeletal mesh wasn't found", pathToAsset);
			return {};
		}

		BlendSpaceEventsTriggerMode mode = BlendSpaceEventsTriggerMode::HighestWeightedAnimation;
		if (auto modeNode = baseNode["EventsTriggerMode"])
			mode = Utils::GetEnumFromName<BlendSpaceEventsTriggerMode>(modeNode.as<std::string>());

		float blendTime = 0.1f;
		if (auto blendNode = baseNode["BlendTime"])
			blendTime = blendNode.as<float>();

		bool bSyncEnabled = true;
		if (auto syncNode = baseNode["SyncEnabled"])
			bSyncEnabled = syncNode.as<bool>();

		bool bUseShortestBlendPath = true;
		if (auto blendPathNode = baseNode["UseShortestBlendPath"])
			bUseShortestBlendPath = blendPathNode.as<bool>();

		BlendSpaceAxisSettings horAxis;
		horAxis.Name = "Horizontal Axis";
		if (auto horNode = baseNode["HorizontalAxis"])
		{
			horAxis.Name = horNode["Name"].as<std::string>();
			horAxis.Min = horNode["Min"].as<float>();
			horAxis.Max = horNode["Max"].as<float>();
		}

		BlendSpaceAxisSettings verAxis;
		verAxis.Name = "Vertical Axis";
		if (auto verNode = baseNode["VerticalAxis"])
		{
			verAxis.Name = verNode["Name"].as<std::string>();
			verAxis.Min = verNode["Min"].as<float>();
			verAxis.Max = verNode["Max"].as<float>();
		}

		std::vector<BlendSpaceVertex> points;
		auto pointNodes = baseNode["Points"];
		if (pointNodes)
		{
			points.reserve(pointNodes.size());
			for (auto node : pointNodes)
			{
				auto& point = points.emplace_back();
				point.Animation = GetAsset<AssetAnimation>(node["Animation"]);
				point.Coord = node["Coord"].as<glm::dvec2>();
				if (auto speedNode = node["AnimSpeed"])
					point.AnimSpeed = speedNode.as<float>();
			}
		}

		class LocalAssetAnimationBlendSpace : public AssetAnimationBlendSpace
		{
		public:
			LocalAssetAnimationBlendSpace(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const Ref<AssetSkeletalMesh>& skeletal, const BlendSpaceAxisSettings& horAxis, const BlendSpaceAxisSettings& verAxis,
				const std::vector<BlendSpaceVertex>& pointsData, BlendSpaceEventsTriggerMode mode)
				: AssetAnimationBlendSpace(path, guid, bAlwaysIncludeInGameBuilds, skeletal, horAxis, verAxis, pointsData, mode) {
			}
		};

		auto result = MakeRef<LocalAssetAnimationBlendSpace>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, mesh, horAxis, verAxis, points, mode);
		result->SetBlendTime(blendTime);
		result->SetSyncEnabled(bSyncEnabled);
		result->SetUseShortestBlendPath(bUseShortestBlendPath);

		return result;
	}

	Ref<AssetScene> Serializer::DeserializeAssetScene(const DataBuffer& data, const Path& pathToAsset)
	{
		class LocalAssetScene : public AssetScene
		{
		public:
			LocalAssetScene(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds)
				: AssetScene(path, guid, bAlwaysIncludeInGameBuilds) {
			}
		};

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Scene))
			return {};

		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;
		return MakeRef<LocalAssetScene>(pathToAsset, baseNode["GUID"].as<GUID>(), bAlwaysIncludeInGameBuilds);
	}

	Ref<AssetBehaviorGraph> Serializer::DeserializeAssetBehaviorGraph(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::BehaviorGraph))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		const bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		AIBehaviorNode rootNode;
		if (auto node = baseNode["Nodes"])
			DeserializeAIBehaviorNode(node, rootNode);

		GraphEditorSerializationData graphEditorData;
		if (auto graphNode = baseNode["Graph"])
			DeserializeGraph(graphNode, graphEditorData.Graph);

		class LocalAssetBehaviorGraph : public AssetBehaviorGraph
		{
		public:
			LocalAssetBehaviorGraph(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, AIBehaviorNode&& root, GraphEditorSerializationData&& data)
				: AssetBehaviorGraph(path, guid, bAlwaysIncludeInGameBuilds, std::move(root), std::move(data)) {
			}
		};

		return MakeRef<LocalAssetBehaviorGraph>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, std::move(rootNode), std::move(graphEditorData));
	}

	Ref<AssetSceneSequence> Serializer::DeserializeAssetSceneSequence(const DataBuffer& data, const Path& pathToAsset)
	{
		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::SceneSequence))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		bool bAlwaysIncludeInGameBuilds = baseNode["AlwaysIncludeInGameBuilds"] ? baseNode["AlwaysIncludeInGameBuilds"].as<bool>() : false;

		float duration = 5.f;
		float frameRate = 30.f;
		bool bLooping = false;

		if (auto node = baseNode["Duration"])
			duration = node.as<float>();

		if (auto node = baseNode["FrameRate"])
			frameRate = node.as<float>();

		if (auto node = baseNode["Looping"])
			bLooping = node.as<bool>();

		std::vector<Ref<SequenceTrack>> tracks;
		if (auto tracksNode = baseNode["Tracks"])
		{
			tracks.reserve(tracksNode.size());
			for (auto trackNode : tracksNode)
			{
				if (Ref<SequenceTrack> track = DeserializeSequenceTrack(trackNode))
				{
					tracks.push_back(std::move(track));
				}
				else
				{
					EG_CORE_ERROR("Failed to deserialize a sequence track");
				}
			}
		}

		class LocalAssetSceneSequence : public AssetSceneSequence
		{
		public:
			LocalAssetSceneSequence(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, std::vector<Ref<SequenceTrack>>&& tracks, float duration, float frameRate, bool bLooping)
				: AssetSceneSequence(path, guid, bAlwaysIncludeInGameBuilds, std::move(tracks), duration, frameRate, bLooping) {
			}
		};

		return MakeRef<LocalAssetSceneSequence>(pathToAsset, guid, bAlwaysIncludeInGameBuilds, std::move(tracks), duration, frameRate, bLooping);
	}
}
