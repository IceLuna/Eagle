#include "egpch.h"
#include "Serializer.h"

#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Classes/Font.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/AssimpImporter.h"

#include <stb_image.h>

namespace Eagle
{
	template<typename AssetType>
	static Ref<AssetType> GetAsset(const YAML::Node& node)
	{
		if (node)
		{
			Ref<Asset> asset;
			if (AssetManager::Get(node.as<GUID>(), &asset))
				return Cast<AssetType>(asset);
		}

		return {};
	}

	static bool SanitaryAssetChecks(const YAML::Node& baseNode, const Path& path, AssetType expectedType)
	{
		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to deserialize an asset: {}", path.u8string());
			return false;
		}

		AssetType actualType = AssetType::None;
		if (auto node = baseNode["Type"])
			actualType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		if (actualType != expectedType)
		{
			EG_CORE_ERROR("Failed to load an asset. It's not a {}: {}", Utils::GetEnumName(actualType), path.u8string());
			return false;
		}

		return true;
	}

	static void SerializeGraphVar(YAML::Emitter& out, const Ref<GraphVariable>& var)
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

	static Ref<GraphVariable> DeserializeGraphVar(const YAML::Node& varNode)
	{
		GraphVariableType varType = Utils::GetEnumFromName<GraphVariableType>(varNode["Type"].as<std::string>());
		auto valueNode = varNode["Value"];
		Ref<GraphVariable> result;

		switch (varType)
		{
		case GraphVariableType::Bool:
			result = MakeRef<GraphVariableBool>(valueNode ? valueNode.as<bool>() : false);
			break;
		case GraphVariableType::Int:
			result = MakeRef<GraphVariableInt>(valueNode ? valueNode.as<int>() : 0);
			break;
		case GraphVariableType::Float:
			result = MakeRef<GraphVariableFloat>(valueNode ? valueNode.as<float>() : 0.f);
			break;
		case GraphVariableType::Animation:
			result = MakeRef<GraphVariableAnimation>(valueNode ? GetAsset<AssetAnimation>(valueNode) : nullptr);
			break;
		case GraphVariableType::String:
			result = MakeRef<GraphVariableString>(valueNode ? valueNode.as<std::string>() : nullptr);
			break;
		case GraphVariableType::Vec4:
			result = MakeRef<GraphVariableVec4>(valueNode ? valueNode.as<glm::vec4>() : glm::vec4(0));
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
			out << YAML::Key << "CachedOwnerID" << YAML::Value << node.CachedOwnerID;
			out << YAML::Key << "CachedNodeID" << YAML::Value << node.CachedNodeID;
			out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(node.Type);
			if (node.Type == GraphNodeType::BlendSpace && node.BlendSpace)
				out << YAML::Key << "BlendSpace" << YAML::Value << node.BlendSpace->GetGUID();
			out << YAML::Key << "AddedCounter" << YAML::Value << node.AddedCounter;
			if (node.UserData.empty() == false)
				out << YAML::Key << "UserData" << YAML::Value << node.UserData;

			{
				out << YAML::Key << "InputPins" << YAML::Value << YAML::BeginSeq;
				for (const auto& pin : node.InputPins)
				{
					out << YAML::BeginMap;
					out << YAML::Key << "ID" << YAML::Value << pin.PinID;
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

			// Output Pins
			{
				out << YAML::Key << "OutputPins" << YAML::Value << YAML::BeginSeq;
				for (const auto& pinID : node.OutputPins)
				{
					out << YAML::BeginMap;
					out << YAML::Key << "ID" << YAML::Value << pinID;
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
			nodeData.NodeID = nodeNode["NodeID"].as<uint32_t>();
			if (auto cachedNode = nodeNode["CachedOwnerID"])
				nodeData.CachedOwnerID = cachedNode.as<GUID>();
			if (auto cachedNode = nodeNode["CachedNodeID"])
				nodeData.CachedNodeID = cachedNode.as<uint32_t>();
			if (auto typeNode = nodeNode["Type"])
				nodeData.Type = Utils::GetEnumFromName<GraphNodeType>(typeNode.as<std::string>());
			if (auto bsNode = nodeNode["BlendSpace"])
				nodeData.BlendSpace = GetAsset<AssetAnimationBlendSpace>(bsNode);
			if (auto counterNode = nodeNode["AddedCounter"])
				nodeData.AddedCounter = counterNode.as<uint32_t>();
			if (auto userDataNode = nodeNode["UserData"])
				nodeData.UserData = userDataNode.as<std::string>();

			{
				const auto inputPinsNode = nodeNode["InputPins"];
				nodeData.InputPins.reserve(inputPinsNode.size());
				for (const auto& inputPinNode : inputPinsNode)
				{
					auto& pinData = nodeData.InputPins.emplace_back();
					pinData.PinID = inputPinNode["ID"].as<uint32_t>();
					if (auto defaultValNode = inputPinNode["DefaultValue"])
						pinData.DefaultValue = DeserializeGraphVar(defaultValNode);
				}
			}

			{
				const auto outputPinsNode = nodeNode["OutputPins"];
				nodeData.OutputPins.reserve(outputPinsNode.size());
				for (const auto& outputPinNode : outputPinsNode)
				{
					auto& pinID = nodeData.OutputPins.emplace_back();
					pinID = outputPinNode["ID"].as<uint32_t>();
				}
			}

			auto connectionsNode = nodeNode["Connections"];
			for (const auto& connectionNode : connectionsNode)
			{
				GraphConnectionData& connectionData = nodeData.OutputConnections.emplace_back();
				connectionData.NodeID = connectionNode["NodeID"].as<uint32_t>();
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

	static bool HasRootMotion(const YAML::Node& node)
	{
		const auto baseNode = node["Animation"];
		if (!baseNode)
			return false;

		// Root Motion
		if (auto rootMotionNode = baseNode["RootMotion"])
			return true;

		return false;
	}

	static void SerializeRagdollBonesData(YAML::Emitter& out, const SkeletalRagdollBones& node)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << node.Name;
		out << YAML::Key << "Offset" << YAML::Value << Math::ToTransformMatrix(node.Settings.UserOffset);
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

	void Serializer::EmitBoneNode(YAML::Emitter& out, const BoneNode& node)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Transformation" << YAML::Value << node.Transformation;
		out << YAML::Key << "Name" << YAML::Value << node.Name;
		out << YAML::Key << "IsVirtual" << YAML::Value << node.bVirtualBone;

		if (node.Children.size())
		{
			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;

			for (const auto& child : node.Children)
				EmitBoneNode(out, child);

			out << YAML::EndSeq;
		}

		out << YAML::EndMap;
	}

	void Serializer::ReadBoneNode(const YAML::Node& baseNode, BoneNode& node)
	{
		node.Transformation = baseNode["Transformation"].as<glm::mat4>();
		node.Name = baseNode["Name"].as<std::string>();
		if (auto virtualNode = baseNode["IsVirtual"])
			node.bVirtualBone = virtualNode.as<bool>();

		const auto childrenNode = baseNode["Children"];
		if (childrenNode)
		{
			node.Children.reserve(childrenNode.size());
			for (const auto& childNode : childrenNode)
			{
				auto& child = node.Children.emplace_back();
				ReadBoneNode(childNode, child);
			}
		}
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
			case AssetType::StaticMesh:
				SerializeAssetStaticMesh(out, Cast<AssetStaticMesh>(asset));
				break;
			case AssetType::SkeletalMesh:
				SerializeAssetSkeletalMesh(out, Cast<AssetSkeletalMesh>(asset));
				break;
			case AssetType::Audio:
				SerializeAssetAudio(out, Cast<AssetAudio>(asset));
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
			case AssetType::SoundGroup:
				SerializeAssetSoundGroup(out, Cast<AssetSoundGroup>(asset));
				break;
			case AssetType::Entity:
				SerializeAssetEntity(out, Cast<AssetEntity>(asset));
				break;
			case AssetType::Animation:
				SerializeAssetAnimation(out, Cast<AssetAnimation>(asset));
				break;
			case AssetType::AnimationGraph:
				SerializeAssetAnimationGraph(out, Cast<AssetAnimationGraph>(asset));
				break;
			case AssetType::ParticleSystem:
				SerializeAssetParticleSystem(out, Cast<AssetParticleSystem>(asset));
				break;
			case AssetType::AnimationBlendSpace:
				SerializeAssetAnimationBlendSpace(out, Cast<AssetAnimationBlendSpace>(asset));
				break;
			default:
				EG_CORE_ASSERT(false);
				EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
		}
	}

	void Serializer::SerializeAssetTexture2D(YAML::Emitter& out, const Ref<AssetTexture2D>& asset)
	{
		const auto& texture = asset->GetTexture();
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));
		const bool bCompressed = asset->IsCompressed();
		const ScopedDataBuffer& ktxData = asset->GetKTXData();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Texture2D);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "FilterMode" << YAML::Value << Utils::GetEnumName(texture->GetFilterMode());
		out << YAML::Key << "AddressMode" << YAML::Value << Utils::GetEnumName(texture->GetAddressMode());
		out << YAML::Key << "Anisotropy" << YAML::Value << texture->GetAnisotropy();
		out << YAML::Key << "MipsCount" << YAML::Value << texture->GetMipsCount();
		out << YAML::Key << "Width" << YAML::Value << texture->GetWidth();
		out << YAML::Key << "Height" << YAML::Value << texture->GetHeight();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(asset->GetFormat());
		out << YAML::Key << "IsCompressed" << YAML::Value << bCompressed;
		out << YAML::Key << "IsNormalMap" << YAML::Value << asset->IsNormalMap();
		out << YAML::Key << "NeedAlpha" << YAML::Value << asset->DoesNeedAlpha();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		if (bCompressed)
			out << YAML::Key << "KTXData" << YAML::Value << YAML::Binary((uint8_t*)ktxData.Data(), ktxData.Size());
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
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::TextureCube);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(asset->GetFormat());
		out << YAML::Key << "LayerSize" << YAML::Value << textureCube->GetSize().x;
		out << YAML::Key << "PrefilterSize" << YAML::Value << textureCube->GetPrefilterSize();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetStaticMesh(YAML::Emitter& out, const Ref<AssetStaticMesh>& asset)
	{
		const auto& mesh = asset->GetMesh();
		DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(Vertex) };
		const size_t origVerticesDataSize = verticesBuffer.Size; // Required for decompression
		ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::StaticMesh);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();

		// AABB
		{
			const auto& aabb = mesh->GetAABB();
			out << YAML::Key << "AABB" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Min" << YAML::Value << aabb.Min;
			out << YAML::Key << "Max" << YAML::Value << aabb.Max;
			out << YAML::EndMap;
		}

		if (const uint32_t materialsCount = mesh->GetMaterialSlotsCount())
		{
			bool bAnyValid = false;
			for (uint32_t i = 0; i < materialsCount; ++i)
				if (const auto& materialAsset = mesh->GetMaterialAsset(i))
				{
					bAnyValid = true;
					break;
				}

			if (bAnyValid)
			{
				out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
				for (uint32_t i = 0; i < materialsCount; ++i)
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
		out << YAML::Key << "SizeVertices" << YAML::Value << origVerticesDataSize;
		out << YAML::Key << "Vertices" << YAML::Value << YAML::Binary((uint8_t*)compressedVertices.Data(), compressedVertices.Size());

		// Indices per material
		const uint32_t materialSlots = mesh->GetMaterialSlotsCount();
		out << YAML::Key << "IndicesPerMaterial" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(i), mesh->GetIndicesCount(i) * sizeof(Index) };
			const size_t origIndicesDataSize = indicesBuffer.Size; // Required for decompression
			ScopedDataBuffer compressedIndices(Compressor::Compress(indicesBuffer));

			out << YAML::BeginMap;
			out << YAML::Key << "SizeIndices" << YAML::Value << origIndicesDataSize;
			out << YAML::Key << "Indices" << YAML::Value << YAML::Binary((uint8_t*)compressedIndices.Data(), compressedIndices.Size());
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetSkeletalMesh(YAML::Emitter& out, const Ref<AssetSkeletalMesh>& asset)
	{
		const auto& mesh = asset->GetMesh();
		DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(SkeletalVertex) };
		const size_t origVerticesDataSize = verticesBuffer.Size; // Required for decompression
		ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SkeletalMesh);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();

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

		const auto& ragdollData = mesh->GetRagdollRoot();
		if (!ragdollData.Name.empty())
		{
			out << YAML::Key << "RagdollBonesData" << YAML::Value << YAML::BeginSeq;
			SerializeRagdollBonesData(out, ragdollData);
			out << YAML::EndSeq;
		}

		if (const uint32_t materialsCount = mesh->GetMaterialSlotsCount())
		{
			bool bAnyValid = false;
			for (uint32_t i = 0; i < materialsCount; ++i)
				if (const auto& materialAsset = mesh->GetMaterialAsset(i))
				{
					bAnyValid = true;
					break;
				}

			if (bAnyValid)
			{
				out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
				for (uint32_t i = 0; i < materialsCount; ++i)
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

			const auto& bones = skeletalInfo.BoneInfoMap;
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
		out << YAML::Key << "SizeVertices" << YAML::Value << origVerticesDataSize;
		out << YAML::Key << "Vertices" << YAML::Value << YAML::Binary((uint8_t*)compressedVertices.Data(), compressedVertices.Size());

		// Indices per material
		const uint32_t materialSlots = mesh->GetMaterialSlotsCount();
		out << YAML::Key << "IndicesPerMaterial" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < materialSlots; ++i)
		{
			DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(i), mesh->GetIndicesCount(i) * sizeof(Index) };
			const size_t origIndicesDataSize = indicesBuffer.Size; // Required for decompression
			ScopedDataBuffer compressedIndices(Compressor::Compress(indicesBuffer));

			out << YAML::BeginMap;
			out << YAML::Key << "SizeIndices" << YAML::Value << origIndicesDataSize;
			out << YAML::Key << "Indices" << YAML::Value << YAML::Binary((uint8_t*)compressedIndices.Data(), compressedIndices.Size());
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetAudio(YAML::Emitter& out, const Ref<AssetAudio>& asset)
	{
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Audio);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Volume" << YAML::Value << asset->GetAudio()->GetVolume();
		out << YAML::Key << "Pitch" << YAML::Value << asset->GetAudio()->GetPitch();
		out << YAML::Key << "Pan" << YAML::Value << asset->GetAudio()->GetPan();
		if (const auto& soundGroup = asset->GetSoundGroupAsset())
			out << YAML::Key << "SoundGroup" << YAML::Value << soundGroup->GetGUID();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetFont(YAML::Emitter& out, const Ref<AssetFont>& asset)
	{
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Font);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetMaterial(YAML::Emitter& out, const Ref<AssetMaterial>& asset)
	{
		const auto& material = asset->GetMaterial();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Material);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

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
		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetPhysicsMaterial(YAML::Emitter& out, const Ref<AssetPhysicsMaterial>& asset)
	{
		const auto& material = asset->GetMaterial();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::PhysicsMaterial);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		out << YAML::Key << "StaticFriction" << YAML::Value << material->GetStaticFriction();
		out << YAML::Key << "DynamicFriction" << YAML::Value << material->GetDynamicFriction();
		out << YAML::Key << "Bounciness" << YAML::Value << material->GetBounciness();

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetSoundGroup(YAML::Emitter& out, const Ref<AssetSoundGroup>& asset)
	{
		const auto& soundGroup = asset->GetSoundGroup();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SoundGroup);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		out << YAML::Key << "Volume" << YAML::Value << soundGroup->GetVolume();
		out << YAML::Key << "Pitch" << YAML::Value << soundGroup->GetPitch();
		out << YAML::Key << "IsPaused" << YAML::Value << soundGroup->IsPaused();
		out << YAML::Key << "IsMuted" << YAML::Value << soundGroup->IsMuted();

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetEntity(YAML::Emitter& out, const Ref<AssetEntity>& asset)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Entity);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		Serializer::SerializeEntity(out, *asset->GetEntity().get());

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetAnimation(YAML::Emitter& out, const Ref<AssetAnimation>& asset)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(asset->GetAssetType());
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Index" << YAML::Value << asset->GetAnimationIndex();
		out << YAML::Key << "Skeletal" << YAML::Value << asset->GetSkeletal()->GetGUID();
		Serializer::SerializeAnimation(out, *asset->GetAnimation().get());
		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetAnimationGraph(YAML::Emitter& out, const Ref<AssetAnimationGraph>& asset)
	{
		const auto& graph = asset->GetGraph();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::AnimationGraph);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

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

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetParticleSystem(YAML::Emitter& out, const Ref<AssetParticleSystem>& asset)
	{
		const auto& emitters = asset->GetEmitters();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::ParticleSystem);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		out << YAML::Key << "Emitters";
		out << YAML::BeginSeq;
		for (const auto& emitter : emitters)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << emitter.Name;
			if (const auto& asset = emitter.Texture)
				out << YAML::Key << "Texture" << YAML::Value << asset->GetGUID();
			out << YAML::Key << "ColorStart" << YAML::Value << emitter.ColorStart;
			out << YAML::Key << "ColorEnd" << YAML::Value << emitter.ColorEnd;

			out << YAML::Key << "VelocityMin" << YAML::Value << emitter.VelocityMin;
			out << YAML::Key << "VelocityMax" << YAML::Value << emitter.VelocityMax;

			out << YAML::Key << "VelocityCoefStart" << YAML::Value << emitter.VelocityCoefStart;
			out << YAML::Key << "VelocityCoefEnd" << YAML::Value << emitter.VelocityCoefEnd;

			out << YAML::Key << "RotationZStart" << YAML::Value << emitter.RotationZStart;
			out << YAML::Key << "RotationZEnd" << YAML::Value << emitter.RotationZEnd;

			out << YAML::Key << "SizeStart" << YAML::Value << emitter.SizeStart;
			out << YAML::Key << "SizeEnd" << YAML::Value << emitter.SizeEnd;
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
			out << YAML::Key << "NumParticles" << YAML::Value << emitter.NumParticles;
			out << YAML::Key << "NumParticlesRatio" << YAML::Value << emitter.NumParticlesRatio;
			out << YAML::Key << "FastForwardTo" << YAML::Value << emitter.FastForwardTo;
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
			out << YAML::Key << "CollisionMode" << YAML::Value << Utils::GetEnumName(emitter.CollisionMode);

			out << YAML::Key << "bDestroyImmediately" << YAML::Value << emitter.bDestroyImmediately;
			out << YAML::Key << "bEmit" << YAML::Value << emitter.bEmit;
			out << YAML::Key << "bExplode" << YAML::Value << emitter.bExplode;
			out << YAML::Key << "bApplyGravity" << YAML::Value << emitter.bApplyGravity;
			out << YAML::Key << "bAlphaBlending" << YAML::Value << emitter.bAlphaBlending;
			out << YAML::Key << "bAdditive" << YAML::Value << emitter.bAdditive;
			out << YAML::Key << "bBlendAnimation" << YAML::Value << emitter.bBlendAnimation;
			out << YAML::Key << "bFaceDirection" << YAML::Value << emitter.bFaceDirection;

			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetAnimationBlendSpace(YAML::Emitter& out, const Ref<AssetAnimationBlendSpace>& asset)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::AnimationBlendSpace);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "SkeletalMesh" << YAML::Value << asset->GetSkeletalMesh()->GetGUID();

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
			out << YAML::Key << "Coord" << YAML::Value << point.Vertex.Coord;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	void Serializer::DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb)
	{
		float minDistance = reverbNode["MinDistance"].as<float>();
		float maxDistance = reverbNode["MaxDistance"].as<float>();
		reverb.SetMinMaxDistance(minDistance, maxDistance);
		reverb.SetPreset(Utils::GetEnumFromName<ReverbPreset>(reverbNode["Preset"].as<std::string>()));
		reverb.SetActive(reverbNode["IsActive"].as<bool>());
	}

	void Serializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		if (entity.HasComponent<EntityAssetComponent>())
		{
			const auto& component = entity.GetComponent<EntityAssetComponent>();
			out << YAML::Key << "Asset" << YAML::Value << component.AssetGUID;
		}

		if (entity.HasComponent<EntitySceneNameComponent>())
		{
			const auto& sceneNameComponent = entity.GetComponent<EntitySceneNameComponent>();
			const auto& name = sceneNameComponent.Name;

			int parentID = -1;
			if (Entity parent = entity.GetParent())
				parentID = (int)parent.GetID();

			out << YAML::Key << "EntitySceneParams";
			out << YAML::BeginMap; //EntitySceneName

			out << YAML::Key << "Name" << YAML::Value << name;
			out << YAML::Key << "Parent" << YAML::Value << parentID;

			out << YAML::EndMap; //EntitySceneParams
		}

		if (entity.HasComponent<TransformComponent>())
		{
			const auto& worldTransform = entity.GetWorldTransform();
			const auto& relativeTransform = entity.GetRelativeTransform();

			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; //TransformComponent

			out << YAML::Key << "WorldLocation" << YAML::Value << worldTransform.Location;
			out << YAML::Key << "WorldRotation" << YAML::Value << worldTransform.Rotation;
			out << YAML::Key << "WorldScale" << YAML::Value << worldTransform.Scale3D;

			out << YAML::EndMap; //TransformComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = entity.GetComponent<CameraComponent>().Camera;

			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; //CameraComponent;

			out << YAML::Key << "Camera";
			out << YAML::BeginMap; //Camera

			out << YAML::Key << "ProjectionMode" << YAML::Value << Utils::GetEnumName(camera.GetProjectionMode());
			out << YAML::Key << "PerspectiveVerticalFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNearClip" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::Key << "ShadowFarClip" << YAML::Value << camera.GetShadowFarClip();
			out << YAML::Key << "CascadesSmoothTransitionAlpha" << YAML::Value << camera.GetCascadesSmoothTransitionAlpha();
			out << YAML::Key << "CascadesSplitAlpha" << YAML::Value << camera.GetCascadesSplitAlpha();

			out << YAML::EndMap; //Camera

			SerializeRelativeTransform(out, cameraComponent.GetRelativeTransform());

			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; //CameraComponent;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			const auto& spriteComponent = entity.GetComponent<SpriteComponent>();

			out << YAML::Key << "SpriteComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, spriteComponent.GetRelativeTransform());

			out << YAML::Key << "bAtlas" << YAML::Value << spriteComponent.IsAtlas();
			out << YAML::Key << "bCastsShadows" << YAML::Value << spriteComponent.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << spriteComponent.DoesReceiveDecals();
			out << YAML::Key << "AtlasSpriteCoords" << YAML::Value << spriteComponent.GetAtlasSpriteCoords();
			out << YAML::Key << "AtlasSpriteSize" << YAML::Value << spriteComponent.GetAtlasSpriteSize();
			out << YAML::Key << "AtlasSpriteSizeCoef" << YAML::Value << spriteComponent.GetAtlasSpriteSizeCoef();
			if (const auto& materialAsset = spriteComponent.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << materialAsset->GetGUID();

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<BillboardComponent>())
		{
			auto& component = entity.GetComponent<BillboardComponent>();

			out << YAML::Key << "BillboardComponent";
			out << YAML::BeginMap; //BillboardComponent

			SerializeRelativeTransform(out, component.GetRelativeTransform());
			if (component.TextureAsset)
				out << YAML::Key << "Texture" << YAML::Value << component.TextureAsset->GetGUID();

			out << YAML::EndMap; //BillboardComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			auto& smComponent = entity.GetComponent<StaticMeshComponent>();

			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; //StaticMeshComponent

			out << YAML::Key << "bCastsShadows" << YAML::Value << smComponent.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << smComponent.DoesReceiveDecals();

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());

			if (const auto& meshAsset = smComponent.GetMeshAsset())
				out << YAML::Key << "Mesh" << YAML::Value << meshAsset->GetGUID();
			
			if (const uint32_t materialsCount = smComponent.GetMaterialsSlotsCount())
			{
				out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
				for (uint32_t i = 0; i < materialsCount; ++i)
				{
					const auto& materialAsset = smComponent.GetMaterialAsset(i);
					out << YAML::BeginMap;
					out << YAML::Key << "Index" << YAML::Value << i;
					out << YAML::Key << "Material" << YAML::Value << (materialAsset ? materialAsset->GetGUID() : GUID(0, 0));
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}

			out << YAML::EndMap; //StaticMeshComponent
		}

		if (entity.HasComponent<SkeletalMeshComponent>())
		{
			auto& smComponent = entity.GetComponent<SkeletalMeshComponent>();

			out << YAML::Key << "SkeletalMeshComponent";
			out << YAML::BeginMap; //SkeletalMeshComponent

			out << YAML::Key << "bCastsShadows" << YAML::Value << smComponent.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << smComponent.DoesReceiveDecals();

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());

			if (const auto& meshAsset = smComponent.GetMeshAsset())
				out << YAML::Key << "Mesh" << YAML::Value << meshAsset->GetGUID();

			if (const uint32_t materialsCount = smComponent.GetMaterialsSlotsCount())
			{
				out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
				for (uint32_t i = 0; i < materialsCount; ++i)
				{
					const auto& materialAsset = smComponent.GetMaterialAsset(i);
					out << YAML::BeginMap;
					out << YAML::Key << "Index" << YAML::Value << i;
					out << YAML::Key << "Material" << YAML::Value << (materialAsset ? materialAsset->GetGUID() : GUID(0, 0));
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}

			out << YAML::Key << "AnimationType" << Utils::GetEnumName(smComponent.AnimType);
			if (const auto& animAsset = smComponent.GetAnimationAsset())
				out << YAML::Key << "AnimationClip" << YAML::Value << animAsset->GetGUID();
			if (const auto& graphAsset = smComponent.GetAnimationGraphAsset())
			{
				out << YAML::Key << "AnimationGraph" << YAML::Value << graphAsset->GetGUID();

				// Variables
				{
					const auto& graph = smComponent.GetAnimationGraph();
					const auto& variables = graph->GetVariables();
					out << YAML::Key << "Variables" << YAML::Value << YAML::BeginSeq;

					for (const auto& [name, var] : variables)
					{
						out << YAML::BeginMap;
						out << YAML::Key << "Name" << YAML::Value << name;
						SerializeGraphVar(out, var);

						out << YAML::EndMap;
					}

					out << YAML::EndSeq;
				}
			}
			out << YAML::Key << "ClipStartPos" << smComponent.CurrentClipPlayTime;
			out << YAML::Key << "ClipPlaybackSpeed" << smComponent.ClipPlaybackSpeed;
			out << YAML::Key << "ClipLooping" << smComponent.bClipLooping;
			out << YAML::Key << "RootMotionLockFlags" << (uint32_t)smComponent.GetRootMotionLockFlags();
			out << YAML::Key << "bRagdoll" << YAML::Value << smComponent.IsRagdollEnabled();

			out << YAML::EndMap; //SkeletalMeshComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			auto& pointLightComponent = entity.GetComponent<PointLightComponent>();

			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; //PointLightComponent

			SerializeRelativeTransform(out, pointLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << pointLightComponent.GetLightColor();
			out << YAML::Key << "Intensity" << YAML::Value << pointLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << pointLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "Radius" << YAML::Value << pointLightComponent.GetRadius();
			out << YAML::Key << "AffectsWorld" << YAML::Value << pointLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << pointLightComponent.DoesCastShadows();
			out << YAML::Key << "VisualizeRadius" << YAML::Value << pointLightComponent.VisualizeRadiusEnabled();
			out << YAML::Key << "IsVolumetric" << YAML::Value << pointLightComponent.IsVolumetricLight();

			out << YAML::EndMap; //PointLightComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();

			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; //DirectionalLightComponent

			SerializeRelativeTransform(out, directionalLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << directionalLightComponent.GetLightColor();
			out << YAML::Key << "Ambient" << YAML::Value << directionalLightComponent.Ambient;
			out << YAML::Key << "Intensity" << YAML::Value << directionalLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << directionalLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "AffectsWorld" << YAML::Value << directionalLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << directionalLightComponent.DoesCastShadows();
			out << YAML::Key << "IsVolumetric" << YAML::Value << directionalLightComponent.IsVolumetricLight();
			out << YAML::Key << "Visualize" << YAML::Value << directionalLightComponent.bVisualizeDirection;

			out << YAML::EndMap; //DirectionalLightComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			auto& spotLightComponent = entity.GetComponent<SpotLightComponent>();

			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap; //SpotLightComponent

			SerializeRelativeTransform(out, spotLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << spotLightComponent.GetLightColor();
			out << YAML::Key << "InnerCutOffAngle" << YAML::Value << spotLightComponent.GetInnerCutOffAngle();
			out << YAML::Key << "OuterCutOffAngle" << YAML::Value << spotLightComponent.GetOuterCutOffAngle();
			out << YAML::Key << "Intensity" << YAML::Value << spotLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << spotLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "Distance" << YAML::Value << spotLightComponent.GetDistance();
			out << YAML::Key << "AffectsWorld" << YAML::Value << spotLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << spotLightComponent.DoesCastShadows();
			out << YAML::Key << "VisualizeDistance" << YAML::Value << spotLightComponent.VisualizeDistanceEnabled();
			out << YAML::Key << "IsVolumetric" << YAML::Value << spotLightComponent.IsVolumetricLight();

			out << YAML::EndMap; //SpotLightComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			auto& scriptComponent = entity.GetComponent<ScriptComponent>();

			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap;

			out << YAML::Key << "ModuleName" << YAML::Value << scriptComponent.ModuleName;

			//Public Fields
			out << YAML::Key << "PublicFields";
			out << YAML::BeginMap;
			for (auto& it : scriptComponent.PublicFields)
			{
				PublicField& field = it.second;
				if (Serializer::HasSerializableType(field))
					Serializer::SerializePublicFieldValue(out, field);
			}
			out << YAML::EndMap;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RigidBodyComponent>())
		{
			auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

			out << YAML::Key << "RigidBodyComponent";
			out << YAML::BeginMap; //RigidBodyComponent

			out << YAML::Key << "BodyType" << YAML::Value << Utils::GetEnumName(rigidBodyComponent.BodyType);
			out << YAML::Key << "CollisionDetectionType" << YAML::Value << Utils::GetEnumName(rigidBodyComponent.CollisionDetection);
			out << YAML::Key << "Mass" << YAML::Value << rigidBodyComponent.GetMass();
			out << YAML::Key << "LinearDamping" << YAML::Value << rigidBodyComponent.GetLinearDamping();
			out << YAML::Key << "AngularDamping" << YAML::Value << rigidBodyComponent.GetAngularDamping();
			out << YAML::Key << "MaxLinearVelocity" << YAML::Value << rigidBodyComponent.GetMaxLinearVelocity();
			out << YAML::Key << "MaxAngularVelocity" << YAML::Value << rigidBodyComponent.GetMaxAngularVelocity();
			out << YAML::Key << "EnableGravity" << YAML::Value << rigidBodyComponent.IsGravityEnabled();
			out << YAML::Key << "IsKinematic" << YAML::Value << rigidBodyComponent.IsKinematic();
			out << YAML::Key << "LockFlags" << YAML::Value << (uint32_t)rigidBodyComponent.GetLockFlags();

			out << YAML::EndMap; //RigidBodyComponent
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			auto& collider = entity.GetComponent<BoxColliderComponent>();

			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap; //BoxColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "Size" << YAML::Value << collider.GetSize();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::EndMap; //BoxColliderComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			auto& collider = entity.GetComponent<SphereColliderComponent>();

			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; //SphereColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "Radius" << YAML::Value << collider.GetRadius();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::EndMap; //SphereColliderComponent
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			auto& collider = entity.GetComponent<CapsuleColliderComponent>();

			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap; //CapsuleColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "Radius" << YAML::Value << collider.GetRadius();
			out << YAML::Key << "Height" << YAML::Value << collider.GetHeight();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::EndMap; //CapsuleColliderComponent
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			auto& collider = entity.GetComponent<MeshColliderComponent>();

			out << YAML::Key << "MeshColliderComponent";
			out << YAML::BeginMap; //MeshColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& meshAsset = collider.GetCollisionMeshAsset())
				out << YAML::Key << "Mesh" << YAML::Value << meshAsset->GetGUID();

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsConvex" << YAML::Value << collider.IsConvex();
			out << YAML::Key << "IsTwoSided" << YAML::Value << collider.IsTwoSided();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::EndMap; //MeshColliderComponent
		}

		if (entity.HasComponent<AudioComponent>())
		{
			auto& audio = entity.GetComponent<AudioComponent>();

			out << YAML::Key << "AudioComponent";
			out << YAML::BeginMap; //AudioComponent

			SerializeRelativeTransform(out, audio.GetRelativeTransform());
			if (const auto& asset = audio.GetAudioAsset())
				out << YAML::Key << "Sound" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Volume" << YAML::Value << audio.GetVolume();
			out << YAML::Key << "Pitch" << YAML::Value << audio.GetPitch();
			out << YAML::Key << "Pan" << YAML::Value << audio.GetPan();
			out << YAML::Key << "LoopCount" << YAML::Value << audio.GetLoopCount();
			out << YAML::Key << "FFTSamples" << YAML::Value << audio.GetFFTSamples();
			out << YAML::Key << "FFTType" << YAML::Value << Utils::GetEnumName(audio.GetFFTType());
			out << YAML::Key << "IsLooping" << YAML::Value << audio.IsLooping();
			out << YAML::Key << "IsMuted" << YAML::Value << audio.IsMuted();
			out << YAML::Key << "IsStreaming" << YAML::Value << audio.IsStreaming();
			out << YAML::Key << "FFTEnabled" << YAML::Value << audio.IsFFTEnabled();
			out << YAML::Key << "MinDistance" << YAML::Value << audio.GetMinDistance();
			out << YAML::Key << "MaxDistance" << YAML::Value << audio.GetMaxDistance();
			out << YAML::Key << "RollOff" << YAML::Value << Utils::GetEnumName(audio.GetRollOffModel());
			out << YAML::Key << "Autoplay" << YAML::Value << audio.bAutoplay;
			out << YAML::Key << "EnableDopplerEffect" << YAML::Value << audio.bEnableDopplerEffect;
			out << YAML::Key << "Is3D" << YAML::Value << audio.Is3D();

			out << YAML::EndMap; //AudioComponent
		}

		if (entity.HasComponent<ReverbComponent>())
		{
			auto& reverb = entity.GetComponent<ReverbComponent>();

			out << YAML::Key << "ReverbComponent";
			out << YAML::BeginMap; //ReverbComponent

			SerializeRelativeTransform(out, reverb.GetRelativeTransform());
			out << YAML::Key << "bVisualize" << YAML::Value << reverb.IsVisualizeRadiusEnabled();
			Serializer::SerializeReverb(out, reverb.GetReverb());

			out << YAML::EndMap; //ReverbComponent
		}

		if (entity.HasComponent<TextComponent>())
		{
			auto& text = entity.GetComponent<TextComponent>();

			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap; //TextComponent

			SerializeRelativeTransform(out, text.GetRelativeTransform());
			if (const auto& asset = text.GetFontAsset())
				out << YAML::Key << "Font" << YAML::Value << asset->GetGUID();
			if (const auto& asset = text.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Text" << YAML::Value << text.GetText();
			out << YAML::Key << "Color" << YAML::Value << text.GetColor();
			out << YAML::Key << "IsLit" << YAML::Value << text.IsLit();
			out << YAML::Key << "bCastsShadows" << YAML::Value << text.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << text.DoesReceiveDecals();
			out << YAML::Key << "LineSpacing" << YAML::Value << text.GetLineSpacing();
			out << YAML::Key << "Kerning" << YAML::Value << text.GetKerning();
			out << YAML::Key << "MaxWidth" << YAML::Value << text.GetMaxWidth();

			out << YAML::EndMap; //TextComponent
		}

		if (entity.HasComponent<Text2DComponent>())
		{
			auto& text = entity.GetComponent<Text2DComponent>();

			out << YAML::Key << "Text2DComponent";
			out << YAML::BeginMap; //Text2DComponent

			if (const auto& asset = text.GetFontAsset())
				out << YAML::Key << "Font" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Text" << YAML::Value << text.GetText();
			out << YAML::Key << "Color" << YAML::Value << text.GetColor();
			out << YAML::Key << "LineSpacing" << YAML::Value << text.GetLineSpacing();
			out << YAML::Key << "Pos" << YAML::Value << text.GetPosition();
			out << YAML::Key << "Scale" << YAML::Value << text.GetScale();
			out << YAML::Key << "Rotation" << YAML::Value << text.GetRotation();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "Kerning" << YAML::Value << text.GetKerning();
			out << YAML::Key << "MaxWidth" << YAML::Value << text.GetMaxWidth();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();

			out << YAML::EndMap; //Text2DComponent
		}

		if (entity.HasComponent<Image2DComponent>())
		{
			auto& text = entity.GetComponent<Image2DComponent>();

			out << YAML::Key << "Image2DComponent";
			out << YAML::BeginMap; //Image2DComponent

			if (const auto& asset = text.GetTextureAsset())
				out << YAML::Key << "Texture" << YAML::Value << asset->GetGUID();
			out << YAML::Key << "Tint" << YAML::Value << text.GetTint();
			out << YAML::Key << "Pos" << YAML::Value << text.GetPosition();
			out << YAML::Key << "Scale" << YAML::Value << text.GetScale();
			out << YAML::Key << "Rotation" << YAML::Value << text.GetRotation();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();

			out << YAML::EndMap; //Image2DComponent
		}

		if (entity.HasComponent<ParticleSystemComponent>())
		{
			auto& system = entity.GetComponent<ParticleSystemComponent>();

			out << YAML::Key << "ParticleSystemComponent";
			out << YAML::BeginMap; //ParticleSystemComponent
			
			SerializeRelativeTransform(out, system.GetRelativeTransform());

			if (const auto& asset = system.GetAsset())
				out << YAML::Key << "ParticleSystem" << YAML::Value << asset->GetGUID();
			
			out << YAML::EndMap; //ParticleSystemComponent
		}

		if (entity.HasComponent<DecalComponent>())
		{
			auto& decal = entity.GetComponent<DecalComponent>();

			out << YAML::Key << "DecalComponent";
			out << YAML::BeginMap; //DecalComponent
			
			SerializeRelativeTransform(out, decal.GetRelativeTransform());
			out << YAML::Key << "SortPriority" << YAML::Value << decal.GetSortPriority();
			out << YAML::Key << "AdjustAspectRatio" << YAML::Value << decal.IsAdjustAspectRatioEnabled();
			if (const auto& asset = decal.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << asset->GetGUID();
			
			out << YAML::EndMap; //DecalComponent
		}

		if (entity.HasComponent<NavigationMeshComponent>())
		{
			auto& component = entity.GetComponent<NavigationMeshComponent>();

			out << YAML::Key << "NavigationMeshComponent";
			out << YAML::BeginMap; // NavigationMeshComponent

			SerializeRelativeTransform(out, component.GetRelativeTransform());
			out << YAML::Key << "bAutoRebuild" << YAML::Value << component.bAutoRebuild;

			// Settings
			{
				const auto& settings = component.GetSettings();

				out << YAML::Key << "Settings" << YAML::Value << YAML::BeginMap;

				// AABB
				out << YAML::Key << "AABB" << YAML::Value << YAML::BeginMap;
				out << YAML::Key << "Min" << YAML::Value << settings.AABB.Min;
				out << YAML::Key << "Max" << YAML::Value << settings.AABB.Max;
				out << YAML::EndMap;

				out << YAML::Key << "MaxQueryNodes" << YAML::Value << settings.MaxQueryNodes;
				out << YAML::Key << "ExpectedLayersPerTile" << YAML::Value << settings.ExpectedLayersPerTile;
				out << YAML::Key << "MaxLayers" << YAML::Value << settings.MaxLayers;
				out << YAML::Key << "MaxObstacles" << YAML::Value << settings.MaxObstacles;
				out << YAML::Key << "TileSize" << YAML::Value << settings.TileSize;
				out << YAML::Key << "CellSize" << YAML::Value << settings.CellSize;
				out << YAML::Key << "CellHeight" << YAML::Value << settings.CellHeight;
				out << YAML::Key << "MaxSlope" << YAML::Value << settings.MaxSlope;
				out << YAML::Key << "AgentHeight" << YAML::Value << settings.AgentHeight;
				out << YAML::Key << "AgentMaxClimb" << YAML::Value << settings.AgentMaxClimb;
				out << YAML::Key << "AgentRadius" << YAML::Value << settings.AgentRadius;
				out << YAML::Key << "EdgeMaxLen" << YAML::Value << settings.EdgeMaxLen;
				out << YAML::Key << "EdgeMaxError" << YAML::Value << settings.EdgeMaxError;
				out << YAML::Key << "RegionMinSize" << YAML::Value << settings.RegionMinSize;
				out << YAML::Key << "RegionMergeSize" << YAML::Value << settings.RegionMergeSize;
				out << YAML::Key << "VertsPerPoly" << YAML::Value << settings.VertsPerPoly;
				out << YAML::Key << "BorderSize" << YAML::Value << settings.BorderSize;
				out << YAML::Key << "FilterLowHangingObstacles" << YAML::Value << settings.FilterLowHangingObstacles;
				out << YAML::Key << "FilterLedgeSpans" << YAML::Value << settings.FilterLedgeSpans;
				out << YAML::Key << "FilterWalkableLowHeightSpans" << YAML::Value << settings.FilterWalkableLowHeightSpans;

				out << YAML::EndMap;
			}

			// Crowd Settings
			{
				const auto& settings = component.GetCrowdSettings();

				out << YAML::Key << "CrowdSettings" << YAML::Value << YAML::BeginMap;

				out << YAML::Key << "MaxAgents" << YAML::Value << settings.MaxAgents;
				out << YAML::Key << "MaxAgentRadius" << YAML::Value << settings.MaxAgentRadius;

				out << YAML::EndMap;
			}

			out << YAML::EndMap; // NavigationMeshComponent
		}

		if (entity.HasComponent<NavigationCrowdAgentComponent>())
		{
			auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();

			out << YAML::Key << "NavigationCrowdAgentComponent";
			out << YAML::BeginMap; // NavigationCrowdAgentComponent

			// Settings
			{
				const auto& settings = component.GetSettings();

				out << YAML::Key << "Settings" << YAML::Value << YAML::BeginMap;

				out << YAML::Key << "AgentRadius" << YAML::Value << settings.AgentRadius;
				out << YAML::Key << "AgentHeight" << YAML::Value << settings.AgentHeight;
				out << YAML::Key << "MaxAcceleration" << YAML::Value << settings.MaxAcceleration;
				out << YAML::Key << "MaxSpeed" << YAML::Value << settings.MaxSpeed;
				out << YAML::Key << "SeparationWeight" << YAML::Value << settings.SeparationWeight;
				out << YAML::Key << "ObstacleAvoidanceQuality" << YAML::Value << Utils::GetEnumName(settings.ObstacleAvoidanceQuality);
				out << YAML::Key << "bAnticipateTurns" << YAML::Value << settings.bAnticipateTurns;
				out << YAML::Key << "bOptimizeVis" << YAML::Value << settings.bOptimizeVis;
				out << YAML::Key << "bOptimizeTopo" << YAML::Value << settings.bOptimizeTopo;
				out << YAML::Key << "bSeparation" << YAML::Value << settings.bSeparation;

				out << YAML::EndMap;
			}

			out << YAML::EndMap; // NavigationMeshComponent
		}
	}

	void Serializer::DeserializeEntity(Entity deserializedEntity, const YAML::Node& entityNode)
	{
		if (auto node = entityNode["Asset"])
		{
			if (deserializedEntity.HasComponent<EntityAssetComponent>() == false)
				deserializedEntity.AddComponent<EntityAssetComponent>();
			
			auto& component = deserializedEntity.GetComponent<EntityAssetComponent>();
			component.AssetGUID = node.as<GUID>();
		}

		if (auto sceneNameComponentNode = entityNode["EntitySceneParams"])
		{
			if (deserializedEntity.HasComponent<EntitySceneNameComponent>() == false)
				deserializedEntity.AddComponent<EntitySceneNameComponent>();
			
			auto& sceneNameComponent = deserializedEntity.GetComponent<EntitySceneNameComponent>();
			sceneNameComponent.Name = sceneNameComponentNode["Name"].as<std::string>();
		}

		if (auto transformComponentNode = entityNode["TransformComponent"])
		{
			//Every entity has a transform component
			Transform worldTransform;

			worldTransform.Location = transformComponentNode["WorldLocation"].as<glm::vec3>();
			worldTransform.Rotation = transformComponentNode["WorldRotation"].as<Rotator>();
			worldTransform.Scale3D = transformComponentNode["WorldScale"].as<glm::vec3>();

			deserializedEntity.SetWorldTransform(worldTransform);
		}

		if (auto cameraComponentNode = entityNode["CameraComponent"])
		{
			auto& cameraComponent = deserializedEntity.AddComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;
			Transform relativeTransform;

			auto& cameraNode = cameraComponentNode["Camera"];
			camera.SetProjectionMode(Utils::GetEnumFromName<CameraProjectionMode>(cameraNode["ProjectionMode"].as<std::string>()));

			camera.SetPerspectiveVerticalFOV(cameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(cameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(cameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(cameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(cameraNode["OrthographicFarClip"].as<float>());
			if (auto node = cameraNode["ShadowFarClip"])
				camera.SetShadowFarClip(node.as<float>());
			if (auto node = cameraNode["CascadesSplitAlpha"])
				camera.SetCascadesSplitAlpha(node.as<float>());
			if (auto node = cameraNode["CascadesSmoothTransitionAlpha"])
				camera.SetCascadesSmoothTransitionAlpha(node.as<float>());

			DeserializeRelativeTransform(cameraComponentNode, relativeTransform);

			cameraComponent.SetRelativeTransform(relativeTransform);

			cameraComponent.Primary = cameraComponentNode["Primary"].as<bool>();
			cameraComponent.FixedAspectRatio = cameraComponentNode["FixedAspectRatio"].as<bool>();
		}

		if (auto spriteComponentNode = entityNode["SpriteComponent"])
		{
			auto& spriteComponent = deserializedEntity.AddComponent<SpriteComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(spriteComponentNode, relativeTransform);
			spriteComponent.SetRelativeTransform(relativeTransform);

			if (auto materialNode = spriteComponentNode["Material"])
				spriteComponent.SetMaterialAsset(GetAsset<AssetMaterial>(materialNode));

			if (auto node = spriteComponentNode["bAtlas"])
				spriteComponent.SetIsAtlas(node.as<bool>());
			if (auto node = spriteComponentNode["bCastsShadows"])
				spriteComponent.SetCastsShadows(node.as<bool>());
			if (auto node = spriteComponentNode["bReceivesDecals"])
				spriteComponent.SetReceivesDecals(node.as<bool>());
			if (auto node = spriteComponentNode["AtlasSpriteCoords"])
				spriteComponent.SetAtlasSpriteCoords(node.as<glm::vec2>());
			if (auto node = spriteComponentNode["AtlasSpriteSize"])
				spriteComponent.SetAtlasSpriteSize(node.as<glm::vec2>());
			if (auto node = spriteComponentNode["AtlasSpriteSizeCoef"])
				spriteComponent.SetAtlasSpriteSizeCoef(node.as<glm::vec2>());
		}

		if (auto billboardComponentNode = entityNode["BillboardComponent"])
		{
			auto& billboardComponent = deserializedEntity.AddComponent<BillboardComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(billboardComponentNode, relativeTransform);
			billboardComponent.TextureAsset = GetAsset<AssetTexture2D>(billboardComponentNode["Texture"]);

			billboardComponent.SetRelativeTransform(relativeTransform);
		}

		if (auto staticMeshComponentNode = entityNode["StaticMeshComponent"])
		{
			auto& smComponent = deserializedEntity.AddComponent<StaticMeshComponent>();
			smComponent.SetCastsShadows(staticMeshComponentNode["bCastsShadows"].as<bool>());
			if (auto node = staticMeshComponentNode["bReceivesDecals"])
				smComponent.SetReceivesDecals(node.as<bool>());

			Transform relativeTransform;
			DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto meshNode = staticMeshComponentNode["Mesh"])
				smComponent.SetMeshAsset(GetAsset<AssetStaticMesh>(meshNode));
			if (auto materialsNode = staticMeshComponentNode["Materials"])
				for (const auto& matNode : materialsNode)
					smComponent.SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));
		}

		if (auto skeletalMeshComponentNode = entityNode["SkeletalMeshComponent"])
		{
			auto& smComponent = deserializedEntity.AddComponent<SkeletalMeshComponent>();
			smComponent.SetCastsShadows(skeletalMeshComponentNode["bCastsShadows"].as<bool>());
			if (auto node = skeletalMeshComponentNode["bReceivesDecals"])
				smComponent.SetReceivesDecals(node.as<bool>());

			Transform relativeTransform;
			DeserializeRelativeTransform(skeletalMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto meshNode = skeletalMeshComponentNode["Mesh"])
				smComponent.SetMeshAsset(GetAsset<AssetSkeletalMesh>(meshNode));
			if (auto materialsNode = skeletalMeshComponentNode["Materials"])
				for (const auto& matNode : materialsNode)
					smComponent.SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));
			if (auto animationNode = skeletalMeshComponentNode["AnimationType"])
				smComponent.AnimType = Utils::GetEnumFromName<SkeletalMeshComponent::AnimationType>(animationNode.as<std::string>());
			if (auto animationNode = skeletalMeshComponentNode["AnimationClip"])
				smComponent.SetAnimationAsset(GetAsset<AssetAnimation>(animationNode));
			if (auto animationNode = skeletalMeshComponentNode["AnimationGraph"])
			{
				auto graphAsset = GetAsset<AssetAnimationGraph>(animationNode);
				smComponent.SetAnimationGraphAsset(graphAsset);

				// Variables
				if (graphAsset)
				{
					if (auto variablesNode = skeletalMeshComponentNode["Variables"])
					{
						const VariablesMap& variables = smComponent.GetAnimationGraph()->GetVariables();
						for (const auto& varNode : variablesNode)
						{
							const std::string varName = varNode["Name"].as<std::string>();
							if (auto it = variables.find(varName); it != variables.end())
							{
								auto deserializedVar = DeserializeGraphVar(varNode);
								if (deserializedVar->GetType() == it->second->GetType())
								{
									it->second->CopyValue(deserializedVar);
									it->second->bShowInUI = deserializedVar->bShowInUI;
								}
							}
						}
					}
				}
			}
			if (auto node = skeletalMeshComponentNode["ClipStartPos"])
				smComponent.CurrentClipPlayTime = node.as<float>();
			if (auto node = skeletalMeshComponentNode["ClipPlaybackSpeed"])
				smComponent.ClipPlaybackSpeed = node.as<float>();
			if (auto node = skeletalMeshComponentNode["ClipLooping"])
				smComponent.bClipLooping = node.as<bool>();
			if (auto node = skeletalMeshComponentNode["RootMotionLockFlags"])
				smComponent.SetRootMotionLockFlag((RootMotionLockFlag)node.as<uint32_t>());
			if (auto node = skeletalMeshComponentNode["bRagdoll"])
				smComponent.SetRagdollEnabled(node.as<bool>());
		}

		if (auto pointLightComponentNode = entityNode["PointLightComponent"])
		{
			auto& pointLightComponent = deserializedEntity.AddComponent<PointLightComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(pointLightComponentNode, relativeTransform);
			pointLightComponent.SetRelativeTransform(relativeTransform);

			pointLightComponent.SetLightColor(pointLightComponentNode["LightColor"].as<glm::vec3>());
			if (auto node = pointLightComponentNode["Intensity"])
				pointLightComponent.SetIntensity(node.as<float>());
			if (auto node = pointLightComponentNode["VolumetricFogIntensity"])
				pointLightComponent.SetVolumetricFogIntensity(node.as<float>());
			if (auto node = pointLightComponentNode["Radius"])
				pointLightComponent.SetRadius(node.as<float>());
			if (auto node = pointLightComponentNode["AffectsWorld"])
				pointLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = pointLightComponentNode["CastsShadows"])
				pointLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = pointLightComponentNode["VisualizeRadius"])
				pointLightComponent.SetVisualizeRadiusEnabled(node.as<bool>());
			if (auto node = pointLightComponentNode["IsVolumetric"])
				pointLightComponent.SetIsVolumetricLight(node.as<bool>());
		}

		if (auto directionalLightComponentNode = entityNode["DirectionalLightComponent"])
		{
			auto& directionalLightComponent = deserializedEntity.AddComponent<DirectionalLightComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(directionalLightComponentNode, relativeTransform);
			directionalLightComponent.SetRelativeTransform(relativeTransform);

			if (auto lightColorNode = directionalLightComponentNode["LightColor"])
				directionalLightComponent.SetLightColor(lightColorNode.as<glm::vec3>());
			if (auto ambientNode = directionalLightComponentNode["Ambient"])
				directionalLightComponent.Ambient = ambientNode.as<glm::vec3>();
			if (auto intensityNode = directionalLightComponentNode["Intensity"])
				directionalLightComponent.SetIntensity(intensityNode.as<float>());
			if (auto intensityNode = directionalLightComponentNode["VolumetricFogIntensity"])
				directionalLightComponent.SetVolumetricFogIntensity(intensityNode.as<float>());
			if (auto node = directionalLightComponentNode["AffectsWorld"])
				directionalLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = directionalLightComponentNode["CastsShadows"])
				directionalLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = directionalLightComponentNode["IsVolumetric"])
				directionalLightComponent.SetIsVolumetricLight(node.as<bool>());
			if (auto node = directionalLightComponentNode["Visualize"])
				directionalLightComponent.bVisualizeDirection = node.as<bool>();
		}

		if (auto spotLightComponentNode = entityNode["SpotLightComponent"])
		{
			auto& spotLightComponent = deserializedEntity.AddComponent<SpotLightComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(spotLightComponentNode, relativeTransform);
			spotLightComponent.SetRelativeTransform(relativeTransform);

			spotLightComponent.SetLightColor(spotLightComponentNode["LightColor"].as<glm::vec3>());

			if (auto node = spotLightComponentNode["InnerCutOffAngle"])
			{
				spotLightComponent.SetInnerCutOffAngle(node.as<float>());
				spotLightComponent.SetOuterCutOffAngle(spotLightComponentNode["OuterCutOffAngle"].as<float>());
			}
			if (auto node = spotLightComponentNode["Intensity"])
				spotLightComponent.SetIntensity(node.as<float>());
			if (auto node = spotLightComponentNode["VolumetricFogIntensity"])
				spotLightComponent.SetVolumetricFogIntensity(node.as<float>());
			if (auto node = spotLightComponentNode["Distance"])
				spotLightComponent.SetDistance(node.as<float>());
			if (auto node = spotLightComponentNode["AffectsWorld"])
				spotLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = spotLightComponentNode["CastsShadows"])
				spotLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = spotLightComponentNode["VisualizeDistance"])
				spotLightComponent.SetVisualizeDistanceEnabled(node.as<bool>());
			if (auto node = spotLightComponentNode["IsVolumetric"])
				spotLightComponent.SetIsVolumetricLight(node.as<bool>());
		}

		if (auto scriptComponentNode = entityNode["ScriptComponent"])
		{
			auto& scriptComponent = deserializedEntity.AddComponent<ScriptComponent>();

			scriptComponent.ModuleName = scriptComponentNode["ModuleName"].as<std::string>();
			ScriptEngine::InitEntityScript(deserializedEntity);

			auto publicFieldsNode = scriptComponentNode["PublicFields"];
			if (publicFieldsNode)
				Serializer::DeserializePublicFieldValues(publicFieldsNode, scriptComponent);
		}

		if (auto rigidBodyComponentNode = entityNode["RigidBodyComponent"])
		{
			auto& rigidBodyComponent = deserializedEntity.AddComponent<RigidBodyComponent>();

			rigidBodyComponent.BodyType = Utils::GetEnumFromName<PhysicsBodyType>(rigidBodyComponentNode["BodyType"].as<std::string>());
			rigidBodyComponent.CollisionDetection = Utils::GetEnumFromName<CollisionDetectionType>(rigidBodyComponentNode["CollisionDetectionType"].as<std::string>());
			rigidBodyComponent.SetMass(rigidBodyComponentNode["Mass"].as<float>());
			rigidBodyComponent.SetLinearDamping(rigidBodyComponentNode["LinearDamping"].as<float>());
			rigidBodyComponent.SetAngularDamping(rigidBodyComponentNode["AngularDamping"].as<float>());
			rigidBodyComponent.SetMaxLinearVelocity(rigidBodyComponentNode["MaxLinearVelocity"].as<float>());
			rigidBodyComponent.SetMaxAngularVelocity(rigidBodyComponentNode["MaxAngularVelocity"].as<float>());
			rigidBodyComponent.SetEnableGravity(rigidBodyComponentNode["EnableGravity"].as<bool>());
			rigidBodyComponent.SetIsKinematic(rigidBodyComponentNode["IsKinematic"].as<bool>());
			rigidBodyComponent.SetLockFlag(ActorLockFlag(rigidBodyComponentNode["LockFlags"].as<uint32_t>()));
		}

		if (auto boxColliderNode = entityNode["BoxColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<BoxColliderComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(boxColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(boxColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(boxColliderNode["IsTrigger"].as<bool>());
			collider.SetSize(boxColliderNode["Size"].as<glm::vec3>());
			collider.SetShowCollision(boxColliderNode["IsCollisionVisible"].as<bool>());
			if (auto node = boxColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = boxColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
		}

		if (auto sphereColliderNode = entityNode["SphereColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<SphereColliderComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(sphereColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(sphereColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(sphereColliderNode["IsTrigger"].as<bool>());
			collider.SetRadius(sphereColliderNode["Radius"].as<float>());
			collider.SetShowCollision(sphereColliderNode["IsCollisionVisible"].as<bool>());
			if (auto node = sphereColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = sphereColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
		}

		if (auto capsuleColliderNode = entityNode["CapsuleColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<CapsuleColliderComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(capsuleColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(capsuleColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(capsuleColliderNode["IsTrigger"].as<bool>());
			collider.SetRadius(capsuleColliderNode["Radius"].as<float>());
			collider.SetHeight(capsuleColliderNode["Height"].as<float>());
			collider.SetShowCollision(capsuleColliderNode["IsCollisionVisible"].as<bool>());
			if (auto node = capsuleColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = capsuleColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
		}

		if (auto meshColliderNode = entityNode["MeshColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<MeshColliderComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(meshColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(meshColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(meshColliderNode["IsTrigger"].as<bool>());
			collider.SetShowCollision(meshColliderNode["IsCollisionVisible"].as<bool>());
			collider.SetIsConvex(meshColliderNode["IsConvex"].as<bool>());
			if (auto node = meshColliderNode["IsTwoSided"])
				collider.SetIsTwoSided(node.as<bool>());
			if (auto meshNode = meshColliderNode["Mesh"])
				collider.SetCollisionMeshAsset(GetAsset<AssetStaticMesh>(meshNode));
			if (auto node = meshColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = meshColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
		}

		if (auto audioNode = entityNode["AudioComponent"])
		{
			auto& audio = deserializedEntity.AddComponent<AudioComponent>();
			Path soundPath;

			Transform relativeTransform;
			DeserializeRelativeTransform(audioNode, relativeTransform);
			audio.SetRelativeTransform(relativeTransform);
			auto asset = GetAsset<AssetAudio>(audioNode["Sound"]);

			float volume = audioNode["Volume"].as<float>();
			if (auto node = audioNode["Pitch"])
				audio.SetPitch(node.as<float>());
			if (auto node = audioNode["Pan"])
				audio.SetPan(node.as<float>());
			int loopCount = audioNode["LoopCount"].as<int>();
			if (auto node = audioNode["FFTSamples"])
				audio.SetFFTSamples(node.as<uint32_t>());
			if (auto node = audioNode["FFTType"])
				audio.SetFFTType(Utils::GetEnumFromName<FFTWindowType>(node.as<std::string>()));
			bool bLooping = audioNode["IsLooping"].as<bool>();
			bool bMuted = audioNode["IsMuted"].as<bool>();
			bool bStreaming = audioNode["IsStreaming"].as<bool>();
			if (auto node = audioNode["FFTEnabled"])
				audio.SetFFTEnabled(node.as<bool>());
			float minDistance = audioNode["MinDistance"].as<float>();
			float maxDistance = audioNode["MaxDistance"].as<float>();
			RollOffModel rollOff = Utils::GetEnumFromName<RollOffModel>(audioNode["RollOff"].as<std::string>());
			bool bAutoplay = audioNode["Autoplay"].as<bool>();
			bool bEnableDoppler = audioNode["EnableDopplerEffect"].as<bool>();
			if (auto node = audioNode["Is3D"])
				audio.SetIs3D(node.as<bool>());

			audio.SetVolume(volume);
			audio.SetLoopCount(loopCount);
			audio.SetLooping(bLooping);
			audio.SetMuted(bMuted);
			audio.SetMinMaxDistance(minDistance, maxDistance);
			audio.SetRollOffModel(rollOff);
			audio.SetStreaming(bStreaming);
			audio.bAutoplay = bAutoplay;
			audio.bEnableDopplerEffect = bEnableDoppler;

			audio.SetAudioAsset(asset);
		}

		if (auto reverbNode = entityNode["ReverbComponent"])
		{
			auto& reverb = deserializedEntity.AddComponent<ReverbComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(reverbNode, relativeTransform);
			reverb.SetRelativeTransform(relativeTransform);

			if (auto node = reverbNode["bVisualize"])
				reverb.SetVisualizeRadiusEnabled(node.as<bool>());

			if (auto node = reverbNode["Reverb"])
				Serializer::DeserializeReverb(node, reverb);
		}

		if (auto textNode = entityNode["TextComponent"])
		{
			auto& text = deserializedEntity.AddComponent<TextComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(textNode, relativeTransform);
			text.SetRelativeTransform(relativeTransform);

			text.SetFontAsset(GetAsset<AssetFont>(textNode["Font"]));
			text.SetMaterialAsset(GetAsset<AssetMaterial>(textNode["Material"]));
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			text.SetIsLit(textNode["IsLit"].as<bool>());
			if (auto node = textNode["bCastsShadows"])
				text.SetCastsShadows(node.as<bool>());
			if (auto node = textNode["bReceivesDecals"])
				text.SetReceivesDecals(node.as<bool>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			text.SetKerning(textNode["Kerning"].as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
		}

		if (auto textNode = entityNode["Text2DComponent"])
		{
			auto& text = deserializedEntity.AddComponent<Text2DComponent>();

			text.SetFontAsset(GetAsset<AssetFont>(textNode["Font"]));
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			text.SetPosition(textNode["Pos"].as<glm::vec2>());
			text.SetScale(textNode["Scale"].as<glm::vec2>());
			text.SetRotation(textNode["Rotation"].as<float>());
			text.SetIsVisible(textNode["IsVisible"].as<bool>());
			text.SetKerning(textNode["Kerning"].as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
			text.SetOpacity(textNode["Opacity"].as<float>());
		}

		if (auto imageNode = entityNode["Image2DComponent"])
		{
			auto& image2D = deserializedEntity.AddComponent<Image2DComponent>();

			Ref<AssetTexture2D> asset = GetAsset<AssetTexture2D>(imageNode["Texture"]);
			image2D.SetTextureAsset(asset);
			image2D.SetTint(imageNode["Tint"].as<glm::vec3>());
			image2D.SetPosition(imageNode["Pos"].as<glm::vec2>());
			image2D.SetScale(imageNode["Scale"].as<glm::vec2>());
			image2D.SetRotation(imageNode["Rotation"].as<float>());
			image2D.SetIsVisible(imageNode["IsVisible"].as<bool>());
			image2D.SetOpacity(imageNode["Opacity"].as<float>());
		}

		if (auto systemNode = entityNode["ParticleSystemComponent"])
		{
			auto& system = deserializedEntity.AddComponent<ParticleSystemComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(systemNode, relativeTransform);
			system.SetRelativeTransform(relativeTransform);
			system.SetAsset(GetAsset<AssetParticleSystem>(systemNode["ParticleSystem"]));
		}

		if (auto decalNode = entityNode["DecalComponent"])
		{
			auto& decal = deserializedEntity.AddComponent<DecalComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(decalNode, relativeTransform);
			decal.SetRelativeTransform(relativeTransform);
			if (auto node = decalNode["SortPriority"])
				decal.SetSortPriority(node.as<uint32_t>());
			if (auto node = decalNode["AdjustAspectRatio"])
				decal.SetAdjustAspectRatioEnabled(node.as<bool>());
			decal.SetMaterialAsset(GetAsset<AssetMaterial>(decalNode["Material"]));
		}

		if (auto componentNode = entityNode["NavigationMeshComponent"])
		{
			auto& component = deserializedEntity.AddComponent<NavigationMeshComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(componentNode, relativeTransform);
			component.SetRelativeTransform(relativeTransform);
			if (auto rebuildNode = componentNode["bAutoRebuild"])
				component.bAutoRebuild = rebuildNode.as<bool>();

			AINavigation::MeshSettings settings{};
			AINavigation::CrowdSettings crowdSettings{};
			if (auto settingsNode = componentNode["Settings"])
			{
				if (auto aabbNode = settingsNode["AABB"])
				{
					settings.AABB.Min = aabbNode["Min"].as<glm::vec3>();
					settings.AABB.Max = aabbNode["Max"].as<glm::vec3>();
				}
				settings.MaxQueryNodes = settingsNode["MaxQueryNodes"].as<uint32_t>();
				settings.ExpectedLayersPerTile = settingsNode["ExpectedLayersPerTile"].as<uint32_t>();
				settings.MaxLayers = settingsNode["MaxLayers"].as<uint32_t>();
				settings.MaxObstacles = settingsNode["MaxObstacles"].as<uint32_t>();
				settings.TileSize = settingsNode["TileSize"].as<uint32_t>();
				settings.CellSize = settingsNode["CellSize"].as<float>();
				settings.CellHeight = settingsNode["CellHeight"].as<float>();
				settings.MaxSlope = settingsNode["MaxSlope"].as<float>();
				settings.AgentHeight = settingsNode["AgentHeight"].as<float>();
				settings.AgentMaxClimb = settingsNode["AgentMaxClimb"].as<float>();
				settings.AgentRadius = settingsNode["AgentRadius"].as<float>();
				settings.EdgeMaxLen = settingsNode["EdgeMaxLen"].as<float>();
				settings.EdgeMaxError = settingsNode["EdgeMaxError"].as<float>();
				settings.RegionMinSize = settingsNode["RegionMinSize"].as<float>();
				settings.RegionMergeSize = settingsNode["RegionMergeSize"].as<float>();
				settings.VertsPerPoly = settingsNode["VertsPerPoly"].as<uint32_t>();
				settings.BorderSize = settingsNode["BorderSize"].as<uint32_t>();
				settings.FilterLowHangingObstacles = settingsNode["FilterLowHangingObstacles"].as<bool>();
				settings.FilterLedgeSpans = settingsNode["FilterLedgeSpans"].as<bool>();
				settings.FilterWalkableLowHeightSpans = settingsNode["FilterWalkableLowHeightSpans"].as<bool>();
			}

			if (auto settingsNode = componentNode["CrowdSettings"])
			{
				crowdSettings.MaxAgents = settingsNode["MaxAgents"].as<uint32_t>();
				if (auto node = settingsNode["MaxAgentRadius"])
					crowdSettings.MaxAgentRadius = node.as<float>();
				component.SetCrowdSettings(crowdSettings);
			}

			component.SetSettings(settings);
		}

		if (auto componentNode = entityNode["NavigationCrowdAgentComponent"])
		{
			auto& component = deserializedEntity.AddComponent<NavigationCrowdAgentComponent>();

			AINavigation::AgentSettings settings{};
			if (auto settingsNode = componentNode["Settings"])
			{
				settings.AgentRadius = settingsNode["AgentRadius"].as<float>();
				settings.AgentHeight = settingsNode["AgentHeight"].as<float>();
				settings.MaxAcceleration = settingsNode["MaxAcceleration"].as<float>();
				settings.MaxSpeed = settingsNode["MaxSpeed"].as<float>();
				settings.SeparationWeight = settingsNode["SeparationWeight"].as<float>();
				settings.ObstacleAvoidanceQuality = Utils::GetEnumFromName<AINavigation::AgentObstacleAvoidanceQuality>(settingsNode["ObstacleAvoidanceQuality"].as<std::string>());
				settings.bAnticipateTurns = settingsNode["bAnticipateTurns"].as<bool>();
				settings.bOptimizeVis = settingsNode["bOptimizeVis"].as<bool>();
				settings.bOptimizeTopo = settingsNode["bOptimizeTopo"].as<bool>();
				settings.bSeparation = settingsNode["bSeparation"].as<bool>();
			}

			component.SetSettings(settings);
		}
	}

	void Serializer::SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform)
	{
		out << YAML::Key << "RelativeLocation" << YAML::Value << relativeTransform.Location;
		out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
		out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;
	}

	void Serializer::SerializeRendererSettings(YAML::Emitter& out, const SceneRendererSettings& settings)
	{
		const auto& bloomSettings = settings.BloomSettings;
		const auto& ssaoSettings = settings.SSAOSettings;
		const auto& gtaoSettings = settings.GTAOSettings;
		const auto& fogSettings = settings.FogSettings;
		const auto& volumetricSettings = settings.VolumetricSettings;
		const auto& shadowSettings = settings.ShadowsSettings;
		const auto& photoLinearParams = settings.PhotoLinearTonemappingParams;
		const auto& filmicParams = settings.FilmicTonemappingParams;
		const auto& dofSettings = settings.DOFSettings;
		const auto& motionBlurSettings = settings.MotionBlur;
		const auto& autoExposureSettings = settings.AutoExposure;
		const auto& sssr = settings.ScreenSpaceReflections;

		out << YAML::Key << "RendererSettings" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "SoftShadows" << YAML::Value << settings.bEnableSoftShadows;
		out << YAML::Key << "TranslucentShadows" << YAML::Value << settings.bTranslucentShadows;
		out << YAML::Key << "ShadowsSmoothTransition" << YAML::Value << settings.bEnableCSMSmoothTransition;
		out << YAML::Key << "StutterlessShaders" << YAML::Value << settings.bStutterlessShaders;
		out << YAML::Key << "EnableObjectPicking" << YAML::Value << settings.bEnableObjectPicking;
		out << YAML::Key << "Enable2DObjectPicking" << YAML::Value << settings.bEnable2DObjectPicking;
		out << YAML::Key << "SortOpaqueParticles" << YAML::Value << settings.bSortOpaqueParticles;
		out << YAML::Key << "LineWidth" << YAML::Value << settings.LineWidth;
		out << YAML::Key << "GridScale" << YAML::Value << settings.GridScale;
		out << YAML::Key << "TransparencyLayers" << YAML::Value << settings.TransparencyLayers;
		out << YAML::Key << "AO" << YAML::Value << Utils::GetEnumName(settings.AO);
		out << YAML::Key << "AA" << YAML::Value << Utils::GetEnumName(settings.AA);
		out << YAML::Key << "Gamma" << YAML::Value << settings.Gamma;
		out << YAML::Key << "Exposure" << YAML::Value << settings.Exposure;
		out << YAML::Key << "TonemappingMethod" << YAML::Value << Utils::GetEnumName(settings.Tonemapping);

		out << YAML::Key << "Bloom Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Threshold" << YAML::Value << bloomSettings.Threshold;
		out << YAML::Key << "Intensity" << YAML::Value << bloomSettings.Intensity;
		out << YAML::Key << "DirtIntensity" << YAML::Value << bloomSettings.DirtIntensity;
		out << YAML::Key << "Knee" << YAML::Value << bloomSettings.Knee;
		out << YAML::Key << "bEnable" << YAML::Value << bloomSettings.bEnable;
		if (bloomSettings.Dirt)
			out << YAML::Key << "Dirt" << YAML::Value << bloomSettings.Dirt->GetGUID();
		out << YAML::EndMap; // Bloom Settings

		out << YAML::Key << "SSAO Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << ssaoSettings.GetNumberOfSamples();
		out << YAML::Key << "Radius" << YAML::Value << ssaoSettings.GetRadius();
		out << YAML::Key << "Bias" << YAML::Value << ssaoSettings.GetBias();
		out << YAML::EndMap; // SSAO Settings

		out << YAML::Key << "GTAO Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << gtaoSettings.GetNumberOfSamples();
		out << YAML::Key << "Radius" << YAML::Value << gtaoSettings.GetRadius();
		out << YAML::EndMap; // GTAO Settings

		out << YAML::Key << "Fog Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value << fogSettings.Color;
		out << YAML::Key << "MinDistance" << YAML::Value << fogSettings.MinDistance;
		out << YAML::Key << "MaxDistance" << YAML::Value << fogSettings.MaxDistance;
		out << YAML::Key << "Density" << YAML::Value << fogSettings.Density;
		out << YAML::Key << "Equation" << YAML::Value << Utils::GetEnumName(fogSettings.Equation);
		out << YAML::Key << "bEnable" << YAML::Value << fogSettings.bEnable;
		out << YAML::EndMap; // Fog Settings

		out << YAML::Key << "Volumetric Light Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << volumetricSettings.Albedo;
		out << YAML::Key << "Samples" << YAML::Value << volumetricSettings.Samples;
		out << YAML::Key << "MaxScatteringDistance" << YAML::Value << volumetricSettings.MaxScatteringDistance;
		out << YAML::Key << "FogSpeed" << YAML::Value << volumetricSettings.FogSpeed;
		out << YAML::Key << "Anisotropy" << YAML::Value << volumetricSettings.Anisotropy;
		out << YAML::Key << "bFogEnable" << YAML::Value << volumetricSettings.bFogEnable;
		out << YAML::Key << "bEnable" << YAML::Value << volumetricSettings.bEnable;
		out << YAML::EndMap; // Volumetric Light Settings

		out << YAML::Key << "Shadow Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "PointLightSize" << YAML::Value << shadowSettings.PointLightShadowMapSize;
		out << YAML::Key << "SpotLightSize" << YAML::Value << shadowSettings.SpotLightShadowMapSize;
		out << YAML::Key << "DirLightSizes" << YAML::Value << shadowSettings.DirLightShadowMapSizes;
		out << YAML::EndMap; // Shadow Settings

		out << YAML::Key << "PhotoLinear Tonemapping";
		out << YAML::BeginMap;
		out << YAML::Key << "Sensitivity" << YAML::Value << photoLinearParams.Sensitivity;
		out << YAML::Key << "ExposureTime" << YAML::Value << photoLinearParams.ExposureTime;
		out << YAML::Key << "FStop" << YAML::Value << photoLinearParams.FStop;
		out << YAML::EndMap; //PhotoLinearTonemappingSettings

		out << YAML::Key << "Filmic Tonemapping";
		out << YAML::BeginMap;
		out << YAML::Key << "WhitePoint" << YAML::Value << filmicParams.WhitePoint;
		out << YAML::EndMap; //FilmicTonemappingSettings

		out << YAML::Key << "DOF";
		out << YAML::BeginMap;
		out << YAML::Key << "ApertureShape" << YAML::Value << dofSettings.ApertureShape;
		out << YAML::Key << "ApertureSize" << YAML::Value << dofSettings.ApertureSize;
		out << YAML::Key << "FocalLength" << YAML::Value << dofSettings.FocalLength;
		out << YAML::Key << "COCScale" << YAML::Value << dofSettings.COCScale;
		out << YAML::Key << "MaxCOC" << YAML::Value << dofSettings.MaxCOC;
		out << YAML::Key << "bDebug" << YAML::Value << dofSettings.bDebugOutput;
		out << YAML::EndMap; //DOF

		out << YAML::Key << "MotionBlur";
		out << YAML::BeginMap;
		out << YAML::Key << "bEnable" << YAML::Value << motionBlurSettings.bEnable;
		out << YAML::Key << "NumSamples" << YAML::Value << motionBlurSettings.NumSamples;
		out << YAML::Key << "Strength" << YAML::Value << motionBlurSettings.Strength;
		out << YAML::Key << "bDebug" << YAML::Value << motionBlurSettings.bDebugOutput;
		out << YAML::EndMap; //MotionBlur

		out << YAML::Key << "AutoExposure";
		out << YAML::BeginMap;
		out << YAML::Key << "MinLogLum" << YAML::Value << autoExposureSettings.MinLogLum;
		out << YAML::Key << "MaxLogLum" << YAML::Value << autoExposureSettings.MaxLogLum;
		out << YAML::Key << "AdaptationSpeed" << YAML::Value << autoExposureSettings.AdaptationSpeed;
		out << YAML::Key << "AdaptationKey" << YAML::Value << autoExposureSettings.AdaptationKey;
		out << YAML::Key << "bEnable" << YAML::Value << autoExposureSettings.bEnable;
		out << YAML::Key << "bHalfResolution" << YAML::Value << autoExposureSettings.bHalfResolution;
		out << YAML::EndMap; //AutoExposure

		out << YAML::Key << "SSSR";
		out << YAML::BeginMap;
		out << YAML::Key << "RoughnessThreshold" << YAML::Value << sssr.RoughnessThreshold;
		out << YAML::Key << "SamplesPerQuad" << YAML::Value << sssr.SamplesPerQuad;
		out << YAML::Key << "MaxTraversalIterations" << YAML::Value << sssr.MaxTraversalIterations;
		out << YAML::Key << "bEnable" << YAML::Value << sssr.bEnable;
		out << YAML::EndMap; //SSSR

		out << YAML::EndMap;
	}

	void Serializer::SerializeAnimation(YAML::Emitter& out, const SkeletalMeshAnimation& anim)
	{
		out << YAML::Key << "Animation" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "Duration" << YAML::Value << anim.Duration;
		out << YAML::Key << "TicksPerSecond" << YAML::Value << anim.TicksPerSecond;
		out << YAML::Key << "bInPlace" << YAML::Value << anim.bInPlace;

		if (anim.HasRootMotion())
		{
			out << YAML::Key << "RootMotion";
			{
				out << YAML::Value;
				out << YAML::BeginMap;

				out << YAML::Key << "Locations" << YAML::Value << YAML::Binary((uint8_t*)anim.RootMotion.Locations.data(), anim.RootMotion.Locations.size() * sizeof(KeyPosition));
				out << YAML::Key << "Rotations" << YAML::Value << YAML::Binary((uint8_t*)anim.RootMotion.Rotations.data(), anim.RootMotion.Rotations.size() * sizeof(KeyRotation));
				out << YAML::Key << "Scales" << YAML::Value << YAML::Binary((uint8_t*)anim.RootMotion.Scales.data(), anim.RootMotion.Scales.size() * sizeof(KeyScale));

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

		out << YAML::Key << "Bones";
		{
			out << YAML::Value << YAML::BeginSeq;

			const auto& bones = anim.Bones;
			for (auto& [name, data] : bones)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << name;
				out << YAML::Key << "Locations" << YAML::Value << YAML::Binary((uint8_t*)data.Locations.data(), data.Locations.size() * sizeof(KeyPosition));
				out << YAML::Key << "Rotations" << YAML::Value << YAML::Binary((uint8_t*)data.Rotations.data(), data.Rotations.size() * sizeof(KeyRotation));
				out << YAML::Key << "Scales" << YAML::Value << YAML::Binary((uint8_t*)data.Scales.data(), data.Scales.size() * sizeof(KeyScale));
				out << YAML::Key << "ID" << YAML::Value << data.BoneID;
				out << YAML::EndMap;
			}

			out << YAML::EndSeq;
		}

		out << YAML::EndMap;
	}

	void Serializer::DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform)
	{
		if (auto n = node["RelativeLocation"])
			relativeTransform.Location = n.as<glm::vec3>();
		if (auto n = node["RelativeRotation"])
			relativeTransform.Rotation = n.as<Rotator>();
		if (auto n = node["RelativeScale"])
			relativeTransform.Scale3D = n.as<glm::vec3>();
	}

	void Serializer::DeserializeRendererSettings(YAML::Node& node, SceneRendererSettings& settings)
	{
		auto data = node["RendererSettings"];
		if (!data)
		{
			EG_CORE_ERROR("Failed to deserialize renderer settings");
			return;
		}

		if (auto softShadows = data["SoftShadows"])
			settings.bEnableSoftShadows = softShadows.as<bool>();
		if (auto translucentShadows = data["TranslucentShadows"])
			settings.bTranslucentShadows = translucentShadows.as<bool>();
		if (auto smoothShadows = data["ShadowsSmoothTransition"])
			settings.bEnableCSMSmoothTransition = smoothShadows.as<bool>();
		if (auto stutterless = data["StutterlessShaders"])
			settings.bStutterlessShaders = stutterless.as<bool>();
		if (auto objectPicking = data["EnableObjectPicking"])
			settings.bEnableObjectPicking = objectPicking.as<bool>();
		if (auto objectPicking = data["Enable2DObjectPicking"])
			settings.bEnable2DObjectPicking = objectPicking.as<bool>();
		if (auto sortOpaqueParticles = data["SortOpaqueParticles"])
			settings.bSortOpaqueParticles = sortOpaqueParticles.as<bool>();
		if (auto lineWidthNode = data["LineWidth"])
			settings.LineWidth = lineWidthNode.as<float>();
		if (auto gridScaleNode = data["GridScale"])
			settings.GridScale = gridScaleNode.as<float>();
		if (auto layersNode = data["TransparencyLayers"])
			settings.TransparencyLayers = layersNode.as<uint32_t>();
		if (auto node = data["AO"])
			settings.AO = Utils::GetEnumFromName<AmbientOcclusion>(node.as<std::string>());
		if (auto node = data["AA"])
			settings.AA = Utils::GetEnumFromName<AAMethod>(node.as<std::string>());
		if (auto gammaNode = data["Gamma"])
			settings.Gamma = gammaNode.as<float>();
		if (auto exposureNode = data["Exposure"])
			settings.Exposure = exposureNode.as<float>();
		if (auto tonemappingNode = data["TonemappingMethod"])
			settings.Tonemapping = Utils::GetEnumFromName<TonemappingMethod>(tonemappingNode.as<std::string>());

		if (auto bloomSettingsNode = data["Bloom Settings"])
		{
			settings.BloomSettings.Threshold = bloomSettingsNode["Threshold"].as<float>();
			settings.BloomSettings.Intensity = bloomSettingsNode["Intensity"].as<float>();
			settings.BloomSettings.DirtIntensity = bloomSettingsNode["DirtIntensity"].as<float>();
			settings.BloomSettings.Knee = bloomSettingsNode["Knee"].as<float>();
			settings.BloomSettings.bEnable = bloomSettingsNode["bEnable"].as<bool>();
			if (auto node = bloomSettingsNode["Dirt"])
			{
				Ref<Asset> asset;
				if (AssetManager::Get(node.as<GUID>(), &asset))
					settings.BloomSettings.Dirt = Cast<AssetTexture2D>(asset);
			}
		}

		if (auto ssaoSettingsNode = data["SSAO Settings"])
		{
			settings.SSAOSettings.SetNumberOfSamples(ssaoSettingsNode["Samples"].as<uint32_t>());
			settings.SSAOSettings.SetRadius(ssaoSettingsNode["Radius"].as<float>());
			settings.SSAOSettings.SetBias(ssaoSettingsNode["Bias"].as<float>());
		}

		if (auto gtaoSettingsNode = data["GTAO Settings"])
		{
			settings.GTAOSettings.SetNumberOfSamples(gtaoSettingsNode["Samples"].as<uint32_t>());
			settings.GTAOSettings.SetRadius(gtaoSettingsNode["Radius"].as<float>());
		}

		if (auto fogSettingsNode = data["Fog Settings"])
		{
			settings.FogSettings.Color = fogSettingsNode["Color"].as<glm::vec3>();
			settings.FogSettings.MinDistance = fogSettingsNode["MinDistance"].as<float>();
			settings.FogSettings.MaxDistance = fogSettingsNode["MaxDistance"].as<float>();
			settings.FogSettings.Density = fogSettingsNode["Density"].as<float>();
			settings.FogSettings.Equation = Utils::GetEnumFromName<FogEquation>(fogSettingsNode["Equation"].as<std::string>());
			settings.FogSettings.bEnable = fogSettingsNode["bEnable"].as<bool>();
		}

		if (auto volumetricSettingsNode = data["Volumetric Light Settings"])
		{
			if (auto node = volumetricSettingsNode["Albedo"])
				settings.VolumetricSettings.Albedo = node.as<glm::vec3>();
			settings.VolumetricSettings.Samples = volumetricSettingsNode["Samples"].as<uint32_t>();
			settings.VolumetricSettings.MaxScatteringDistance = volumetricSettingsNode["MaxScatteringDistance"].as<float>();
			if (auto node = volumetricSettingsNode["FogSpeed"])
				settings.VolumetricSettings.FogSpeed = node.as<float>();
			if (auto node = volumetricSettingsNode["Anisotropy"])
				settings.VolumetricSettings.Anisotropy = node.as<float>();
			settings.VolumetricSettings.bFogEnable = volumetricSettingsNode["bFogEnable"].as<bool>();
			settings.VolumetricSettings.bEnable = volumetricSettingsNode["bEnable"].as<bool>();
		}

		if (auto shadowSettingsNode = data["Shadow Settings"])
		{
			settings.ShadowsSettings.PointLightShadowMapSize = shadowSettingsNode["PointLightSize"].as<uint32_t>();
			settings.ShadowsSettings.SpotLightShadowMapSize = shadowSettingsNode["SpotLightSize"].as<uint32_t>();
			settings.ShadowsSettings.DirLightShadowMapSizes = shadowSettingsNode["DirLightSizes"].as<std::vector<uint32_t>>();
		}

		if (auto photolinearNode = data["PhotoLinear Tonemapping"])
		{
			PhotoLinearTonemappingSettings params;
			params.Sensitivity = photolinearNode["Sensitivity"].as<float>();
			params.ExposureTime = photolinearNode["ExposureTime"].as<float>();
			params.FStop = photolinearNode["FStop"].as<float>();

			settings.PhotoLinearTonemappingParams = params;
		}

		if (auto filmicNode = data["Filmic Tonemapping"])
		{
			settings.FilmicTonemappingParams.WhitePoint = filmicNode["WhitePoint"].as<float>();
		}

		if (auto dofNode = data["DOF"])
		{
			settings.DOFSettings.ApertureShape = dofNode["ApertureShape"].as<glm::vec2>();
			settings.DOFSettings.ApertureSize = dofNode["ApertureSize"].as<float>();
			settings.DOFSettings.FocalLength = dofNode["FocalLength"].as<float>();
			settings.DOFSettings.COCScale = dofNode["COCScale"].as<float>();
			settings.DOFSettings.MaxCOC = dofNode["MaxCOC"].as<float>();
			settings.DOFSettings.bDebugOutput = dofNode["bDebug"].as<bool>();
		}

		if (auto motionBlurNode = data["MotionBlur"])
		{
			settings.MotionBlur.bEnable = motionBlurNode["bEnable"].as<bool>();
			settings.MotionBlur.NumSamples = motionBlurNode["NumSamples"].as<uint32_t>();
			settings.MotionBlur.Strength = motionBlurNode["Strength"].as<float>();
			settings.MotionBlur.bDebugOutput = motionBlurNode["bDebug"].as<bool>();
		}

		if (auto autoExposureNode = data["AutoExposure"])
		{
			settings.AutoExposure.MinLogLum = autoExposureNode["MinLogLum"].as<float>();
			settings.AutoExposure.MaxLogLum = autoExposureNode["MaxLogLum"].as<float>();
			settings.AutoExposure.AdaptationSpeed = autoExposureNode["AdaptationSpeed"].as<float>();
			settings.AutoExposure.AdaptationKey = autoExposureNode["AdaptationKey"].as<float>();
			settings.AutoExposure.bEnable = autoExposureNode["bEnable"].as<bool>();
			settings.AutoExposure.bHalfResolution = autoExposureNode["bHalfResolution"].as<bool>();
		}

		if (auto sssrNode = data["SSSR"])
		{
			settings.ScreenSpaceReflections.RoughnessThreshold = sssrNode["RoughnessThreshold"].as<float>();
			settings.ScreenSpaceReflections.SamplesPerQuad = sssrNode["SamplesPerQuad"].as<uint32_t>();
			settings.ScreenSpaceReflections.MaxTraversalIterations = sssrNode["MaxTraversalIterations"].as<uint32_t>();
			settings.ScreenSpaceReflections.bEnable = sssrNode["bEnable"].as<bool>();
		}
	}

	void Serializer::DeserializeAnimation(const YAML::Node& node, SkeletalMeshAnimation& animation)
	{
		const auto baseNode = node["Animation"];
		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to deserialize animation");
			return;
		}

		animation.Duration = baseNode["Duration"].as<float>();
		animation.TicksPerSecond = baseNode["TicksPerSecond"].as<float>();
		if (auto inPlaceNode = baseNode["bInPlace"])
			animation.bInPlace = inPlaceNode.as<bool>();

		// Root Motion
		if (auto rootMotionNode = baseNode["RootMotion"])
		{
			// Locations
			{
				YAML::Binary binary = rootMotionNode["Locations"].as<YAML::Binary>();
				size_t binaryCount = binary.size() / sizeof(KeyPosition);
				animation.RootMotion.Locations.resize(binaryCount);
				memcpy(animation.RootMotion.Locations.data(), binary.data(), binary.size());
			}

			// Rotations
			{
				YAML::Binary binary = rootMotionNode["Rotations"].as<YAML::Binary>();
				size_t binaryCount = binary.size() / sizeof(KeyRotation);
				animation.RootMotion.Rotations.resize(binaryCount);
				memcpy(animation.RootMotion.Rotations.data(), binary.data(), binary.size());
			}

			// Scales
			{
				YAML::Binary binary = rootMotionNode["Scales"].as<YAML::Binary>();
				size_t binaryCount = binary.size() / sizeof(KeyScale);
				animation.RootMotion.Scales.resize(binaryCount);
				memcpy(animation.RootMotion.Scales.data(), binary.data(), binary.size());
			}
		}

		// Events
		if (auto eventsNode = baseNode["Events"])
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
			animation.Bones.clear();
			auto boneNodes = baseNode["Bones"];
			for (const auto& boneNode : boneNodes)
			{
				BoneAnimation animData;
				std::string name = boneNode["Name"].as<std::string>();

				// Locations
				{
					YAML::Binary binary = boneNode["Locations"].as<YAML::Binary>();
					size_t binaryCount = binary.size() / sizeof(KeyPosition);
					animData.Locations.resize(binaryCount);
					memcpy(animData.Locations.data(), binary.data(), binary.size());
				}

				// Rotations
				{
					YAML::Binary binary = boneNode["Rotations"].as<YAML::Binary>();
					size_t binaryCount = binary.size() / sizeof(KeyRotation);
					animData.Rotations.resize(binaryCount);
					memcpy(animData.Rotations.data(), binary.data(), binary.size());
				}

				// Scales
				{
					YAML::Binary binary = boneNode["Scales"].as<YAML::Binary>();
					size_t binaryCount = binary.size() / sizeof(KeyScale);
					animData.Scales.resize(binaryCount);
					memcpy(animData.Scales.data(), binary.data(), binary.size());
				}

				animData.BoneID = boneNode["ID"].as<uint32_t>();

				animation.Bones.emplace(std::move(name), std::move(animData));
			}
		}
	}

	Ref<Asset> Serializer::DeserializeAsset(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
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
		case AssetType::StaticMesh:
			return DeserializeAssetStaticMesh(baseNode, pathToAsset, bReloadRaw);
		case AssetType::SkeletalMesh:
			return DeserializeAssetSkeletalMesh(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Audio:
			return DeserializeAssetAudio(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Font:
			return DeserializeAssetFont(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Material:
			return DeserializeAssetMaterial(baseNode, pathToAsset);
		case AssetType::PhysicsMaterial:
			return DeserializeAssetPhysicsMaterial(baseNode, pathToAsset);
		case AssetType::SoundGroup:
			return DeserializeAssetSoundGroup(baseNode, pathToAsset);
		case AssetType::Entity:
			return DeserializeAssetEntity(baseNode, pathToAsset);
		case AssetType::Animation:
			return DeserializeAssetAnimation(baseNode, pathToAsset, bReloadRaw);
		case AssetType::AnimationGraph:
			return DeserializeAssetAnimationGraph(baseNode, pathToAsset);
		case AssetType::ParticleSystem:
			return DeserializeAssetParticleSystem(baseNode, pathToAsset);
		case AssetType::AnimationBlendSpace:
			return DeserializeAssetAnimationBlendSpace(baseNode, pathToAsset);
		default:
			EG_CORE_ASSERT(false);
			EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
			return {};
		}
	}

	Ref<AssetTexture2D> Serializer::DeserializeAssetTexture2D(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Texture2D))
			return {};
		
		const Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
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
		const bool bNeedAlpha = baseNode["NeedAlpha"].as<bool>();
		const bool bCompressedTexture = baseNode["IsCompressed"].as<bool>();

		YAML::Binary yamlBinary;
		ScopedDataBuffer binary;
		DataBuffer ktxData;

		// Reload the data from disk
		// Or get the data from the asset file.
		// If the texture is compressed, an asset will contain compressed texture data that's ready to go.
		// Otherwise, it'll contain the data that needs to be loaded by stb_image
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
		}
		else if (auto baseDataNode = baseNode["Data"])
		{
			size_t origSize = baseDataNode["Size"].as<size_t>();

			yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
			binary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
			if (auto ktxNode = baseDataNode["KTXData"])
			{
				yamlBinary = ktxNode.as<YAML::Binary>();
				ktxData = DataBuffer((void*)yamlBinary.data(), yamlBinary.size());
			}
		}
		else
		{
			EG_CORE_ERROR("Failed to deserialize texture 2D: {}", pathToAsset.u8string());
			return {};
		}
		
		void* stbiImageData = nullptr;
		void* compressedTextureHandle = nullptr;
		bool bCompressionFailed = false;

		Ref<Texture2D> texture;
		if (bCompressedTexture)
		{
			// If reload is requested, we load the image data and then compress it.
			// Otherwise, we have a compressed data, so just load it.
			if (bReloadRaw)
			{
				const int desiredChannels = 4;
				stbiImageData = stbi_load_from_memory((uint8_t*)binary.Data(), (int)binary.Size(), &width, &height, &channels, desiredChannels);

				const size_t textureMemSize = desiredChannels * width * height;
				compressedTextureHandle = TextureCompressor::Compress(DataBuffer(stbiImageData, textureMemSize), glm::uvec2(width, height),
					specs.MipsCount, bNormalMap, bNeedAlpha);
			}
			else if (ktxData)
			{
				compressedTextureHandle = TextureCompressor::CreateFromCompressed(ktxData, bNeedAlpha);
			}

			// Required to update it in case of `bReloadRaw` was true
			if (compressedTextureHandle)
				ktxData = TextureCompressor::GetKTX2Data(compressedTextureHandle);

			if (compressedTextureHandle && TextureCompressor::IsCompressedSuccessfully(compressedTextureHandle))
			{
				const DataBuffer compressedImageData = TextureCompressor::GetImageData(compressedTextureHandle);
				const ImageFormat imageFormat = TextureCompressor::GetFormat(compressedTextureHandle);

				const uint32_t mipsCount = TextureCompressor::GetMipsCount(compressedTextureHandle);
				std::vector<DataBuffer> dataPerMip(mipsCount);
				for (uint32_t i = 0; i < mipsCount; ++i)
					dataPerMip[i] = TextureCompressor::GetMipData(compressedTextureHandle, i);

				texture = Texture2D::Create(pathToAsset.stem().u8string(), imageFormat, glm::uvec2(width, height), dataPerMip, specs);
			}
			else
			{
				EG_CORE_ERROR("Failed to load the compressed texture. Falling back to loading raw data: {}", pathToAsset.u8string());
				if (stbiImageData)
				{
					stbi_image_free(stbiImageData);
					stbiImageData = nullptr;
				}
				bCompressionFailed = true; // fall back to loading raw data
			}
		}

		// If non-compressed requested, or compression failed
		if (!bCompressedTexture || bCompressionFailed)
		{
			const int desiredChannels = AssetTextureFormatToChannels(assetFormat);
			stbiImageData = stbi_load_from_memory((uint8_t*)binary.Data(), (int)binary.Size(), &width, &height, &channels, desiredChannels);
			if (!stbiImageData)
			{
				EG_CORE_ERROR("Deserialization failed. stbi_load_from_memory failed: {}", pathToAsset.u8string());
				return {};
			}
			const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);
			texture = Texture2D::Create(pathToAsset.stem().u8string(), imageFormat, glm::uvec2(width, height), stbiImageData, specs);
		}

		class LocalAssetTexture2D : public AssetTexture2D
		{
		public:
			LocalAssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const DataBuffer& ktxData, const Ref<Texture2D>& texture,
				AssetTexture2DFormat format, bool bCompressed, bool bNormalMap, bool bNeedAlpha)
				: AssetTexture2D(path, pathToRaw, guid, rawData, ktxData, texture, format, bCompressed, bNormalMap, bNeedAlpha) {}
		};

		Ref<AssetTexture2D> asset = MakeRef<LocalAssetTexture2D>(pathToAsset, pathToRaw, guid,
			DataBuffer(binary.Data(), binary.Size()), ktxData, texture, assetFormat, bCompressedTexture, bNormalMap, bNeedAlpha);

		if (stbiImageData)
			stbi_image_free(stbiImageData);
		if (compressedTextureHandle)
			TextureCompressor::Destroy(compressedTextureHandle);

		return asset;
	}

	Ref<AssetTextureCube> Serializer::DeserializeAssetTextureCube(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::TextureCube))
			return {}; // Failed

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const AssetTextureCubeFormat assetFormat = Utils::GetEnumFromName<AssetTextureCubeFormat>(baseNode["Format"].as<std::string>());
		const uint32_t layerSize = baseNode["LayerSize"].as<uint32_t>();
		uint32_t prefilterSize = layerSize;
		if (auto prefilterNode = baseNode["PrefilterSize"])
			prefilterSize = prefilterNode.as<uint32_t>();

		ScopedDataBuffer binary;
		YAML::Binary yamlBinary;

		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				const size_t origSize = baseDataNode["Size"].as<size_t>();

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				binary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
			}
		}

		int width, height, channels;
		const int desiredChannels = AssetTextureFormatToChannels(assetFormat);
		void* stbiImageData = stbi_loadf_from_memory((uint8_t*)binary.Data(), (int)binary.Size(), &width, &height, &channels, desiredChannels);

		if (!stbiImageData)
		{
			EG_CORE_ERROR("Import failed. stbi_loadf_from_memory failed: {} - {}", pathToAsset.u8string(), Utils::GetEnumName(assetFormat));
			return {};
		}
		const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);
		const bool bFloat16 = IsFloat16Format(assetFormat);
		void* imageData = stbiImageData;
		if (bFloat16)
		{
			const size_t pixels = size_t(width) * height * desiredChannels;
			imageData = malloc(pixels * sizeof(uint16_t));
			uint16_t* imageData16 = (uint16_t*)imageData;
			float* stbiImageData32 = (float*)stbiImageData;

			for (size_t i = 0; i < pixels; ++i)
				imageData16[i] = Utils::ToFloat16(stbiImageData32[i]);
		}
		else if (assetFormat == AssetTextureCubeFormat::R11G11B10)
		{
			const size_t pixels = size_t(width) * height;
			imageData = malloc(pixels * sizeof(uint32_t));
			uint32_t* imageData32 = (uint32_t*)imageData;
			float* stbiImageData32 = (float*)stbiImageData;
			for (size_t i = 0; i < pixels; ++i)
			{
				glm::vec3 rgb = glm::vec3(stbiImageData32[i * 3], stbiImageData32[i * 3 + 1], stbiImageData32[i * 3 + 2]);
				imageData32[i] = Utils::ToR11G11B10(rgb);
			}
		}

		class LocalAssetTextureCube : public AssetTextureCube
		{
		public:
			LocalAssetTextureCube(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<TextureCube>& texture, AssetTextureCubeFormat format)
				: AssetTextureCube(path, pathToRaw, guid, rawData, texture, format) {}
		};

		Ref<AssetTextureCube> asset = MakeRef<LocalAssetTextureCube>(pathToAsset, pathToRaw, guid, binary.GetDataBuffer(),
			TextureCube::Create(pathToAsset.stem().u8string(), imageFormat, imageData, glm::uvec2(width, height), layerSize, prefilterSize), assetFormat);

		if (stbiImageData != imageData)
			free(imageData);
		stbi_image_free(stbiImageData);

		return asset;
	}

	Ref<AssetStaticMesh> Serializer::DeserializeAssetStaticMesh(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetMesh : public AssetStaticMesh
		{
		public:
			LocalAssetMesh(const Path& path, const Path& pathToRaw, GUID guid, const Ref<StaticMesh>& mesh)
				: AssetStaticMesh(path, pathToRaw, guid, mesh) {}
		};

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::StaticMesh))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();

		if (bReloadRaw)
		{
			auto importedMeshData = Utils::ImportStaticMesh(pathToRaw);
			if (!importedMeshData.Mesh)
			{
				EG_CORE_ERROR("Failed to reload a mesh asset: {}", pathToRaw.u8string());
				return {};
			}

			return MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, importedMeshData.Mesh);
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
			const size_t origVerticesSize = baseDataNode["SizeVertices"].as<size_t>();

			// Vertices
			{
				YAML::Binary yamlBinaryVertices = baseDataNode["Vertices"].as<YAML::Binary>();
				ScopedDataBuffer decompressedBinaryVertices{ Compressor::Decompress(DataBuffer{ (void*)yamlBinaryVertices.data(), yamlBinaryVertices.size() }, origVerticesSize) };

				const size_t verticesCount = decompressedBinaryVertices.Size() / sizeof(Vertex);
				vertices.resize(verticesCount);
				memcpy(vertices.data(), decompressedBinaryVertices.Data(), decompressedBinaryVertices.Size());
			}

			// Indices
			{
				auto indicesPerMaterialNode = baseDataNode["IndicesPerMaterial"];
				for (const auto& node : indicesPerMaterialNode)
				{
					const size_t origIndicesSize = node["SizeIndices"].as<size_t>();
					YAML::Binary yamlBinaryIndices = node["Indices"].as<YAML::Binary>();
					ScopedDataBuffer decompressedBinaryIndices{ Compressor::Decompress(DataBuffer{ (void*)yamlBinaryIndices.data(), yamlBinaryIndices.size() }, origIndicesSize) };

					const size_t indicesCount = decompressedBinaryIndices.Size() / sizeof(Index);
					auto& indices = indicesPerMaterial.emplace_back();
					indices.resize(indicesCount);
					memcpy(indices.data(), decompressedBinaryIndices.Data(), decompressedBinaryIndices.Size());
				}
			}
		}

		Ref<StaticMesh> staticMesh = StaticMesh::Create(vertices, indicesPerMaterial, aabb);
		if (materialsNode)
			for (const auto& matNode : materialsNode)
				staticMesh->SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));

		return MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, staticMesh);
	}
	
	Ref<AssetSkeletalMesh> Serializer::DeserializeAssetSkeletalMesh(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetMesh : public AssetSkeletalMesh
		{
		public:
			LocalAssetMesh(const Path& path, const Path& pathToRaw, GUID guid, const Ref<SkeletalMesh>& mesh)
				: AssetSkeletalMesh(path, pathToRaw, guid, mesh) {}
		};

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::SkeletalMesh))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();
		if (bReloadRaw)
		{
			auto importedMeshData = Utils::ImportSkeletalMesh(pathToRaw);
			if (!importedMeshData.Mesh)
			{
				EG_CORE_ERROR("Failed to reload a skeletal mesh asset: {}", pathToRaw.u8string());
				return {};
			}

			return MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, importedMeshData.Mesh);
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
		if (auto node = baseNode["MinRagdollBoneSize"])
			minRagdollBoneSize = node.as<float>();
		if (auto node = baseNode["MaxRagdollTwist"])
			maxRagdollTwist = node.as<float>();
		if (auto node = baseNode["MaxRagdollSwing"])
			maxRagdollSwing = node.as<float>();

		std::unordered_map<std::string, SkeletalRagdollBones::UserSettings> ragdollPerBoneData;
		if (auto node = baseNode["RagdollBonesData"])
		{
			ragdollPerBoneData.reserve(node.size());
			for (const auto& dataNode : node)
			{
				auto& data = ragdollPerBoneData[dataNode["Name"].as<std::string>()];
				data.UserOffset = Math::DecomposeTransformMatrix(dataNode["Offset"].as<glm::mat4>());
				data.LinearDamping = dataNode["LinearDamping"].as<float>();
				data.AngularDamping = dataNode["AngularDamping"].as<float>();
				if (auto simulateNode = dataNode["bEnableSimulation"])
					data.bEnableSimulation = simulateNode.as<bool>();
				if (auto collisionNode = dataNode["bEnableCollision"])
					data.bEnableCollision = collisionNode.as<bool>();
				data.Mass = dataNode["Mass"].as<float>();
				data.Material = GetAsset<AssetPhysicsMaterial>(dataNode["Material"]);
				data.Shape = Utils::GetEnumFromName<SkeletalRagdollBones::UserSettings::ShapeType>(dataNode["Shape"].as<std::string>());
			}
		}

		auto materialsNode = baseNode["Materials"];

		SkeletalMeshInfo skeletalInfo;
		skeletalInfo.InverseTransform = baseNode["InverseTransform"].as<glm::mat4>();
		skeletalInfo.CoordCorrection = baseNode["CoordCorrection"].as<glm::mat4>();
		ReadBoneNode(baseNode["Skeletal"], skeletalInfo.RootBone);

		// BoneInfoMap
		{
			auto boneNodes = baseNode["BoneInfoMap"];
			for (const auto& boneNode : boneNodes)
			{
				std::string name = boneNode["Name"].as<std::string>();
				BoneInfo info;
				info.Offset = boneNode["Matrix"].as<glm::mat4>();
				info.BoneID = boneNode["ID"].as<int>();
				skeletalInfo.BoneInfoMap.emplace(std::move(name), std::move(info));
			}
		}

		std::vector<SkeletalVertex> vertices;
		std::vector<std::vector<Index>> indicesPerMaterial;

		if (auto baseDataNode = baseNode["Data"])
		{
			const size_t origVerticesSize = baseDataNode["SizeVertices"].as<size_t>();

			// Vertices
			{
				YAML::Binary yamlBinaryVertices = baseDataNode["Vertices"].as<YAML::Binary>();
				ScopedDataBuffer decompressedBinaryVertices{ Compressor::Decompress(DataBuffer{ (void*)yamlBinaryVertices.data(), yamlBinaryVertices.size() }, origVerticesSize) };

				const size_t verticesCount = decompressedBinaryVertices.Size() / sizeof(SkeletalVertex);
				vertices.resize(verticesCount);
				memcpy(vertices.data(), decompressedBinaryVertices.Data(), decompressedBinaryVertices.Size());
			}

			// Indices
			{
				auto indicesPerMaterialNode = baseDataNode["IndicesPerMaterial"];
				for (const auto& node : indicesPerMaterialNode)
				{
					const size_t origIndicesSize = node["SizeIndices"].as<size_t>();
					YAML::Binary yamlBinaryIndices = node["Indices"].as<YAML::Binary>();
					ScopedDataBuffer decompressedBinaryIndices{ Compressor::Decompress(DataBuffer{ (void*)yamlBinaryIndices.data(), yamlBinaryIndices.size() }, origIndicesSize) };

					const size_t indicesCount = decompressedBinaryIndices.Size() / sizeof(Index);
					auto& indices = indicesPerMaterial.emplace_back();
					indices.resize(indicesCount);
					memcpy(indices.data(), decompressedBinaryIndices.Data(), decompressedBinaryIndices.Size());
				}
			}
		}

		Ref<SkeletalMesh> skeletalMesh = SkeletalMesh::Create(vertices, indicesPerMaterial, skeletalInfo, aabb, ragdollPerBoneData, minRagdollBoneSize, maxRagdollTwist, maxRagdollSwing);
		if (materialsNode)
			for (const auto& matNode : materialsNode)
				skeletalMesh->SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));

		return MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, skeletalMesh);
	}

	Ref<AssetAudio> Serializer::DeserializeAssetAudio(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Audio))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();

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
				EG_CORE_ERROR("Failed to reload a raw asset: {}", pathToRaw.u8string());
				return {};
			}
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				const size_t origSize = baseDataNode["Size"].as<size_t>();
				bool bCompressed = true;

				YAML::Binary yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					binary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
				}
			}
			if (!binary)
			{
				EG_CORE_ERROR("Failed to load the asset: {}", pathToAsset.u8string());
				return {};
			}
		}

		class LocalAssetAudio : public AssetAudio
		{
		public:
			LocalAssetAudio(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Audio>& audio, const Ref<AssetSoundGroup>& soundGroup)
				: AssetAudio(path, pathToRaw, guid, rawData, audio, soundGroup) {}
		};

		DataBuffer buffer = DataBuffer(binary.Data(), binary.Size());
		Ref<Audio> audio = Audio::Create(buffer);
		audio->SetVolume(volume);
		audio->SetPitch(pitch);
		audio->SetPan(pan);
		return MakeRef<LocalAssetAudio>(pathToAsset, pathToRaw, guid, buffer, audio, soundGroup);
	}

	Ref<AssetFont> Serializer::DeserializeAssetFont(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Font))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();

		ScopedDataBuffer binary;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			if (!binary)
			{
				EG_CORE_ERROR("Failed to reload a raw asset: {}", pathToRaw.u8string());
				return {};
			}
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				const size_t origSize = baseDataNode["Size"].as<size_t>();
				YAML::Binary yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				binary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
			}
			if (!binary)
			{
				EG_CORE_ERROR("Failed to load the asset: {}", pathToAsset.u8string());
				return {};
			}
		}

		class LocalAssetFont: public AssetFont
		{
		public:
			LocalAssetFont(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Font>& font)
				: AssetFont(path, pathToRaw, guid, rawData, font) {}
		};

		return MakeRef<LocalAssetFont>(pathToAsset, pathToRaw, guid, binary.GetDataBuffer(), Font::Create(binary.GetDataBuffer(), pathToAsset.stem().u8string()));
	}

	Ref<AssetMaterial> Serializer::DeserializeAssetMaterial(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Material))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

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

	Ref<AssetPhysicsMaterial> Serializer::DeserializeAssetPhysicsMaterial(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::PhysicsMaterial))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

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
			LocalAssetPhysicsMaterial(const Path& path, GUID guid, const Ref<PhysicsMaterial>& material)
				: AssetPhysicsMaterial(path, guid, material) {}
		};

		return MakeRef<LocalAssetPhysicsMaterial>(pathToAsset, guid, PhysicsMaterial::Create(staticFriction, dynamicFriction, bounciness));
	}

	Ref<AssetSoundGroup> Serializer::DeserializeAssetSoundGroup(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::SoundGroup))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
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
			LocalAssetSoundGroup(const Path& path, GUID guid, const Ref<SoundGroup>& soundGroup)
				: AssetSoundGroup(path, guid, soundGroup) {}
		};

		return MakeRef<LocalAssetSoundGroup>(pathToAsset, guid, soundGroup);
	}

	Ref<AssetEntity> Serializer::DeserializeAssetEntity(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Entity))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		Entity entity = AssetEntity::CreateEntity(guid);
		Serializer::DeserializeEntity(entity, baseNode);

		class LocalAssetEntity: public AssetEntity
		{
		public:
			LocalAssetEntity(const Path& path, GUID guid, const Ref<Entity>& entity)
				: AssetEntity(path, guid, entity) {}
		};

		return MakeRef<LocalAssetEntity>(pathToAsset, guid, MakeRef<Entity>(entity));
	}

	Ref<AssetAnimation> Serializer::DeserializeAssetAnimation(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetAnimation : public AssetAnimation
		{
		public:
			LocalAssetAnimation(const Path& path, const Path& pathToRaw, GUID guid, const Ref<SkeletalMeshAnimation>& anim, const Ref<AssetSkeletalMesh>& skeletal, uint32_t index)
				: AssetAnimation(path, pathToRaw, guid, anim, skeletal, index) {}
		};

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Animation))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			const std::string errorMessage = "Failed to reload an asset. Raw file doesn't exist: " + pathToRaw.u8string();
			EG_CORE_ERROR("{}", errorMessage);
			Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
			return {};
		}

		const uint32_t animIndex = baseNode["Index"].as<uint32_t>();
		Ref<AssetSkeletalMesh> skeletal = GetAsset<AssetSkeletalMesh>(baseNode["Skeletal"]);

		const GUID guid = baseNode["GUID"].as<GUID>();
		if (bReloadRaw)
		{
			const bool bImportRootMotion = HasRootMotion(baseNode);
			std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), bImportRootMotion);
			if (animations.size() < animIndex)
			{
				const std::string errorMessage = "Failed to reload an animation asset. The asset was initially imported at index " + 
					std::to_string(animIndex) + ", but now the file doesn't contains an animation at that index: " + pathToRaw.u8string();
				EG_CORE_ERROR("{}", errorMessage);
				Application::Get().GetImGuiLayer()->AddMessage(errorMessage);
				return {};
			}

			return MakeRef<LocalAssetAnimation>(pathToAsset, pathToRaw, guid, MakeRef<SkeletalMeshAnimation>(std::move(animations[animIndex])), skeletal, animIndex);
		}

		SkeletalMeshAnimation animation;
		Serializer::DeserializeAnimation(baseNode, animation);

		return MakeRef<LocalAssetAnimation>(pathToAsset, pathToRaw, guid, MakeRef<SkeletalMeshAnimation>(std::move(animation)), skeletal, animIndex);
	}

	Ref<AssetAnimationGraph> Serializer::DeserializeAssetAnimationGraph(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::AnimationGraph))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

		auto mesh = GetAsset<AssetSkeletalMesh>(baseNode["SkeletalMesh"]);
		if (!mesh)
		{
			EG_CORE_ERROR("Failed to deserialize animation graph at {}. Skeletal mesh wasn't found", pathToAsset.u8string());
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
			LocalAssetAnimationGraph(const Path& path, GUID guid, const Ref<AnimationGraph>& graph, const GraphEditorSerializationData& data)
				: AssetAnimationGraph(path, guid, graph, data) {}
		};

		auto result = MakeRef<LocalAssetAnimationGraph>(pathToAsset, guid, graph, graphEditorData);
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

	Ref<AssetParticleSystem> Serializer::DeserializeAssetParticleSystem(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::ParticleSystem))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

		std::vector<ParticleEmitter> emitters;
		auto emittersNode = baseNode["Emitters"];
		for (const auto& node : emittersNode)
		{
			auto& emitter = emitters.emplace_back();

			if (auto n = node["Name"])
				emitter.Name = n.as<std::string>();
			emitter.Texture = GetAsset<AssetTexture2D>(node["Texture"]);
			emitter.ColorStart = node["ColorStart"].as<glm::vec4>();
			emitter.ColorEnd = node["ColorEnd"].as<glm::vec4>();

			if (auto n = node["VelocityMin"])
				emitter.VelocityMin = n.as<glm::vec3>();
			if (auto n = node["VelocityMax"])
				emitter.VelocityMax = n.as<glm::vec3>();

			if (auto n = node["VelocityCoefStart"])
				emitter.VelocityCoefStart = n.as<glm::vec3>();
			if (auto n = node["VelocityCoefEnd"])
				emitter.VelocityCoefEnd = n.as<glm::vec3>();

			if (auto n = node["RotationZStart"])
				emitter.RotationZStart = n.as<float>();
			if (auto n = node["RotationZEnd"])
				emitter.RotationZEnd = n.as<float>();

			emitter.SizeStart = node["SizeStart"].as<glm::vec2>();
			emitter.SizeEnd = node["SizeEnd"].as<glm::vec2>();
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
			emitter.NumParticles = node["NumParticles"].as<uint32_t>();
			emitter.NumParticlesRatio = node["NumParticlesRatio"].as<float>();
			emitter.FastForwardTo = node["FastForwardTo"].as<float>();
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
				emitter.MeshAsset = GetAsset<AssetStaticMesh>(n);
			emitter.CollisionMode = Utils::GetEnumFromName<ParticleEmitter::CollisionModeType>(node["CollisionMode"].as<std::string>());
			if (auto n = node["bDestroyImmediately"])
				emitter.bDestroyImmediately = n.as<bool>();
			emitter.bEmit = node["bEmit"].as<bool>();
			emitter.bExplode = node["bExplode"].as<bool>();
			emitter.bApplyGravity = node["bApplyGravity"].as<bool>();
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
			LocalAssetParticleSystem(const Path& path, GUID guid, const std::vector<ParticleEmitter>& emitters)
				: AssetParticleSystem(path, guid, emitters) {}
		};

		return MakeRef<LocalAssetParticleSystem>(pathToAsset, guid, emitters);
	}

	Ref<AssetAnimationBlendSpace> Serializer::DeserializeAssetAnimationBlendSpace(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::AnimationBlendSpace))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

		auto mesh = GetAsset<AssetSkeletalMesh>(baseNode["SkeletalMesh"]);
		if (!mesh)
		{
			EG_CORE_ERROR("Failed to deserialize animation blend space at {}. Skeletal mesh wasn't found", pathToAsset.u8string());
			return {};
		}

		BlendSpaceAxisSettings horAxis;
		{
			auto horNode = baseNode["HorizontalAxis"];
			horAxis.Name = horNode["Name"].as<std::string>();
			horAxis.Min = horNode["Min"].as<float>();
			horAxis.Max = horNode["Max"].as<float>();
		}

		BlendSpaceAxisSettings verAxis;
		{
			auto verNode = baseNode["VerticalAxis"];
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
				point.Vertex.Coord = node["Coord"].as<glm::dvec2>();
			}
		}

		class LocalAssetAnimationBlendSpace : public AssetAnimationBlendSpace
		{
		public:
			LocalAssetAnimationBlendSpace(const Path& path, GUID guid, const Ref<AssetSkeletalMesh>& skeletal, const BlendSpaceAxisSettings& horAxis, const BlendSpaceAxisSettings& verAxis, const std::vector<BlendSpaceVertex>& pointsData)
				: AssetAnimationBlendSpace(path, guid, skeletal, horAxis, verAxis, pointsData) {
			}
		};

		auto result = MakeRef<LocalAssetAnimationBlendSpace>(pathToAsset, guid, mesh, horAxis, verAxis, points);

		return result;
	}

	AssetType Serializer::GetAssetType(const Path& pathToAsset)
	{
		YAML::Node baseNode = YAML::LoadFile(pathToAsset.string());

		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to get an asset type: {}", pathToAsset.u8string());
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
			case FieldType::Entity:
			case FieldType::Asset:
			case FieldType::AssetTexture2D:
			case FieldType::AssetTextureCube:
			case FieldType::AssetStaticMesh:
			case FieldType::AssetSkeletalMesh:
			case FieldType::AssetAudio:
			case FieldType::AssetSoundGroup:
			case FieldType::AssetFont:
			case FieldType::AssetMaterial:
			case FieldType::AssetPhysicsMaterial:
			case FieldType::AssetEntity:
			case FieldType::AssetScene:
			case FieldType::AssetAnimation:
			case FieldType::AssetAnimationGraph:
			case FieldType::AssetParticleSystem:
			case FieldType::AssetAnimationBlendSpace:
				SerializeField<GUID>(out, field);
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
					case FieldType::Entity:
					case FieldType::Asset:
					case FieldType::AssetTexture2D:
					case FieldType::AssetTextureCube:
					case FieldType::AssetStaticMesh:
					case FieldType::AssetSkeletalMesh:
					case FieldType::AssetAudio:
					case FieldType::AssetSoundGroup:
					case FieldType::AssetFont:
					case FieldType::AssetMaterial:
					case FieldType::AssetPhysicsMaterial:
					case FieldType::AssetEntity:
					case FieldType::AssetScene:
					case FieldType::AssetAnimation:
					case FieldType::AssetAnimationGraph:
					case FieldType::AssetParticleSystem:
					case FieldType::AssetAnimationBlendSpace:
						SetStoredValue<GUID>(node, field);
						break;
				}
			}
		}
	}

	bool Serializer::HasSerializableType(const PublicField& field)
	{
		switch (field.Type)
		{
			case FieldType::Int:
			case FieldType::UnsignedInt:
			case FieldType::Float:
			case FieldType::String:
			case FieldType::Vec2:
			case FieldType::Vec3:
			case FieldType::Vec4:
			case FieldType::Bool:
			case FieldType::Color3:
			case FieldType::Color4:
			case FieldType::Enum:
			case FieldType::Entity:
			case FieldType::Asset:
			case FieldType::AssetTexture2D:
			case FieldType::AssetTextureCube:
			case FieldType::AssetStaticMesh:
			case FieldType::AssetSkeletalMesh:
			case FieldType::AssetAudio:
			case FieldType::AssetSoundGroup:
			case FieldType::AssetFont:
			case FieldType::AssetMaterial:
			case FieldType::AssetPhysicsMaterial:
			case FieldType::AssetEntity:
			case FieldType::AssetScene:
			case FieldType::AssetAnimation:
			case FieldType::AssetAnimationGraph:
			case FieldType::AssetParticleSystem:
			case FieldType::AssetAnimationBlendSpace:
				return true;
			default: return false;
		}
	}

}
