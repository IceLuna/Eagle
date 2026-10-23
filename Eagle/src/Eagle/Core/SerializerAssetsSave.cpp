#include "egpch.h"
#include "Serializer.h"
#include "SerializerInternal.h"

#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Classes/Font.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/AssimpImporter.h"
#include "Eagle/Utils/SerializerUtils.h"

namespace Eagle
{
	static void SerializeAIBehaviorNode(YAML::Emitter& out, const AIBehaviorNode& node)
	{
		out << YAML::Key << "FullName" << YAML::Value << node.Data.ClassData.FullName;
		out << YAML::Key << "ID" << YAML::Value << node.Data.ID;

		if (!node.Data.ClassData.Fields.empty())
		{
			SerializeScriptFields(out, node.Data.ClassData.Fields);
		}

		if (!node.AttachedDecorators.empty())
		{
			out << YAML::Key << "Decorators" << YAML::Value << YAML::BeginSeq;
			for (const auto& decorator : node.AttachedDecorators)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "FullName" << YAML::Value << decorator.ClassData.FullName;
				if (!decorator.ClassData.Fields.empty())
				{
					SerializeScriptFields(out, decorator.ClassData.Fields);
				}
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}

		if (!node.Children.empty())
		{
			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
			for (const auto& child : node.Children)
			{
				out << YAML::BeginMap;
				SerializeAIBehaviorNode(out, child);
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}
	}

	static void SerializeGraph(YAML::Emitter& out, const GraphSerializationData& data)
	{
		out << YAML::Key << "Name" << YAML::Value << data.Name;
		out << YAML::Key << "Scroll" << YAML::Value << data.ScrollOffset;
		out << YAML::Key << "ID" << YAML::Value << data.ID;
		out << YAML::Key << "Zoom" << YAML::Value << data.Zoom;

		out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
		for (const auto& node : data.Nodes)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "OwnerID" << YAML::Value << node.OwnerID;
			out << YAML::Key << "Name" << YAML::Value << node.Name;
			out << YAML::Key << "Position" << YAML::Value << node.Position;
			out << YAML::Key << "Size" << YAML::Value << node.Size;
			out << YAML::Key << "NodeID" << YAML::Value << node.NodeID;
			out << YAML::Key << "IsOutputNode" << YAML::Value << node.bOutputNode;
			out << YAML::Key << "CachedOwnerID" << YAML::Value << node.CachedOwnerID;
			out << YAML::Key << "CachedNodeID" << YAML::Value << node.CachedNodeID;
			out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(node.Type);
			if (node.Type == GraphNodeType::BlendSpace && node.BlendSpace)
				out << YAML::Key << "BlendSpace" << YAML::Value << node.BlendSpace->GetGUID();
			out << YAML::Key << "AddedCounter" << YAML::Value << node.AddedCounter;
			if (node.UserData.empty() == false)
				out << YAML::Key << "UserData" << YAML::Value << node.UserData;
			if (node.Type == GraphNodeType::AIBehaviorNode)
			{
				AIBehaviorNode behaviorNode;
				behaviorNode.Data = node.BehaviorClassData;
				behaviorNode.AttachedDecorators = node.AttachedDecorators;

				out << YAML::Key << "BehaviorNodeData" << YAML::Value << YAML::BeginMap;
				SerializeAIBehaviorNode(out, behaviorNode);
				out << YAML::EndMap;
			}

			{
				out << YAML::Key << "InputPins" << YAML::Value << YAML::BeginSeq;
				for (const auto& pin : node.InputPins)
				{
					out << YAML::BeginMap;
					out << YAML::Key << "ID" << YAML::Value << pin.Index;
					if (pin.DefaultValue)
					{
						out << YAML::Key << "DefaultValue" << YAML::Value << YAML::BeginMap;
						SerializeGraphVar(out, pin.DefaultValue);
						out << YAML::EndMap;
					}
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}

			out << YAML::Key << "Connections" << YAML::Value << YAML::BeginSeq;
			for (const auto& connection : node.OutputConnections)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "NodeID" << YAML::Value << connection.NodeID;
				out << YAML::Key << "PinIndex" << YAML::Value << connection.PinIndex;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		if (data.Subgraphs.size())
		{
			out << YAML::Key << "Subgraphs" << YAML::BeginSeq;

			for (const auto& subgraph : data.Subgraphs)
			{
				out << YAML::BeginMap;
				SerializeGraph(out, subgraph);
				out << YAML::EndMap;
			}

			out << YAML::EndSeq;
		}
	}

	static void SerializeRagdollBonesData(YAML::Emitter& out, const SkeletalRagdollBone& node)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << node.Name;
		out << YAML::Key << "Offset" << YAML::Value << Math::ToTransformMatrix(node.Settings.UserOffset);
		out << YAML::Key << "PositionSolverIterations" << YAML::Value << node.Settings.PositionSolverIterations;
		out << YAML::Key << "VelocitySolverIterations" << YAML::Value << node.Settings.VelocitySolverIterations;
		out << YAML::Key << "LinearDamping" << YAML::Value << node.Settings.LinearDamping;
		out << YAML::Key << "AngularDamping" << YAML::Value << node.Settings.AngularDamping;
		out << YAML::Key << "bEnableSimulation" << YAML::Value << node.Settings.bEnableSimulation;
		out << YAML::Key << "bEnableCollision" << YAML::Value << node.Settings.bEnableCollision;
		out << YAML::Key << "Mass" << YAML::Value << node.Settings.Mass;
		out << YAML::Key << "Shape" << YAML::Value << Utils::GetEnumName(node.Settings.Shape);
		if (node.Settings.Material)
			out << YAML::Key << "Material" << YAML::Value << node.Settings.Material->GetGUID();
		out << YAML::EndMap;

		for (const auto& child : node.Children)
			SerializeRagdollBonesData(out, child);
	}

	// Keyframe channels are all serialized through these two helpers, so adding a new
	// channel value type only needs a `YAML::convert` specialization for that type.
	template <typename T>
	static void SerializeCurve(YAML::Emitter& out, const char* name, const Curve<T>& channel)
	{
		if (channel.IsEmpty())
			return;

		out << YAML::Key << name << YAML::Value << YAML::BeginSeq;
		for (const auto& key : channel.GetKeys())
		{
			out << YAML::BeginMap;
			out << YAML::Key << "ID" << YAML::Value << key.ID;
			out << YAML::Key << "Time" << YAML::Value << key.Time;
			out << YAML::Key << "Value" << YAML::Value << key.Value;
			out << YAML::Key << "Interpolation" << YAML::Value << Eagle::Utils::GetEnumName(key.Interpolation);

			// Tangents only mean anything for user-authored cubic keys
			if (key.Interpolation == CurveInterpolation::Cubic)
			{
				out << YAML::Key << "InTangent" << YAML::Value << key.InTangent;
				out << YAML::Key << "OutTangent" << YAML::Value << key.OutTangent;
			}
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	// A `CurveProperty` is stored as its mode, its constant and (when there are any) its curve keys
	template <typename T>
	static void SerializeCurveProperty(YAML::Emitter& out, const char* name, const CurveProperty<T>& property)
	{
		out << YAML::Key << name << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Mode" << YAML::Value << Eagle::Utils::GetEnumName(property.Mode);
		out << YAML::Key << "Constant" << YAML::Value << property.Constant;
		SerializeCurve(out, "Keys", property.Keys);
		out << YAML::EndMap;
	}

	static void SerializeSequenceTrack(YAML::Emitter& out, const Ref<SequenceTrack>& track)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << Eagle::Utils::GetEnumName(track->GetType());
		out << YAML::Key << "ID" << YAML::Value << track->GetID();
		out << YAML::Key << "Name" << YAML::Value << track->GetName();
		out << YAML::Key << "Enabled" << YAML::Value << track->IsEnabled();

		switch (track->GetType())
		{
			case SequenceTrackType::Camera:
			{
				const SequenceCameraTrack& cameraTrack = (const SequenceCameraTrack&)(*track);

				out << YAML::Key << "DefaultFOV" << YAML::Value << cameraTrack.GetDefaultFOVDegrees();
				out << YAML::Key << "NearClip" << YAML::Value << cameraTrack.GetNearClip();
				out << YAML::Key << "FarClip" << YAML::Value << cameraTrack.GetFarClip();

				SerializeCurve(out, "Location", cameraTrack.GetLocationChannel());
				SerializeCurve(out, "Rotation", cameraTrack.GetRotationChannel());
				SerializeCurve(out, "FOV", cameraTrack.GetFOVChannel());
				break;
			}
			case SequenceTrackType::CameraCuts:
			{
				const SequenceCameraCutTrack& cutTrack = (const SequenceCameraCutTrack&)(*track);
				SerializeCurve(out, "Cuts", cutTrack.GetCutsChannel());
				break;
			}
			case SequenceTrackType::PostProcess:
			{
				const SequencePostProcessTrack& postProcessTrack = (const SequencePostProcessTrack&)(*track);

				out << YAML::Key << "CameraTrack" << YAML::Value << postProcessTrack.GetCameraTrackID();
				out << YAML::Key << "ApplyWholeSequence" << YAML::Value << postProcessTrack.DoesApplyWholeSequence();

				out << YAML::Key << "Properties" << YAML::Value << YAML::BeginSeq;
				for (const auto& channel : postProcessTrack.GetChannels())
				{
					out << YAML::BeginMap;
					out << YAML::Key << "Property" << YAML::Value << Eagle::Utils::GetEnumName(channel.Property);
					out << YAML::Key << "Enabled" << YAML::Value << channel.bEnabled;
					std::visit([&out](const auto& data) { SerializeCurve(out, "Keys", data); }, channel.Data);
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
				break;
			}
			case SequenceTrackType::Event:
			{
				const SequenceEventTrack& eventTrack = (const SequenceEventTrack&)(*track);
				SerializeCurve(out, "Events", eventTrack.GetEventsChannel());
				break;
			}
		}

		out << YAML::EndMap;
	}

	// Returns all entities starting from the root (root is included)
	static void GetAllEntities(Entity root, std::vector<Entity>* outEntities)
	{
		outEntities->push_back(root);

		const auto& children = root.GetChildren();
		for (const auto& child : children)
		{
			GetAllEntities(child, outEntities);
		}
	}

	ScopedDataBuffer Serializer::SerializeAsset(const Ref<Asset>& asset)
	{
		if (!asset)
		{
			EG_CORE_ERROR("Failed to serialize an asset. The asset is null");
			return {};
		}

		switch (asset->GetAssetType())
		{
			case AssetType::Texture2D:
				return SerializeAssetTexture2D(Cast<AssetTexture2D>(asset));
			case AssetType::TextureCube:
				return SerializeAssetTextureCube(Cast<AssetTextureCube>(asset));
			case AssetType::StaticMesh:
				return SerializeAssetStaticMesh(Cast<AssetStaticMesh>(asset));
			case AssetType::SkeletalMesh:
				return SerializeAssetSkeletalMesh(Cast<AssetSkeletalMesh>(asset));
			case AssetType::Audio:
				return SerializeAssetAudio(Cast<AssetAudio>(asset));
			case AssetType::Font:
				return SerializeAssetFont(Cast<AssetFont>(asset));
			case AssetType::Material:
				return SerializeAssetMaterial(Cast<AssetMaterial>(asset));
			case AssetType::PhysicsMaterial:
				return SerializeAssetPhysicsMaterial(Cast<AssetPhysicsMaterial>(asset));
			case AssetType::SoundGroup:
				return SerializeAssetSoundGroup(Cast<AssetSoundGroup>(asset));
			case AssetType::Entity:
				return SerializeAssetEntity(Cast<AssetEntity>(asset));
			case AssetType::Animation:
				return SerializeAssetAnimation(Cast<AssetAnimation>(asset));
			case AssetType::AnimationGraph:
				return SerializeAssetAnimationGraph(Cast<AssetAnimationGraph>(asset));
			case AssetType::ParticleSystem:
				return SerializeAssetParticleSystem(Cast<AssetParticleSystem>(asset));
			case AssetType::AnimationBlendSpace:
				return SerializeAssetAnimationBlendSpace(Cast<AssetAnimationBlendSpace>(asset));
			case AssetType::BehaviorGraph:
				return SerializeAssetBehaviorGraph(Cast<AssetBehaviorGraph>(asset));
			case AssetType::SceneSequence:
				return SerializeAssetSceneSequence(Cast<AssetSceneSequence>(asset));
			default:
				EG_CORE_ASSERT(false);
				EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
				return {};
		}
	}

	ScopedDataBuffer Serializer::SerializeAssetTexture2DFromData(const DataBuffer& textureData, const std::vector<ScopedDataBuffer>& compressedDataPerMip, ImageFormat compressedFormat,
		const GUID& guid, const Path& pathToRaw, FilterMode filterMode, AddressMode addressMode, float anisotropy, uint32_t mipsCount, uint32_t width, uint32_t height,
		AssetTexture2DFormat format, TextureCompressor::Quality compression, bool bNormalMap, bool bAlwaysIncludeInGameBuilds)
	{
		struct CompressedTextureDataInfo
		{
			size_t Size = 0;
			size_t Offset = 0;
		};

		size_t totalSize = sizeof(AssetHeader);

		ScopedDataBuffer compressed(Compressor::Compress(textureData));
		std::vector<CompressedTextureDataInfo> compressedInfo;
		const size_t textureDataOffset = Utils::AddSize(compressed, &totalSize);
		const bool bStoreCompressedMips = compression != TextureCompressor::Quality::Disabled;
		if (bStoreCompressedMips)
		{
			compressedInfo.reserve(compressedDataPerMip.size());
			for (const auto& data : compressedDataPerMip)
			{
				auto& info = compressedInfo.emplace_back();
				info.Size = data.Size();
				info.Offset = Utils::AddSize(data, &totalSize);
			}
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Texture2D);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);
		out << YAML::Key << "FilterMode" << YAML::Value << Utils::GetEnumName(filterMode);
		out << YAML::Key << "AddressMode" << YAML::Value << Utils::GetEnumName(addressMode);
		out << YAML::Key << "Anisotropy" << YAML::Value << anisotropy;
		out << YAML::Key << "MipsCount" << YAML::Value << mipsCount;
		out << YAML::Key << "Width" << YAML::Value << width;
		out << YAML::Key << "Height" << YAML::Value << height;
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(format);
		out << YAML::Key << "Compression" << YAML::Value << Utils::GetEnumName(compression);
		out << YAML::Key << "IsNormalMap" << YAML::Value << bNormalMap;

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << compressed.Size();
		out << YAML::Key << "Offset" << YAML::Value << textureDataOffset;
		if (bStoreCompressedMips)
		{
			out << YAML::Key << "CompressedFormat" << YAML::Value << Utils::GetEnumName(compressedFormat);
			out << YAML::Key << "Compressed" << YAML::Value << YAML::BeginSeq;
			for (const auto& info : compressedInfo)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Size" << YAML::Value << info.Size;
				out << YAML::Key << "Offset" << YAML::Value << info.Offset;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}
		out << YAML::EndMap;

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteToBuffer(buffer, compressed, &offset);
		if (bStoreCompressedMips)
		{
			for (const auto& data : compressedDataPerMip)
				Utils::WriteToBuffer(buffer, data, &offset);
		}
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetTexture2D(const Ref<AssetTexture2D>& asset)
	{
		const auto& texture = asset->GetTexture();

		return SerializeAssetTexture2DFromData(asset->GetRawData().GetDataBuffer(), asset->GetCompressedDataPerMip(), texture->GetFormat(), asset->GetGUID(), asset->GetPathToRaw(),
			texture->GetFilterMode(), texture->GetAddressMode(), texture->GetAnisotropy(), texture->GetMipsCount(), texture->GetWidth(), texture->GetHeight(),
			asset->GetFormat(), asset->GetCompressionQuality(), asset->IsNormalMap(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetTextureCubeFromData(const DataBuffer& textureData, const GUID& guid, const Path& pathToRaw, AssetTextureCubeFormat format, uint32_t layerSize, uint32_t prefilterSize, bool bCompress, bool bAlwaysIncludeInGameBuilds)
	{
		size_t totalSize = sizeof(AssetHeader);

		ScopedDataBuffer compressed(Compressor::Compress(textureData));
		const size_t textureDataOffset = Utils::AddSize(compressed, &totalSize);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::TextureCube);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(format);
		out << YAML::Key << "LayerSize" << YAML::Value << layerSize;
		out << YAML::Key << "PrefilterSize" << YAML::Value << prefilterSize;
		out << YAML::Key << "Compress" << YAML::Value << bCompress;

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << compressed.Size();
		out << YAML::Key << "Offset" << YAML::Value << textureDataOffset;
		out << YAML::EndMap;

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteToBuffer(buffer, compressed, &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetTextureCube(const Ref<AssetTextureCube>& asset)
	{
		const auto& textureCube = asset->GetTexture();
		return SerializeAssetTextureCubeFromData(asset->GetRawData().GetDataBuffer(), asset->GetGUID(), asset->GetPathToRaw(),
			asset->GetFormat(), textureCube->GetSize().x, textureCube->GetPrefilterSize(), textureCube->IsCompressed(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetStaticMeshFromMesh(const Ref<StaticMesh>& mesh, const GUID& guid, const Path& pathToRaw,
		bool bCombinedMesh, const std::string& sourceMeshName, uint32_t sourceMeshIndex, bool bLocationReset, bool bAlwaysIncludeInGameBuilds)
	{
		size_t totalSize = sizeof(AssetHeader);

		DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(Vertex) };
		ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));

		const uint32_t materialSlots = mesh->GetMaterialSlotsCount();
		std::vector<ScopedDataBuffer> compressedIndices(materialSlots);
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(i), mesh->GetIndicesCount(i) * sizeof(Index) };
			compressedIndices[i] = Compressor::Compress(indicesBuffer);
		}

		const size_t verticesOffset = Utils::AddSize(compressedVertices, &totalSize);
		std::vector<size_t> indicesOffsets(materialSlots);
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			indicesOffsets[i] = Utils::AddSize(compressedIndices[i], &totalSize);
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::StaticMesh);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);

		out << YAML::Key << "CombinedMesh" << YAML::Value << bCombinedMesh;
		if (!bCombinedMesh)
		{
			out << YAML::Key << "SourceMeshName" << YAML::Value << sourceMeshName;
			out << YAML::Key << "SourceMeshIndex" << YAML::Value << sourceMeshIndex;
			out << YAML::Key << "ResetLocation" << YAML::Value << bLocationReset;
		}

		// AABB
		{
			const auto& aabb = mesh->GetAABB();
			out << YAML::Key << "AABB" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Min" << YAML::Value << aabb.Min;
			out << YAML::Key << "Max" << YAML::Value << aabb.Max;
			out << YAML::EndMap;
		}

		if (const uint32_t materialSlots = mesh->GetMaterialSlotsCount())
		{
			bool bAnyValid = false;
			for (uint32_t i = 0; i < materialSlots; ++i)
			{
				if (const auto& materialAsset = mesh->GetMaterialAsset(i))
				{
					bAnyValid = true;
					break;
				}
			}

			if (bAnyValid)
			{
				out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
				for (uint32_t i = 0; i < materialSlots; ++i)
				{
					if (const auto& materialAsset = mesh->GetMaterialAsset(i))
					{
						out << YAML::BeginMap;
						out << YAML::Key << "Index" << YAML::Value << i;
						out << YAML::Key << "Material" << YAML::Value << materialAsset->GetGUID();
						out << YAML::EndMap;
					}
				}
				out << YAML::EndSeq;
			}
		}

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "VerticesSize" << YAML::Value << compressedVertices.Size();
		out << YAML::Key << "VerticesOffset" << YAML::Value << verticesOffset;

		// Indices per material
		out << YAML::Key << "IndicesPerMaterial" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "IndicesSize" << YAML::Value << compressedIndices[i].Size();
			out << YAML::Key << "IndicesOffset" << YAML::Value << indicesOffsets[i];
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteToBuffer(buffer, compressedVertices, &offset);
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			Utils::WriteToBuffer(buffer, compressedIndices[i], &offset);
		}
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetStaticMesh(const Ref<AssetStaticMesh>& asset)
	{
		return SerializeAssetStaticMeshFromMesh(asset->GetMesh(), asset->GetGUID(), asset->GetPathToRaw(),
			asset->IsCombinedMesh(), asset->GetSourceMeshName(), asset->GetSourceMeshIndex(), asset->WasLocationReset(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetSkeletalMeshFromMesh(const Ref<SkeletalMesh>& mesh, const GUID& guid, const Path& pathToRaw,
		bool bCombinedMesh, const std::string& sourceMeshName, uint32_t sourceMeshIndex, bool bLocationReset, bool bAlwaysIncludeInGameBuilds)
	{
		size_t totalSize = sizeof(AssetHeader);

		DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(SkeletalVertex) };
		ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));

		const uint32_t materialSlots = mesh->GetMaterialSlotsCount();
		std::vector<ScopedDataBuffer> compressedIndices(materialSlots);
		std::vector<size_t> origIndicesDataSizes(materialSlots);
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(i), mesh->GetIndicesCount(i) * sizeof(Index) };
			compressedIndices[i] = Compressor::Compress(indicesBuffer);
		}

		const size_t verticesOffset = Utils::AddSize(compressedVertices, &totalSize);
		std::vector<size_t> indicesOffsets(materialSlots);
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			indicesOffsets[i] = Utils::AddSize(compressedIndices[i], &totalSize);
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SkeletalMesh);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);

		out << YAML::Key << "CombinedMesh" << YAML::Value << bCombinedMesh;
		if (!bCombinedMesh)
		{
			out << YAML::Key << "SourceMeshName" << YAML::Value << sourceMeshName;
			out << YAML::Key << "SourceMeshIndex" << YAML::Value << sourceMeshIndex;
			out << YAML::Key << "ResetLocation" << YAML::Value << bLocationReset;
		}

		// AABB
		{
			const auto& aabb = mesh->GetAABB();
			out << YAML::Key << "AABB" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Min" << YAML::Value << aabb.Min;
			out << YAML::Key << "Max" << YAML::Value << aabb.Max;
			out << YAML::EndMap;
		}

		out << YAML::Key << "MinRagdollBoneSize" << YAML::Value << mesh->GetMinRagdollBoneSize();
		out << YAML::Key << "MaxRagdollTwist" << YAML::Value << mesh->GetRagdollMaxTwist();
		out << YAML::Key << "MaxRagdollSwing" << YAML::Value << mesh->GetRagdollMaxSwing();
		Serializer::SerializeProjectCollisionGroupGUIDs(out);
		out << YAML::Key << "CollisionDetectionType" << YAML::Value << Utils::GetEnumName(mesh->GetCollisionDetectionType());
		out << YAML::Key << "CollisionGroupMask" << YAML::Value << uint32_t(mesh->GetCollisionGroup());
		out << YAML::Key << "InteractingCollisionGroupMask" << YAML::Value << uint32_t(mesh->GetInteractingCollisionGroup());

		const auto& ragdollData = mesh->GetRagdollRoot();
		if (!ragdollData.Name.empty())
		{
			out << YAML::Key << "RagdollBonesData" << YAML::Value << YAML::BeginSeq;
			SerializeRagdollBonesData(out, ragdollData);
			out << YAML::EndSeq;
		}

		if (materialSlots > 0)
		{
			bool bAnyValid = false;
			for (uint32_t i = 0; i < materialSlots; ++i)
				if (const auto& materialAsset = mesh->GetMaterialAsset(i))
				{
					bAnyValid = true;
					break;
				}

			if (bAnyValid)
			{
				out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
				for (uint32_t i = 0; i < materialSlots; ++i)
				{
					if (const auto& materialAsset = mesh->GetMaterialAsset(i))
					{
						out << YAML::BeginMap;
						out << YAML::Key << "Index" << YAML::Value << i;
						out << YAML::Key << "Material" << YAML::Value << materialAsset->GetGUID();
						out << YAML::EndMap;
					}
				}
				out << YAML::EndSeq;
			}
		}

		const auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
		// Skeletal data
		{
			out << YAML::Key << "InverseTransform" << YAML::Value << skeletalInfo.InverseTransform;
			out << YAML::Key << "CoordCorrection" << YAML::Value << skeletalInfo.CoordCorrection;

			out << YAML::Key << "Skeletal" << YAML::Value;
			Serializer::EmitBoneNode(out, skeletalInfo.RootBone);
		}

		out << YAML::Key << "BoneInfoMap";
		{
			out << YAML::Value << YAML::BeginSeq;

			const auto& bones = skeletalInfo.GetBoneInfoMap();
			for (auto& [name, data] : bones)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << name;
				out << YAML::Key << "Matrix" << YAML::Value << data.Offset;
				out << YAML::Key << "ID" << YAML::Value << data.BoneID;
				out << YAML::EndMap;
			}

			out << YAML::EndSeq;
		}

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "VerticesSize" << YAML::Value << compressedVertices.Size();
		out << YAML::Key << "VerticesOffset" << YAML::Value << verticesOffset;

		// Indices per material
		out << YAML::Key << "IndicesPerMaterial" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "IndicesSize" << YAML::Value << compressedIndices[i].Size();
			out << YAML::Key << "IndicesOffset" << YAML::Value << indicesOffsets[i];
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteToBuffer(buffer, compressedVertices, &offset);
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			Utils::WriteToBuffer(buffer, compressedIndices[i], &offset);
		}
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetSkeletalMesh(const Ref<AssetSkeletalMesh>& asset)
	{
		return SerializeAssetSkeletalMeshFromMesh(asset->GetMesh(), asset->GetGUID(), asset->GetPathToRaw(),
			asset->IsCombinedMesh(), asset->GetSourceMeshName(), asset->GetSourceMeshIndex(), asset->WasLocationReset(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetAudioFromData(const DataBuffer& audioData, const GUID& guid, const Path& pathToRaw,
		float volume, float pitch, float pan, const Ref<AssetSoundGroup>& soundGroup, bool bAlwaysIncludeInGameBuilds)
	{
		size_t totalSize = sizeof(AssetHeader);

		ScopedDataBuffer compressed(Compressor::Compress(audioData));
		const size_t dataOffset = Utils::AddSize(compressed, &totalSize);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Audio);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);
		out << YAML::Key << "Volume" << YAML::Value << volume;
		out << YAML::Key << "Pitch" << YAML::Value << pitch;
		out << YAML::Key << "Pan" << YAML::Value << pan;
		if (soundGroup)
			out << YAML::Key << "SoundGroup" << YAML::Value << soundGroup->GetGUID();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << compressed.Size();
		out << YAML::Key << "Offset" << YAML::Value << dataOffset;
		out << YAML::EndMap;

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteToBuffer(buffer, compressed, &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetAudio(const Ref<AssetAudio>& asset)
	{
		const auto& audio = asset->GetAudio();
		return SerializeAssetAudioFromData(asset->GetRawData().GetDataBuffer(), asset->GetGUID(), asset->GetPathToRaw(),
			audio->GetVolume(), audio->GetPitch(), audio->GetPan(), asset->GetSoundGroupAsset(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetFontFromData(const DataBuffer& fontData, const DataBuffer& atlasData, glm::uvec2 atlasSize, const GUID& guid, const Path& pathToRaw, bool bAlwaysIncludeInGameBuilds)
	{
		size_t totalSize = sizeof(AssetHeader);

		ScopedDataBuffer compressed(Compressor::Compress(fontData));
		ScopedDataBuffer compressedAtlas = Compressor::Compress(atlasData);

		const size_t dataOffset = Utils::AddSize(compressed, &totalSize);
		const size_t atlasOffset = Utils::AddSize(compressedAtlas, &totalSize);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Font);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << compressed.Size();
		out << YAML::Key << "Offset" << YAML::Value << dataOffset;
		out << YAML::EndMap;

		out << YAML::Key << "AtlasData" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << compressedAtlas.Size();
		out << YAML::Key << "Offset" << YAML::Value << atlasOffset;
		out << YAML::Key << "AtlasSize" << YAML::Value << atlasSize;
		out << YAML::Key << "AtlasVersion" << YAML::Value << Font::AtlasVersion;
		out << YAML::EndMap;

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteToBuffer(buffer, compressed, &offset);
		Utils::WriteToBuffer(buffer, compressedAtlas, &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetFont(const Ref<AssetFont>& asset)
	{
		const auto& font = asset->GetFont();
		return SerializeAssetFontFromData(asset->GetRawData().GetDataBuffer(), font->GetAtlasData().GetDataBuffer(), glm::uvec2(font->GetAtlas()->GetSize()), asset->GetGUID(), asset->GetPathToRaw(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetMaterial(const Ref<AssetMaterial>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Material);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);
		
		if (asset)
		{
			const auto& material = asset->GetMaterial();
			if (const auto& textureAsset = material->GetAlbedoAsset())
				out << YAML::Key << "AlbedoTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "Albedo" << YAML::Value << material->GetAlbedo();
			out << YAML::Key << "IsRawAlbedoUsed" << YAML::Value << material->IsRawAlbedoUsed();

			if (const auto& textureAsset = material->GetMetalnessAsset())
				out << YAML::Key << "MetalnessTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "Metalness" << YAML::Value << material->GetMetalness();
			out << YAML::Key << "IsRawMetalnessUsed" << YAML::Value << material->IsRawMetalnessUsed();
			out << YAML::Key << "MetalnessTextureChannel" << YAML::Value << Utils::GetEnumName(material->GetMetalnessTextureChannel());

			if (const auto& textureAsset = material->GetNormalAsset())
				out << YAML::Key << "NormalTexture" << YAML::Value << textureAsset->GetGUID();

			if (const auto& textureAsset = material->GetRoughnessAsset())
				out << YAML::Key << "RoughnessTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "Roughness" << YAML::Value << material->GetRoughness();
			out << YAML::Key << "IsRawRoughnessUsed" << YAML::Value << material->IsRawRoughnessUsed();
			out << YAML::Key << "RoughnessTextureChannel" << YAML::Value << Utils::GetEnumName(material->GetRoughnessTextureChannel());

			if (const auto& textureAsset = material->GetAOAsset())
				out << YAML::Key << "AOTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "AO" << YAML::Value << material->GetAO();
			out << YAML::Key << "IsRawAOUsed" << YAML::Value << material->IsRawAOUsed();
			out << YAML::Key << "AOTextureChannel" << YAML::Value << Utils::GetEnumName(material->GetAOTextureChannel());

			if (const auto& textureAsset = material->GetEmissiveAsset())
				out << YAML::Key << "EmissiveTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "Emissive" << YAML::Value << material->GetEmissive();
			out << YAML::Key << "IsRawEmissiveUsed" << YAML::Value << material->IsRawEmissiveUsed();

			if (const auto& textureAsset = material->GetOpacityAsset())
				out << YAML::Key << "OpacityTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "Opacity" << YAML::Value << material->GetOpacity();
			out << YAML::Key << "IsRawOpacityUsed" << YAML::Value << material->IsRawOpacityUsed();
			out << YAML::Key << "OpacityTextureChannel" << YAML::Value << Utils::GetEnumName(material->GetOpacityTextureChannel());

			if (const auto& textureAsset = material->GetOpacityMaskAsset())
				out << YAML::Key << "OpacityMaskTexture" << YAML::Value << textureAsset->GetGUID();
			out << YAML::Key << "OpacityMask" << YAML::Value << material->GetOpacityMask();
			out << YAML::Key << "IsRawOpacityMaskUsed" << YAML::Value << material->IsRawOpacityMaskUsed();
			out << YAML::Key << "OpacityMaskTextureChannel" << YAML::Value << Utils::GetEnumName(material->GetOpacityMaskTextureChannel());

			out << YAML::Key << "TintColor" << YAML::Value << material->GetTintColor();
			out << YAML::Key << "EmissiveIntensity" << YAML::Value << material->GetEmissiveIntensity();
			out << YAML::Key << "TilingFactor" << YAML::Value << material->GetTilingFactor();
			out << YAML::Key << "BlendMode" << YAML::Value << Utils::GetEnumName(material->GetBlendMode());
			out << YAML::Key << "IsDoubleSided" << YAML::Value << material->IsDoubleSided();
		}
		
		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetPhysicsMaterial(const Ref<AssetPhysicsMaterial>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::PhysicsMaterial);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);

		if (asset)
		{
			const auto& material = asset->GetMaterial();
			out << YAML::Key << "StaticFriction" << YAML::Value << material->GetStaticFriction();
			out << YAML::Key << "DynamicFriction" << YAML::Value << material->GetDynamicFriction();
			out << YAML::Key << "Bounciness" << YAML::Value << material->GetBounciness();
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetSoundGroup(const Ref<AssetSoundGroup>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SoundGroup);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);

		if (asset)
		{
			const auto& soundGroup = asset->GetSoundGroup();
			out << YAML::Key << "Volume" << YAML::Value << soundGroup->GetVolume();
			out << YAML::Key << "Pitch" << YAML::Value << soundGroup->GetPitch();
			out << YAML::Key << "IsPaused" << YAML::Value << soundGroup->IsPaused();
			out << YAML::Key << "IsMuted" << YAML::Value << soundGroup->IsMuted();
		}
		else
		{
			out << YAML::Key << "Volume" << YAML::Value << 1.f;
			out << YAML::Key << "Pitch" << YAML::Value << 1.f;
			out << YAML::Key << "IsPaused" << YAML::Value << false;
			out << YAML::Key << "IsMuted" << YAML::Value << false;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetEntity(const Ref<AssetEntity>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Entity);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);

		Serializer::SerializeProjectCollisionGroupGUIDs(out);

		if (asset)
		{
			Entity rootEntity = *asset->GetEntity().get();
			std::vector<Entity> entities;
			entities.reserve(10);
			GetAllEntities(rootEntity, &entities);

			out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
			for (const auto& entity : entities)
			{
				Serializer::SerializeEntity(out, entity);
			}
			out << YAML::EndSeq;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetAnimationFromData(const GUID& guid, const Path& pathToRaw, uint32_t animIndex, const SkeletalMeshAnimation& anim, const Ref<AssetSkeletalMesh>& skeletal, bool bAlwaysIncludeInGameBuilds)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Animation);
		out << YAML::Key << "GUID" << YAML::Value << guid;
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;
		out << YAML::Key << "RawPath" << YAML::Value << Utils::AsString(pathToRaw);
		out << YAML::Key << "Index" << YAML::Value << animIndex;
		out << YAML::Key << "Skeletal" << YAML::Value << skeletal->GetGUID();

		const auto& bones = anim.GetAnimationBones();
		const bool bHasRootMotion = anim.HasRootMotion();

		// Serialize animation
		ScopedDataBuffer bonesBinary;
		size_t bonesOffset = 0;
		{
			out << YAML::Key << "Animation" << YAML::Value << YAML::BeginMap;

			out << YAML::Key << "Duration" << YAML::Value << anim.Duration;
			out << YAML::Key << "TicksPerSecond" << YAML::Value << anim.TicksPerSecond;
			out << YAML::Key << "bInPlace" << YAML::Value << anim.bInPlace;

			if (bHasRootMotion)
			{
				out << YAML::Key << "RootMotionMode" << YAML::Value << Utils::GetEnumName(anim.RootMotionType);
				out << YAML::Key << "RootMotion";
				{
					const size_t rmLocationOffset = Utils::AddSize(anim.RootMotion.Locations, &totalSize);
					const size_t rmRotationOffset = Utils::AddSize(anim.RootMotion.Rotations, &totalSize);
					const size_t rmScaleOffset = Utils::AddSize(anim.RootMotion.Scales, &totalSize);
					const size_t preRMLocationsOffset = Utils::AddSize(anim.PreRootMotionLocations, &totalSize);

					out << YAML::Value;
					out << YAML::BeginMap;

					out << YAML::Key << "LocationsSize" << YAML::Value << anim.RootMotion.Locations.size() * sizeof(KeyPosition);
					out << YAML::Key << "LocationsOffset" << YAML::Value << rmLocationOffset;

					out << YAML::Key << "RotationsSize" << YAML::Value << anim.RootMotion.Rotations.size() * sizeof(KeyRotation);
					out << YAML::Key << "RotationsOffset" << YAML::Value << rmRotationOffset;

					out << YAML::Key << "ScalesSize" << YAML::Value << anim.RootMotion.Scales.size() * sizeof(KeyScale);
					out << YAML::Key << "ScalesOffset" << YAML::Value << rmScaleOffset;

					out << YAML::Key << "PreRMLocationsSize" << YAML::Value << anim.PreRootMotionLocations.size() * sizeof(glm::vec3);
					out << YAML::Key << "PreRMLocationsOffset" << YAML::Value << preRMLocationsOffset;

					out << YAML::EndMap;
				}
			}

			// Events
			if (anim.Events.size() > 0)
			{
				out << YAML::Key << "Events" << YAML::Value << YAML::BeginSeq;

				for (const auto& event : anim.Events)
				{
					out << YAML::BeginMap;

					out << YAML::Key << "Name" << YAML::Value << event.Name;
					out << YAML::Key << "Time" << YAML::Value << event.Time;

					out << YAML::EndMap;
				}

				out << YAML::EndSeq;
			}

			{
				size_t bonesSize = 0;
				for (const auto& [name, data] : bones)
				{
					Utils::AddSize(sizeof(size_t), &bonesSize); // Name size
					Utils::AddSize(name, &bonesSize); // Name

					Utils::AddSize(sizeof(size_t), &bonesSize); // Keys size
					Utils::AddSize(data.Locations.size() * sizeof(KeyPosition), &bonesSize);

					Utils::AddSize(sizeof(size_t), &bonesSize); // Keys size
					Utils::AddSize(data.Rotations.size() * sizeof(KeyRotation), &bonesSize);

					Utils::AddSize(sizeof(size_t), &bonesSize); // Keys size
					Utils::AddSize(data.Scales.size() * sizeof(KeyScale), &bonesSize);

					Utils::AddSize(sizeof(uint32_t), &bonesSize); // BoneID
				}

				size_t offset = 0;
				bonesBinary.Allocate(bonesSize);
				for (const auto& [name, data] : bones)
				{
					Utils::WriteToBuffer(bonesBinary, name.size() + 1, &offset);
					Utils::WriteStringToBuffer(bonesBinary, name, &offset);

					Utils::WriteToBuffer(bonesBinary, data.Locations.size() * sizeof(KeyPosition), &offset);
					Utils::WriteToBuffer(bonesBinary, data.Locations, &offset);

					Utils::WriteToBuffer(bonesBinary, data.Rotations.size() * sizeof(KeyRotation), &offset);
					Utils::WriteToBuffer(bonesBinary, data.Rotations, &offset);

					Utils::WriteToBuffer(bonesBinary, data.Scales.size() * sizeof(KeyScale), &offset);
					Utils::WriteToBuffer(bonesBinary, data.Scales, &offset);

					Utils::WriteToBuffer(bonesBinary, data.BoneID, &offset);
				}
				EG_CORE_ASSERT(offset == bonesSize);

				bonesOffset = Utils::AddSize(bonesBinary, &totalSize);
			}

			out << YAML::Key << "BonesSize" << YAML::Value << bonesBinary.Size();
			out << YAML::Key << "BonesOffset" << YAML::Value << bonesOffset;

			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		if (bHasRootMotion)
		{
			Utils::WriteToBuffer(buffer, anim.RootMotion.Locations, &offset);
			Utils::WriteToBuffer(buffer, anim.RootMotion.Rotations, &offset);
			Utils::WriteToBuffer(buffer, anim.RootMotion.Scales, &offset);
			Utils::WriteToBuffer(buffer, anim.PreRootMotionLocations, &offset);
		}
		Utils::WriteToBuffer(buffer, bonesBinary, &offset);

		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetAnimation(const Ref<AssetAnimation>& asset)
	{
		return SerializeAssetAnimationFromData(asset->GetGUID(), asset->GetPathToRaw(), asset->GetAnimationIndex(), *asset->GetAnimation().get(), asset->GetSkeletal(), asset->IsAlwaysIncludedInGameBuilds());
	}

	ScopedDataBuffer Serializer::SerializeAssetAnimationGraph(const Ref<AssetAnimationGraph>& asset, const Ref<AssetSkeletalMesh>& meshAsset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::AnimationGraph);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);
		if (!asset)
			out << YAML::Key << "SkeletalMesh" << YAML::Value << meshAsset->GetGUID();

		if (asset)
		{
			const auto& graph = asset->GetGraph();

			if (const auto& asset = graph->GetSkeletalAsset())
				out << YAML::Key << "SkeletalMesh" << YAML::Value << asset->GetGUID();

			const auto& editorGraphData = asset->GetSerializationData();

			// Variables
			{
				out << YAML::Key << "Variables" << YAML::Value << YAML::BeginSeq;

				for (const auto& var : editorGraphData.Variables)
				{
					out << YAML::BeginMap;
					out << YAML::Key << "Name" << YAML::Value << var.Name;
					SerializeGraphVar(out, var.Value);

					out << YAML::EndMap;
				}

				out << YAML::EndSeq;
			}

			out << YAML::Key << "Graph" << YAML::Value << YAML::BeginMap;
			SerializeGraph(out, editorGraphData.Graph);
			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetParticleSystem(const Ref<AssetParticleSystem>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::ParticleSystem);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);

		if (asset)
		{
			out << YAML::Key << "Emitters";
			out << YAML::BeginSeq;
			const auto& emitters = asset->GetEmitters();
			for (const auto& emitter : emitters)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << emitter.Name;
				if (const auto& asset = emitter.Texture)
					out << YAML::Key << "Texture" << YAML::Value << asset->GetGUID();
				SerializeCurveProperty(out, "Color", emitter.Color);
				SerializeCurveProperty(out, "ColorIntensity", emitter.ColorIntensity);

				out << YAML::Key << "VelocityMin" << YAML::Value << emitter.VelocityMin;
				out << YAML::Key << "VelocityMax" << YAML::Value << emitter.VelocityMax;

				SerializeCurveProperty(out, "VelocityCoef", emitter.VelocityCoef);
				SerializeCurveProperty(out, "VelocityOverLifetime", emitter.VelocityOverLifetime);
				out << YAML::Key << "VelocitySpace" << YAML::Value << Utils::GetEnumName(emitter.VelocitySpace);

				SerializeCurveProperty(out, "RotationZ", emitter.RotationZ);
				SerializeCurveProperty(out, "RotationSpeed", emitter.RotationSpeed);
				SerializeCurveProperty(out, "Size", emitter.Size);
				out << YAML::Key << "ColliderSizeRatio" << YAML::Value << emitter.ColliderSizeRatio;

				out << YAML::Key << "LifetimeMin" << YAML::Value << emitter.LifetimeMin;
				out << YAML::Key << "LifetimeMax" << YAML::Value << emitter.LifetimeMax;

				out << YAML::Key << "BouncinessMin" << YAML::Value << emitter.BouncinessMin;
				out << YAML::Key << "BouncinessMax" << YAML::Value << emitter.BouncinessMax;

				out << YAML::Key << "AnimationImagesNum" << YAML::Value << emitter.AnimationImagesNum;
				out << YAML::Key << "AnimationSpeed" << YAML::Value << emitter.AnimationSpeed;

				out << YAML::Key << "ID" << YAML::Value << emitter.ID;
				out << YAML::Key << "RelativeLocation" << YAML::Value << emitter.RelativeTransform.Location;
				out << YAML::Key << "RelativeRotation" << YAML::Value << emitter.RelativeTransform.Rotation;
				out << YAML::Key << "VisibilityAABBMin" << YAML::Value << emitter.VisibilityAABB.Min;
				out << YAML::Key << "VisibilityAABBMax" << YAML::Value << emitter.VisibilityAABB.Max;
				out << YAML::Key << "LoopCount" << YAML::Value << emitter.LoopCount;
				out << YAML::Key << "LoopDuration" << YAML::Value << emitter.LoopDuration;
				out << YAML::Key << "SpawnRate" << YAML::Value << emitter.SpawnRate;
				out << YAML::Key << "FastForwardTo" << YAML::Value << emitter.FastForwardTo;
				out << YAML::Key << "StartDelay" << YAML::Value << emitter.StartDelay;
				out << YAML::Key << "RandomTint" << YAML::Value << emitter.bRandomTint;
				out << YAML::Key << "RandomTintA" << YAML::Value << emitter.RandomTintA;
				out << YAML::Key << "RandomTintB" << YAML::Value << emitter.RandomTintB;
				out << YAML::Key << "StartRotationRandomRange" << YAML::Value << emitter.StartRotationRandomRange;
				out << YAML::Key << "StartRotationSpeedRandomRange" << YAML::Value << emitter.StartRotationSpeedRandomRange;
				out << YAML::Key << "StartSizeMultiplierRandomRange" << YAML::Value << emitter.StartSizeMultiplierRandomRange;
				SerializeCurveProperty(out, "Drag", emitter.Drag);
				SerializeCurveProperty(out, "TurbulenceStrength", emitter.TurbulenceStrength);
				SerializeCurveProperty(out, "TurbulenceScale", emitter.TurbulenceScale);
				out << YAML::Key << "TurbulenceSpeed" << YAML::Value << emitter.TurbulenceSpeed;
				out << YAML::Key << "InheritVelocity" << YAML::Value << emitter.InheritVelocity;
				out << YAML::Key << "SpawnPerMeter" << YAML::Value << emitter.SpawnPerMeter;
				out << YAML::Key << "SimulationSpace" << YAML::Value << Utils::GetEnumName(emitter.SimulationSpace);
				out << YAML::Key << "SpawnOnlyFromEvents" << YAML::Value << emitter.bSpawnOnlyFromEvents;
				out << YAML::Key << "Culling" << YAML::Value << Utils::GetEnumName(emitter.Culling);
				out << YAML::Key << "SubEmitters" << YAML::Value << YAML::BeginSeq;
				for (const auto& subEmitter : emitter.SubEmitters)
				{
					out << YAML::BeginMap;
					out << YAML::Key << "EmitterID" << YAML::Value << subEmitter.EmitterID;
					out << YAML::Key << "Trigger" << YAML::Value << Utils::GetEnumName(subEmitter.Trigger);
					out << YAML::Key << "UseEmitterSpawnSettings" << YAML::Value << subEmitter.bUseEmitterSpawnSettings;
					out << YAML::Key << "CountRange" << YAML::Value << subEmitter.CountRange;
					out << YAML::Key << "Probability" << YAML::Value << subEmitter.Probability;
					out << YAML::Key << "InheritVelocity" << YAML::Value << subEmitter.InheritVelocity;
					out << YAML::Key << "InheritColor" << YAML::Value << subEmitter.bInheritColor;
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
				out << YAML::Key << "RadialAcceleration" << YAML::Value << emitter.RadialAcceleration;
				out << YAML::Key << "TangentialAcceleration" << YAML::Value << emitter.TangentialAcceleration;
				out << YAML::Key << "NormalVelocityFactor" << YAML::Value << emitter.NormalVelocityFactor;

				out << YAML::Key << "EmissionShape" << YAML::Value << Utils::GetEnumName(emitter.EmissionShape);
				out << YAML::Key << "SphereRadius" << YAML::Value << emitter.SphereRadius;
				out << YAML::Key << "BoxMin" << YAML::Value << emitter.BoxMin;
				out << YAML::Key << "BoxMax" << YAML::Value << emitter.BoxMax;
				out << YAML::Key << "RingRadius" << YAML::Value << emitter.RingRadius;
				out << YAML::Key << "RingThickness" << YAML::Value << emitter.RingThickness;
				if (emitter.MeshAsset)
					out << YAML::Key << "Mesh" << YAML::Value << emitter.MeshAsset->GetGUID();
				if (const auto& animAsset = emitter.MeshAnimationAsset)
					out << YAML::Key << "AnimationClip" << YAML::Value << animAsset->GetGUID();
				out << YAML::Key << "ClipPlaybackSpeed" << emitter.ClipPlaybackSpeed;
				out << YAML::Key << "ClipLooping" << emitter.bClipLooping;
				out << YAML::Key << "TriggerAnimationEvents" << emitter.bTriggerAnimationEvents;

				out << YAML::Key << "CollisionMode" << YAML::Value << Utils::GetEnumName(emitter.CollisionMode);

				out << YAML::Key << "bDestroyImmediately" << YAML::Value << emitter.bDestroyImmediately;
				out << YAML::Key << "bEmit" << YAML::Value << emitter.bEmit;
				out << YAML::Key << "bExplode" << YAML::Value << emitter.bExplode;
				SerializeCurveProperty(out, "ApplyGravity", emitter.ApplyGravity);
				out << YAML::Key << "DepthFade" << YAML::Value << emitter.bDepthFade;
				out << YAML::Key << "DepthFadeDistance" << YAML::Value << emitter.DepthFadeDistance;
				out << YAML::Key << "CameraFade" << YAML::Value << emitter.bCameraFade;
				out << YAML::Key << "CameraFadeDistance" << YAML::Value << emitter.CameraFadeDistance;
				out << YAML::Key << "bAlphaBlending" << YAML::Value << emitter.bAlphaBlending;
				out << YAML::Key << "bAdditive" << YAML::Value << emitter.bAdditive;
				out << YAML::Key << "bBlendAnimation" << YAML::Value << emitter.bBlendAnimation;
				out << YAML::Key << "bFaceDirection" << YAML::Value << emitter.bFaceDirection;

				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetAnimationBlendSpace(const Ref<AssetAnimationBlendSpace>& asset, const Ref<AssetSkeletalMesh>& meshAsset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::AnimationBlendSpace);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);
		if (!asset)
			out << YAML::Key << "SkeletalMesh" << YAML::Value << meshAsset->GetGUID();

		if (asset)
		{
			out << YAML::Key << "SkeletalMesh" << YAML::Value << asset->GetSkeletalMesh()->GetGUID();
			out << YAML::Key << "EventsTriggerMode" << YAML::Value << Utils::GetEnumName(asset->GetEventsTriggerMode());
			out << YAML::Key << "BlendTime" << YAML::Value << asset->GetBlendTime();
			out << YAML::Key << "SyncEnabled" << YAML::Value << asset->IsSyncEnabled();
			out << YAML::Key << "UseShortestBlendPath" << YAML::Value << asset->IsShortestBlendPathEnabled();

			const auto& horAxis = asset->GetHorizontalAxis();
			out << YAML::Key << "HorizontalAxis" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << horAxis.Name;
			out << YAML::Key << "Min" << YAML::Value << horAxis.Min;
			out << YAML::Key << "Max" << YAML::Value << horAxis.Max;
			out << YAML::EndMap;

			const auto& verAxis = asset->GetVerticalAxis();
			out << YAML::Key << "VerticalAxis" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << verAxis.Name;
			out << YAML::Key << "Min" << YAML::Value << verAxis.Min;
			out << YAML::Key << "Max" << YAML::Value << verAxis.Max;
			out << YAML::EndMap;

			const auto& points = asset->GetPointsData();
			out << YAML::Key << "Points" << YAML::Value << YAML::BeginSeq;
			for (const auto& point : points)
			{
				out << YAML::BeginMap;
				if (point.Animation)
					out << YAML::Key << "Animation" << YAML::Value << point.Animation->GetGUID();
				out << YAML::Key << "Coord" << YAML::Value << point.Coord;
				out << YAML::Key << "AnimSpeed" << YAML::Value << point.AnimSpeed;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetBehaviorGraph(const Ref<AssetBehaviorGraph>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::BehaviorGraph);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);

		if (asset)
		{
			const auto& root = asset->GetRoot();
			if (!root.Data.ClassData.FullName.empty())
			{
				out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginMap;
				SerializeAIBehaviorNode(out, root);
				out << YAML::EndMap;
			}

			const auto& editorGraphData = asset->GetSerializationData();
			out << YAML::Key << "Graph" << YAML::Value << YAML::BeginMap;
			SerializeGraph(out, editorGraphData.Graph);
			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}

	ScopedDataBuffer Serializer::SerializeAssetSceneSequence(const Ref<AssetSceneSequence>& asset)
	{
		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SceneSequence);
		out << YAML::Key << "GUID" << YAML::Value << (asset ? asset->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << (asset ? asset->IsAlwaysIncludedInGameBuilds() : false);

		if (asset)
		{
			out << YAML::Key << "Duration" << YAML::Value << asset->GetDuration();
			out << YAML::Key << "FrameRate" << YAML::Value << asset->GetFrameRate();
			out << YAML::Key << "Looping" << YAML::Value << asset->IsLooping();

			const auto& tracks = asset->GetTracks();
			if (!tracks.empty())
			{
				out << YAML::Key << "Tracks" << YAML::Value << YAML::BeginSeq;
				for (const auto& track : tracks)
				{
					if (track)
						SerializeSequenceTrack(out, track);
				}
				out << YAML::EndSeq;
			}
		}

		out << YAML::EndMap;

		const AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteYaml(buffer, out, &offset);

		return buffer;
	}
}
