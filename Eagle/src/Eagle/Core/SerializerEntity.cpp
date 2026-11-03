#include "egpch.h"
#include "Serializer.h"
#include "SerializerInternal.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Utils/SerializerUtils.h"

namespace Eagle
{
	namespace
	{
		void SerializeEntityAssetComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			out << YAML::Key << key << YAML::Value << entity.GetComponent<EntityAssetComponent>().AssetGUID;
		}

		void DeserializeEntityAssetComponent(Entity deserializedEntity, YAML::Node assetNode)
		{
			if (!deserializedEntity.HasComponent<EntityAssetComponent>())
				deserializedEntity.AddComponent<EntityAssetComponent>();

			deserializedEntity.GetComponent<EntityAssetComponent>().AssetGUID = assetNode.as<GUID>();
		}

		void SerializeTagComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			out << YAML::Key << key;
			out << YAML::BeginMap; //TagComponent

			out << YAML::Key << "Tag" << YAML::Value << entity.GetComponent<TagComponent>().Tag;

			out << YAML::EndMap; //TagComponent
		}

		void DeserializeTagComponent(Entity deserializedEntity, YAML::Node tagComponentNode)
		{
			deserializedEntity.GetComponent<TagComponent>().Tag = tagComponentNode["Tag"].as<std::string>();
		}

		void SerializeCameraComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = entity.GetComponent<CameraComponent>().Camera;

			out << YAML::Key << key;
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
			out << YAML::Key << "DirLightShadowFarClip" << YAML::Value << camera.GetDirLightShadowFarClip();
			out << YAML::Key << "CascadesSmoothTransitionAlpha" << YAML::Value << camera.GetCascadesSmoothTransitionAlpha();
			out << YAML::Key << "CascadesSplitAlpha" << YAML::Value << camera.GetCascadesSplitAlpha();

			out << YAML::EndMap; //Camera

			Serializer::SerializeRelativeTransform(out, cameraComponent.GetRelativeTransform());

			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; //CameraComponent;
		}

		void DeserializeCameraComponent(Entity deserializedEntity, YAML::Node cameraComponentNode)
		{
			auto& cameraComponent = deserializedEntity.AddComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;
			Transform relativeTransform;

			auto cameraNode = cameraComponentNode["Camera"];
			camera.SetProjectionMode(Utils::GetEnumFromName<CameraProjectionMode>(cameraNode["ProjectionMode"].as<std::string>()));

			camera.SetPerspectiveVerticalFOV(cameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(cameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(cameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(cameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(cameraNode["OrthographicFarClip"].as<float>());
			if (auto node = cameraNode["ShadowFarClip"])
				camera.SetShadowFarClip(node.as<float>());
			if (auto node = cameraNode["DirLightShadowFarClip"])
				camera.SetDirLightShadowFarClip(node.as<float>());
			if (auto node = cameraNode["CascadesSplitAlpha"])
				camera.SetCascadesSplitAlpha(node.as<float>());
			if (auto node = cameraNode["CascadesSmoothTransitionAlpha"])
				camera.SetCascadesSmoothTransitionAlpha(node.as<float>());

			Serializer::DeserializeRelativeTransform(cameraComponentNode, relativeTransform);

			cameraComponent.SetRelativeTransform(relativeTransform);

			cameraComponent.Primary = cameraComponentNode["Primary"].as<bool>();
			cameraComponent.FixedAspectRatio = cameraComponentNode["FixedAspectRatio"].as<bool>();
		}

		void SerializeSpriteComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			const auto& spriteComponent = entity.GetComponent<SpriteComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //SpriteComponent

			Serializer::SerializeRelativeTransform(out, spriteComponent.GetRelativeTransform());

			out << YAML::Key << "bAtlas" << YAML::Value << spriteComponent.IsAtlas();
			out << YAML::Key << "bCastsShadows" << YAML::Value << spriteComponent.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << spriteComponent.DoesReceiveDecals();
			out << YAML::Key << "IsVisible" << YAML::Value << spriteComponent.IsVisible();
			out << YAML::Key << "AtlasSpriteCoords" << YAML::Value << spriteComponent.GetAtlasSpriteCoords();
			out << YAML::Key << "AtlasSpriteSize" << YAML::Value << spriteComponent.GetAtlasSpriteSize();
			out << YAML::Key << "AtlasSpriteSizeCoef" << YAML::Value << spriteComponent.GetAtlasSpriteSizeCoef();
			if (const auto& materialAsset = spriteComponent.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << materialAsset->GetGUID();

			out << YAML::EndMap; //SpriteComponent
		}

		void DeserializeSpriteComponent(Entity deserializedEntity, YAML::Node spriteComponentNode)
		{
			auto& spriteComponent = deserializedEntity.AddComponent<SpriteComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(spriteComponentNode, relativeTransform);
			spriteComponent.SetRelativeTransform(relativeTransform);

			if (auto materialNode = spriteComponentNode["Material"])
				spriteComponent.SetMaterialAsset(GetAsset<AssetMaterial>(materialNode));

			if (auto node = spriteComponentNode["bAtlas"])
				spriteComponent.SetIsAtlas(node.as<bool>());
			if (auto node = spriteComponentNode["bCastsShadows"])
				spriteComponent.SetCastsShadows(node.as<bool>());
			if (auto node = spriteComponentNode["bReceivesDecals"])
				spriteComponent.SetReceivesDecals(node.as<bool>());
			if (auto node = spriteComponentNode["IsVisible"])
				spriteComponent.SetVisible(node.as<bool>());
			if (auto node = spriteComponentNode["AtlasSpriteCoords"])
				spriteComponent.SetAtlasSpriteCoords(node.as<glm::vec2>());
			if (auto node = spriteComponentNode["AtlasSpriteSize"])
				spriteComponent.SetAtlasSpriteSize(node.as<glm::vec2>());
			if (auto node = spriteComponentNode["AtlasSpriteSizeCoef"])
				spriteComponent.SetAtlasSpriteSizeCoef(node.as<glm::vec2>());
		}

		void SerializeBillboardComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& component = entity.GetComponent<BillboardComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //BillboardComponent

			Serializer::SerializeRelativeTransform(out, component.GetRelativeTransform());
			if (component.TextureAsset)
				out << YAML::Key << "Texture" << YAML::Value << component.TextureAsset->GetGUID();
			out << YAML::Key << "IsVisible" << YAML::Value << component.bVisible;

			out << YAML::EndMap; //BillboardComponent
		}

		void DeserializeBillboardComponent(Entity deserializedEntity, YAML::Node billboardComponentNode)
		{
			auto& billboardComponent = deserializedEntity.AddComponent<BillboardComponent>();
			Transform relativeTransform;

			Serializer::DeserializeRelativeTransform(billboardComponentNode, relativeTransform);
			billboardComponent.TextureAsset = GetAsset<AssetTexture2D>(billboardComponentNode["Texture"]);
			if (auto node = billboardComponentNode["IsVisible"])
				billboardComponent.bVisible = node.as<bool>();

			billboardComponent.SetRelativeTransform(relativeTransform);
		}

		void SerializeStaticMeshComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& smComponent = entity.GetComponent<StaticMeshComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //StaticMeshComponent

			out << YAML::Key << "bCastsShadows" << YAML::Value << smComponent.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << smComponent.DoesReceiveDecals();
			out << YAML::Key << "IsVisible" << YAML::Value << smComponent.IsVisible();

			Serializer::SerializeRelativeTransform(out, smComponent.GetRelativeTransform());

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

		void DeserializeStaticMeshComponent(Entity deserializedEntity, YAML::Node staticMeshComponentNode)
		{
			auto& smComponent = deserializedEntity.AddComponent<StaticMeshComponent>();
			smComponent.SetCastsShadows(staticMeshComponentNode["bCastsShadows"].as<bool>());
			if (auto node = staticMeshComponentNode["bReceivesDecals"])
				smComponent.SetReceivesDecals(node.as<bool>());
			if (auto node = staticMeshComponentNode["IsVisible"])
				smComponent.SetVisible(node.as<bool>());

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto meshNode = staticMeshComponentNode["Mesh"])
				smComponent.SetMeshAsset(GetAsset<AssetStaticMesh>(meshNode));
			if (auto materialsNode = staticMeshComponentNode["Materials"])
				for (const auto& matNode : materialsNode)
					smComponent.SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));
		}

		void SerializeSkeletalMeshComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& smComponent = entity.GetComponent<SkeletalMeshComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //SkeletalMeshComponent

			out << YAML::Key << "bCastsShadows" << YAML::Value << smComponent.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << smComponent.DoesReceiveDecals();
			out << YAML::Key << "IsVisible" << YAML::Value << smComponent.IsVisible();

			Serializer::SerializeRelativeTransform(out, smComponent.GetRelativeTransform());

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

		void DeserializeSkeletalMeshComponent(Entity deserializedEntity, YAML::Node skeletalMeshComponentNode)
		{
			auto& smComponent = deserializedEntity.AddComponent<SkeletalMeshComponent>();
			smComponent.SetCastsShadows(skeletalMeshComponentNode["bCastsShadows"].as<bool>());
			if (auto node = skeletalMeshComponentNode["bReceivesDecals"])
				smComponent.SetReceivesDecals(node.as<bool>());
			if (auto node = skeletalMeshComponentNode["IsVisible"])
				smComponent.SetVisible(node.as<bool>());

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(skeletalMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto meshNode = skeletalMeshComponentNode["Mesh"])
				smComponent.SetMeshAsset(GetAsset<AssetSkeletalMesh>(meshNode));
			if (auto materialsNode = skeletalMeshComponentNode["Materials"])
				for (const auto& matNode : materialsNode)
					smComponent.SetMaterialAsset(matNode["Index"].as<uint32_t>(), GetAsset<AssetMaterial>(matNode["Material"]));
			if (auto animationNode = skeletalMeshComponentNode["AnimationType"])
				smComponent.AnimType = Utils::GetEnumFromName<AnimationType>(animationNode.as<std::string>());
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

		void SerializePointLightComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& pointLightComponent = entity.GetComponent<PointLightComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //PointLightComponent

			Serializer::SerializeRelativeTransform(out, pointLightComponent.GetRelativeTransform());

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

		void DeserializePointLightComponent(Entity deserializedEntity, YAML::Node pointLightComponentNode)
		{
			auto& pointLightComponent = deserializedEntity.AddComponent<PointLightComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(pointLightComponentNode, relativeTransform);
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

		void SerializeDirectionalLightComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //DirectionalLightComponent

			Serializer::SerializeRelativeTransform(out, directionalLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << directionalLightComponent.GetLightColor();
			out << YAML::Key << "Ambient" << YAML::Value << directionalLightComponent.GetAmbientColor();
			out << YAML::Key << "Intensity" << YAML::Value << directionalLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << directionalLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "AffectsWorld" << YAML::Value << directionalLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << directionalLightComponent.DoesCastShadows();
			out << YAML::Key << "CastsScreenSpaceShadows" << YAML::Value << directionalLightComponent.DoesCastScreenSpaceShadows();
			out << YAML::Key << "IsVolumetric" << YAML::Value << directionalLightComponent.IsVolumetricLight();
			out << YAML::Key << "Visualize" << YAML::Value << directionalLightComponent.IsVisualizeDirectionEnabled();

			out << YAML::EndMap; //DirectionalLightComponent
		}

		void DeserializeDirectionalLightComponent(Entity deserializedEntity, YAML::Node directionalLightComponentNode)
		{
			auto& directionalLightComponent = deserializedEntity.AddComponent<DirectionalLightComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(directionalLightComponentNode, relativeTransform);
			directionalLightComponent.SetRelativeTransform(relativeTransform);

			if (auto lightColorNode = directionalLightComponentNode["LightColor"])
				directionalLightComponent.SetLightColor(lightColorNode.as<glm::vec3>());
			if (auto ambientNode = directionalLightComponentNode["Ambient"])
				directionalLightComponent.SetAmbientColor(ambientNode.as<glm::vec3>());
			if (auto intensityNode = directionalLightComponentNode["Intensity"])
				directionalLightComponent.SetIntensity(intensityNode.as<float>());
			if (auto intensityNode = directionalLightComponentNode["VolumetricFogIntensity"])
				directionalLightComponent.SetVolumetricFogIntensity(intensityNode.as<float>());
			if (auto node = directionalLightComponentNode["AffectsWorld"])
				directionalLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = directionalLightComponentNode["CastsShadows"])
				directionalLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = directionalLightComponentNode["CastsScreenSpaceShadows"])
				directionalLightComponent.SetCastsScreenSpaceShadows(node.as<bool>());
			if (auto node = directionalLightComponentNode["IsVolumetric"])
				directionalLightComponent.SetIsVolumetricLight(node.as<bool>());
			if (auto node = directionalLightComponentNode["Visualize"])
				directionalLightComponent.SetVisualizeDirectionEnabled(node.as<bool>());
		}

		void SerializeSpotLightComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& spotLightComponent = entity.GetComponent<SpotLightComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //SpotLightComponent

			Serializer::SerializeRelativeTransform(out, spotLightComponent.GetRelativeTransform());

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

		void DeserializeSpotLightComponent(Entity deserializedEntity, YAML::Node spotLightComponentNode)
		{
			auto& spotLightComponent = deserializedEntity.AddComponent<SpotLightComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(spotLightComponentNode, relativeTransform);
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

		void SerializeScriptComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& scriptComponent = entity.GetComponent<ScriptComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap;

			out << YAML::Key << "ModuleName" << YAML::Value << scriptComponent.ModuleName;

			SerializeScriptFields(out, scriptComponent.PublicFields);

			out << YAML::EndMap;
		}

		void DeserializeScriptComponent(Entity deserializedEntity, YAML::Node scriptComponentNode)
		{
			auto& scriptComponent = deserializedEntity.AddComponent<ScriptComponent>();

			scriptComponent.ModuleName = scriptComponentNode["ModuleName"].as<std::string>();
			ScriptEngine::UpdateEntityPublicFields(deserializedEntity, false);

			auto publicFieldsNode = scriptComponentNode["PublicFields"];
			if (publicFieldsNode)
				Serializer::DeserializePublicFieldValues(publicFieldsNode, scriptComponent.PublicFields);
		}

		void SerializeRigidBodyComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //RigidBodyComponent

			out << YAML::Key << "BodyType" << YAML::Value << Utils::GetEnumName(rigidBodyComponent.GetBodyType());
			out << YAML::Key << "CollisionDetectionType" << YAML::Value << Utils::GetEnumName(rigidBodyComponent.GetCollisionDetectionType());
			out << YAML::Key << "PositionSolverIterations" << YAML::Value << rigidBodyComponent.GetPositionSolverIterations();
			out << YAML::Key << "VelocitySolverIterations" << YAML::Value << rigidBodyComponent.GetVelocitySolverIterations();
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

		void DeserializeRigidBodyComponent(Entity deserializedEntity, YAML::Node rigidBodyComponentNode)
		{
			PhysicsBodyType bodyType = Utils::GetEnumFromName<PhysicsBodyType>(rigidBodyComponentNode["BodyType"].as<std::string>());
			auto& rigidBodyComponent = deserializedEntity.AddComponent<RigidBodyComponent>(bodyType);


			rigidBodyComponent.SetCollisionDetectionType(Utils::GetEnumFromName<CollisionDetectionType>(rigidBodyComponentNode["CollisionDetectionType"].as<std::string>()));
			if (auto node = rigidBodyComponentNode["PositionSolverIterations"])
				rigidBodyComponent.SetPositionSolverIterations(node.as<uint32_t>());
			if (auto node = rigidBodyComponentNode["VelocitySolverIterations"])
				rigidBodyComponent.SetVelocitySolverIterations(node.as<uint32_t>());
			rigidBodyComponent.SetMass(rigidBodyComponentNode["Mass"].as<float>());
			rigidBodyComponent.SetLinearDamping(rigidBodyComponentNode["LinearDamping"].as<float>());
			rigidBodyComponent.SetAngularDamping(rigidBodyComponentNode["AngularDamping"].as<float>());
			rigidBodyComponent.SetMaxLinearVelocity(rigidBodyComponentNode["MaxLinearVelocity"].as<float>());
			rigidBodyComponent.SetMaxAngularVelocity(rigidBodyComponentNode["MaxAngularVelocity"].as<float>());
			rigidBodyComponent.SetEnableGravity(rigidBodyComponentNode["EnableGravity"].as<bool>());
			rigidBodyComponent.SetIsKinematic(rigidBodyComponentNode["IsKinematic"].as<bool>());
			rigidBodyComponent.SetLockFlag(ActorLockFlag(rigidBodyComponentNode["LockFlags"].as<uint32_t>()));
		}

		void SerializeBoxColliderComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& collider = entity.GetComponent<BoxColliderComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //BoxColliderComponent

			Serializer::SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsCollisionEnabled" << YAML::Value << collider.IsCollisionEnabled();
			out << YAML::Key << "Size" << YAML::Value << collider.GetSize();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::Key << "CollisionGroupMask" << YAML::Value << uint32_t(collider.GetCollisionGroup());
			out << YAML::Key << "InteractingCollisionGroupMask" << YAML::Value << uint32_t(collider.GetInteractingCollisionGroup());
			out << YAML::EndMap; //BoxColliderComponent
		}

		void DeserializeBoxColliderComponent(Entity deserializedEntity, YAML::Node boxColliderNode, uint32_t collisionGroupValidMasks)
		{
			auto& collider = deserializedEntity.AddComponent<BoxColliderComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(boxColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(boxColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(boxColliderNode["IsTrigger"].as<bool>());
			if (auto node = boxColliderNode["IsCollisionEnabled"])
				collider.SetCollisionEnabled(node.as<bool>());
			collider.SetSize(boxColliderNode["Size"].as<glm::vec3>());
			collider.SetShowCollision(boxColliderNode["IsCollisionVisible"].as<bool>());
			if (auto node = boxColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = boxColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
			if (auto node = boxColliderNode["CollisionGroupMask"])
				collider.SetCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
			if (auto node = boxColliderNode["InteractingCollisionGroupMask"])
				collider.SetInteractingCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
		}

		void SerializeCharacterControllerComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& component = entity.GetComponent<CharacterControllerComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //CharacterControllerComponent

			Serializer::SerializeRelativeTransform(out, component.GetRelativeTransform());

			out << YAML::Key << "SlopeLimit" << YAML::Value << component.GetSlopeLimit();
			out << YAML::Key << "ContactOffset" << YAML::Value << component.GetContactOffset();
			out << YAML::Key << "StepOffset" << YAML::Value << component.GetStepOffset();
			if (const auto& material = component.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << material->GetGUID();
			out << YAML::Key << "Shape" << YAML::Value << Utils::GetEnumName(component.GetShapeType());
			out << YAML::Key << "ClimbingMode" << YAML::Value << Utils::GetEnumName(component.GetCapsuleClimbingMode());
			out << YAML::Key << "CapsuleRadius" << YAML::Value << component.GetCapsuleRadius();
			out << YAML::Key << "CapsuleHeight" << YAML::Value << component.GetCapsuleHeight();
			out << YAML::Key << "BoxSize" << YAML::Value << component.GetBoxSize();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << component.IsCollisionVisible();
			out << YAML::Key << "CollisionGroupMask" << YAML::Value << uint32_t(component.GetCollisionGroup());
			out << YAML::Key << "InteractingCollisionGroupMask" << YAML::Value << uint32_t(component.GetInteractingCollisionGroup());
			out << YAML::Key << "UpDirection" << YAML::Value << component.GetControllerUpDirection();
			out << YAML::Key << "CollidesWithOtherControllers" << YAML::Value << component.DoesCollideWithOtherControllers();
			out << YAML::Key << "bMoveWholeEntity" << YAML::Value << component.bMoveWholeEntity;
			out << YAML::Key << "bUseFootLocation" << YAML::Value << component.bUseFootLocation;

			out << YAML::EndMap; //CharacterControllerComponent
		}

		void DeserializeCharacterControllerComponent(Entity deserializedEntity, YAML::Node ccNode, uint32_t collisionGroupValidMasks)
		{
			auto& component = deserializedEntity.AddComponent<CharacterControllerComponent>();

			component.SetSlopeLimit(ccNode["SlopeLimit"].as<float>());
			component.SetContactOffset(ccNode["ContactOffset"].as<float>());
			component.SetStepOffset(ccNode["StepOffset"].as<float>());
			component.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(ccNode["PhysicsMaterial"]));
			component.SetShapeType(Utils::GetEnumFromName<CharacterControllerShape>(ccNode["Shape"].as<std::string>()));
			component.SetCapsuleClimbingMode(Utils::GetEnumFromName<CapsuleClimbingMode>(ccNode["ClimbingMode"].as<std::string>()));
			component.SetCapsuleRadius(ccNode["CapsuleRadius"].as<float>());
			component.SetCapsuleHeight(ccNode["CapsuleHeight"].as<float>());
			component.SetBoxSize(ccNode["BoxSize"].as<glm::vec3>());
			component.SetShowCollision(ccNode["IsCollisionVisible"].as<bool>());
			component.SetDoesCollideWithOtherControllers(ccNode["CollidesWithOtherControllers"].as<bool>());
			if (auto node = ccNode["CollisionGroupMask"])
				component.SetCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
			if (auto node = ccNode["InteractingCollisionGroupMask"])
				component.SetInteractingCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
			if (auto node = ccNode["UpDirection"])
				component.SetControllerUpDirection(node.as<glm::vec3>());
			if (auto node = ccNode["bMoveWholeEntity"])
				component.bMoveWholeEntity = node.as<bool>();
			if (auto node = ccNode["bUseFootLocation"])
				component.bUseFootLocation = node.as<bool>();

			// Above function calls can adjust the transform. But since we want to match exactly,
			// we call it at the end of the deserialization
			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(ccNode, relativeTransform);
			component.SetRelativeTransform(relativeTransform);
		}

		void SerializeSphereColliderComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& collider = entity.GetComponent<SphereColliderComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //SphereColliderComponent

			Serializer::SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsCollisionEnabled" << YAML::Value << collider.IsCollisionEnabled();
			out << YAML::Key << "Radius" << YAML::Value << collider.GetRadius();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::Key << "CollisionGroupMask" << YAML::Value << uint32_t(collider.GetCollisionGroup());
			out << YAML::Key << "InteractingCollisionGroupMask" << YAML::Value << uint32_t(collider.GetInteractingCollisionGroup());
			out << YAML::EndMap; //SphereColliderComponent
		}

		void DeserializeSphereColliderComponent(Entity deserializedEntity, YAML::Node sphereColliderNode, uint32_t collisionGroupValidMasks)
		{
			auto& collider = deserializedEntity.AddComponent<SphereColliderComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(sphereColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(sphereColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(sphereColliderNode["IsTrigger"].as<bool>());
			if (auto node = sphereColliderNode["IsCollisionEnabled"])
				collider.SetCollisionEnabled(node.as<bool>());
			collider.SetRadius(sphereColliderNode["Radius"].as<float>());
			collider.SetShowCollision(sphereColliderNode["IsCollisionVisible"].as<bool>());
			if (auto node = sphereColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = sphereColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
			if (auto node = sphereColliderNode["CollisionGroupMask"])
				collider.SetCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
			if (auto node = sphereColliderNode["InteractingCollisionGroupMask"])
				collider.SetInteractingCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
		}

		void SerializeCapsuleColliderComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& collider = entity.GetComponent<CapsuleColliderComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //CapsuleColliderComponent

			Serializer::SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsCollisionEnabled" << YAML::Value << collider.IsCollisionEnabled();
			out << YAML::Key << "Radius" << YAML::Value << collider.GetRadius();
			out << YAML::Key << "Height" << YAML::Value << collider.GetHeight();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::Key << "CollisionGroupMask" << YAML::Value << uint32_t(collider.GetCollisionGroup());
			out << YAML::Key << "InteractingCollisionGroupMask" << YAML::Value << uint32_t(collider.GetInteractingCollisionGroup());
			out << YAML::EndMap; //CapsuleColliderComponent
		}

		void DeserializeCapsuleColliderComponent(Entity deserializedEntity, YAML::Node capsuleColliderNode, uint32_t collisionGroupValidMasks)
		{
			auto& collider = deserializedEntity.AddComponent<CapsuleColliderComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(capsuleColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(capsuleColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(capsuleColliderNode["IsTrigger"].as<bool>());
			if (auto node = capsuleColliderNode["IsCollisionEnabled"])
				collider.SetCollisionEnabled(node.as<bool>());
			collider.SetRadius(capsuleColliderNode["Radius"].as<float>());
			collider.SetHeight(capsuleColliderNode["Height"].as<float>());
			collider.SetShowCollision(capsuleColliderNode["IsCollisionVisible"].as<bool>());
			if (auto node = capsuleColliderNode["IsObstacle"])
				collider.SetIsObstacle(node.as<bool>());
			if (auto node = capsuleColliderNode["DoesAffectNavMeshBuild"])
				collider.SetAffectsNavMeshBuild(node.as<bool>());
			if (auto node = capsuleColliderNode["CollisionGroupMask"])
				collider.SetCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
			if (auto node = capsuleColliderNode["InteractingCollisionGroupMask"])
				collider.SetInteractingCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
		}

		void SerializeMeshColliderComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& collider = entity.GetComponent<MeshColliderComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //MeshColliderComponent

			Serializer::SerializeRelativeTransform(out, collider.GetRelativeTransform());

			if (const auto& meshAsset = collider.GetCollisionMeshAsset())
				out << YAML::Key << "Mesh" << YAML::Value << meshAsset->GetGUID();

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsCollisionEnabled" << YAML::Value << collider.IsCollisionEnabled();
			out << YAML::Key << "IsConvex" << YAML::Value << collider.IsConvex();
			out << YAML::Key << "IsTwoSided" << YAML::Value << collider.IsTwoSided();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::Key << "IsObstacle" << YAML::Value << collider.IsObstacle();
			out << YAML::Key << "DoesAffectNavMeshBuild" << YAML::Value << collider.DoesAffectNavMeshBuild();
			out << YAML::Key << "CollisionGroupMask" << YAML::Value << uint32_t(collider.GetCollisionGroup());
			out << YAML::Key << "InteractingCollisionGroupMask" << YAML::Value << uint32_t(collider.GetInteractingCollisionGroup());
			out << YAML::EndMap; //MeshColliderComponent
		}

		void DeserializeMeshColliderComponent(Entity deserializedEntity, YAML::Node meshColliderNode, uint32_t collisionGroupValidMasks)
		{
			auto& collider = deserializedEntity.AddComponent<MeshColliderComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(meshColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(meshColliderNode["PhysicsMaterial"]));
			collider.SetIsTrigger(meshColliderNode["IsTrigger"].as<bool>());
			if (auto node = meshColliderNode["IsCollisionEnabled"])
				collider.SetCollisionEnabled(node.as<bool>());
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
			if (auto node = meshColliderNode["CollisionGroupMask"])
				collider.SetCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
			if (auto node = meshColliderNode["InteractingCollisionGroupMask"])
				collider.SetInteractingCollisionGroup(CollisionGroup(node.as<uint32_t>() & collisionGroupValidMasks));
		}

		void SerializeAudioComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& audio = entity.GetComponent<AudioComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //AudioComponent

			Serializer::SerializeRelativeTransform(out, audio.GetRelativeTransform());
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

		void DeserializeAudioComponent(Entity deserializedEntity, YAML::Node audioNode)
		{
			auto& audio = deserializedEntity.AddComponent<AudioComponent>();
			Path soundPath;

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(audioNode, relativeTransform);
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

		void SerializeReverbComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& reverb = entity.GetComponent<ReverbComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //ReverbComponent

			Serializer::SerializeRelativeTransform(out, reverb.GetRelativeTransform());
			out << YAML::Key << "bVisualize" << YAML::Value << reverb.IsVisualizeRadiusEnabled();
			Serializer::SerializeReverb(out, reverb.GetReverb());

			out << YAML::EndMap; //ReverbComponent
		}

		void DeserializeReverbComponent(Entity deserializedEntity, YAML::Node reverbNode)
		{
			auto& reverb = deserializedEntity.AddComponent<ReverbComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(reverbNode, relativeTransform);
			reverb.SetRelativeTransform(relativeTransform);

			if (auto node = reverbNode["bVisualize"])
				reverb.SetVisualizeRadiusEnabled(node.as<bool>());

			if (auto node = reverbNode["Reverb"])
				Serializer::DeserializeReverb(node, reverb);
		}

		void SerializeTextComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& text = entity.GetComponent<TextComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //TextComponent

			Serializer::SerializeRelativeTransform(out, text.GetRelativeTransform());
			if (const auto& asset = text.GetFontAsset())
				out << YAML::Key << "Font" << YAML::Value << asset->GetGUID();
			if (const auto& asset = text.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Text" << YAML::Value << text.GetText();
			out << YAML::Key << "Color" << YAML::Value << text.GetColor();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();
			out << YAML::Key << "IsLit" << YAML::Value << text.IsLit();
			out << YAML::Key << "IsDoubleSided" << YAML::Value << text.IsDoubleSided();
			out << YAML::Key << "bCastsShadows" << YAML::Value << text.DoesCastShadows();
			out << YAML::Key << "bReceivesDecals" << YAML::Value << text.DoesReceiveDecals();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "LineSpacing" << YAML::Value << text.GetLineSpacing();
			out << YAML::Key << "LetterSpacing" << YAML::Value << text.GetLetterSpacing();
			out << YAML::Key << "MaxWidth" << YAML::Value << text.GetMaxWidth();
			out << YAML::Key << "Alignment" << YAML::Value << Utils::GetEnumName(text.GetAlignment());

			out << YAML::EndMap; //TextComponent
		}

		void DeserializeTextComponent(Entity deserializedEntity, YAML::Node textNode)
		{
			auto& text = deserializedEntity.AddComponent<TextComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(textNode, relativeTransform);
			text.SetRelativeTransform(relativeTransform);

			text.SetFontAsset(GetAsset<AssetFont>(textNode["Font"]));
			text.SetMaterialAsset(GetAsset<AssetMaterial>(textNode["Material"]));
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			if (auto node = textNode["Opacity"])
				text.SetOpacity(node.as<float>());
			text.SetIsLit(textNode["IsLit"].as<bool>());
			if (auto node = textNode["IsDoubleSided"])
				text.SetDoubleSided(node.as<bool>());
			if (auto node = textNode["bCastsShadows"])
				text.SetCastsShadows(node.as<bool>());
			if (auto node = textNode["bReceivesDecals"])
				text.SetReceivesDecals(node.as<bool>());
			if (auto node = textNode["IsVisible"])
				text.SetVisible(node.as<bool>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			if (auto node = textNode["LetterSpacing"])
				text.SetLetterSpacing(node.as<float>());
			else if (auto oldNode = textNode["Kerning"])
				text.SetLetterSpacing(oldNode.as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
			if (auto node = textNode["Alignment"])
				text.SetAlignment(Utils::GetEnumFromName<TextAlignment>(node.as<std::string>()));
		}

		void SerializeText2DComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& text = entity.GetComponent<Text2DComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //Text2DComponent

			if (const auto& asset = text.GetFontAsset())
				out << YAML::Key << "Font" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Text" << YAML::Value << text.GetText();
			out << YAML::Key << "Color" << YAML::Value << text.GetColor();
			out << YAML::Key << "Tint" << YAML::Value << text.GetTint();
			out << YAML::Key << "LineSpacing" << YAML::Value << text.GetLineSpacing();
			out << YAML::Key << "LetterSpacing" << YAML::Value << text.GetLetterSpacing();
			out << YAML::Key << "MaxWidth" << YAML::Value << text.GetMaxWidth();
			out << YAML::Key << "FontSize" << YAML::Value << text.GetFontSize();
			out << YAML::Key << "Alignment" << YAML::Value << Utils::GetEnumName(text.GetAlignment());
			out << YAML::Key << "Anchor" << YAML::Value << text.GetAnchor();
			out << YAML::Key << "Pivot" << YAML::Value << text.GetPivot();
			out << YAML::Key << "PivotFollowsAnchor" << YAML::Value << text.DoesPivotFollowAnchor();
			out << YAML::Key << "HitTestable" << YAML::Value << text.IsHitTestable();
			out << YAML::Key << "Offset" << YAML::Value << text.GetOffset();
			out << YAML::Key << "Rotation" << YAML::Value << text.GetRotation();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();

			const Text2DEffects& effects = text.GetEffects();
			out << YAML::Key << "Effects" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Outline" << YAML::Value << effects.bOutline;
			out << YAML::Key << "OutlineColor" << YAML::Value << effects.OutlineColor;
			out << YAML::Key << "OutlineWidth" << YAML::Value << effects.OutlineWidth;
			out << YAML::Key << "Glow" << YAML::Value << effects.bGlow;
			out << YAML::Key << "GlowColor" << YAML::Value << effects.GlowColor;
			out << YAML::Key << "GlowSize" << YAML::Value << effects.GlowSize;
			out << YAML::Key << "Shadow" << YAML::Value << effects.bShadow;
			out << YAML::Key << "ShadowColor" << YAML::Value << effects.ShadowColor;
			out << YAML::Key << "ShadowOffset" << YAML::Value << effects.ShadowOffset;
			out << YAML::Key << "ShadowSoftness" << YAML::Value << effects.ShadowSoftness;
			out << YAML::EndMap;

			out << YAML::EndMap; //Text2DComponent
		}

		void DeserializeText2DComponent(Entity deserializedEntity, YAML::Node textNode)
		{
			auto& text = deserializedEntity.AddComponent<Text2DComponent>();

			text.SetFontAsset(GetAsset<AssetFont>(textNode["Font"]));
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			if (auto node = textNode["Tint"])
				text.SetTint(node.as<glm::vec3>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			text.SetRotation(textNode["Rotation"].as<float>());
			text.SetIsVisible(textNode["IsVisible"].as<bool>());
			if (auto node = textNode["HitTestable"])
				text.SetIsHitTestable(node.as<bool>());
			if (auto node = textNode["LetterSpacing"])
				text.SetLetterSpacing(node.as<float>());
			else if (auto oldNode = textNode["Kerning"])
				text.SetLetterSpacing(oldNode.as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
			text.SetOpacity(textNode["Opacity"].as<float>());
			if (auto effectsNode = textNode["Effects"])
			{
				Text2DEffects effects;
				if (auto node = effectsNode["Outline"])
					effects.bOutline = node.as<bool>();
				if (auto node = effectsNode["OutlineColor"])
					effects.OutlineColor = node.as<glm::vec4>();
				if (auto node = effectsNode["OutlineWidth"])
					effects.OutlineWidth = node.as<float>();
				if (auto node = effectsNode["Glow"])
					effects.bGlow = node.as<bool>();
				if (auto node = effectsNode["GlowColor"])
					effects.GlowColor = node.as<glm::vec4>();
				if (auto node = effectsNode["GlowSize"])
					effects.GlowSize = node.as<float>();
				if (auto node = effectsNode["Shadow"])
					effects.bShadow = node.as<bool>();
				if (auto node = effectsNode["ShadowColor"])
					effects.ShadowColor = node.as<glm::vec4>();
				if (auto node = effectsNode["ShadowOffset"])
					effects.ShadowOffset = node.as<glm::vec2>();
				if (auto node = effectsNode["ShadowSoftness"])
					effects.ShadowSoftness = node.as<float>();
				text.SetEffects(effects);
			}
			if (auto anchorNode = textNode["Anchor"])
			{
				text.SetAnchor(anchorNode.as<glm::vec2>());
				text.SetPivot(textNode["Pivot"].as<glm::vec2>());
				text.SetOffset(textNode["Offset"].as<glm::vec2>());
				text.SetFontSize(textNode["FontSize"].as<float>());
				text.SetAlignment(Utils::GetEnumFromName<TextAlignment>(textNode["Alignment"].as<std::string>()));
				if (auto node = textNode["PivotFollowsAnchor"])
					text.SetPivotFollowsAnchor(node.as<bool>());
			}
		}

		void SerializeImage2DComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& text = entity.GetComponent<Image2DComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //Image2DComponent

			if (const auto& asset = text.GetTextureAsset())
				out << YAML::Key << "Texture" << YAML::Value << asset->GetGUID();
			out << YAML::Key << "Tint" << YAML::Value << text.GetTint();
			out << YAML::Key << "Anchor" << YAML::Value << text.GetAnchor();
			out << YAML::Key << "Pivot" << YAML::Value << text.GetPivot();
			out << YAML::Key << "PivotFollowsAnchor" << YAML::Value << text.DoesPivotFollowAnchor();
			out << YAML::Key << "HitTestable" << YAML::Value << text.IsHitTestable();
			out << YAML::Key << "Offset" << YAML::Value << text.GetOffset();
			out << YAML::Key << "Size" << YAML::Value << text.GetSize();
			out << YAML::Key << "Rotation" << YAML::Value << text.GetRotation();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();

			out << YAML::EndMap; //Image2DComponent
		}

		void DeserializeImage2DComponent(Entity deserializedEntity, YAML::Node imageNode)
		{
			auto& image2D = deserializedEntity.AddComponent<Image2DComponent>();

			Ref<AssetTexture2D> asset = GetAsset<AssetTexture2D>(imageNode["Texture"]);
			image2D.SetTextureAsset(asset);
			image2D.SetTint(imageNode["Tint"].as<glm::vec3>());
			image2D.SetRotation(imageNode["Rotation"].as<float>());
			if (auto anchorNode = imageNode["Anchor"])
			{
				image2D.SetAnchor(anchorNode.as<glm::vec2>());
				image2D.SetPivot(imageNode["Pivot"].as<glm::vec2>());
				image2D.SetOffset(imageNode["Offset"].as<glm::vec2>());
				image2D.SetSize(imageNode["Size"].as<glm::vec2>());
				if (auto node = imageNode["PivotFollowsAnchor"])
					image2D.SetPivotFollowsAnchor(node.as<bool>());
			}
			image2D.SetIsVisible(imageNode["IsVisible"].as<bool>());
			if (auto node = imageNode["HitTestable"])
				image2D.SetIsHitTestable(node.as<bool>());
			image2D.SetOpacity(imageNode["Opacity"].as<float>());
		}

		void SerializeParticleSystemComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& system = entity.GetComponent<ParticleSystemComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //ParticleSystemComponent

			Serializer::SerializeRelativeTransform(out, system.GetRelativeTransform());

			if (const auto& asset = system.GetAsset())
				out << YAML::Key << "ParticleSystem" << YAML::Value << asset->GetGUID();
			out << YAML::Key << "bAutospawn" << YAML::Value << system.bAutospawn;

			out << YAML::EndMap; //ParticleSystemComponent
		}

		void DeserializeParticleSystemComponent(Entity deserializedEntity, YAML::Node systemNode)
		{
			auto& system = deserializedEntity.AddComponent<ParticleSystemComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(systemNode, relativeTransform);
			system.SetRelativeTransform(relativeTransform);
			system.SetAsset(GetAsset<AssetParticleSystem>(systemNode["ParticleSystem"]));
			if (auto autospawnNode = systemNode["bAutospawn"])
				system.bAutospawn = autospawnNode.as<bool>();
		}

		void SerializeSceneSequenceComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& sequence = entity.GetComponent<SceneSequenceComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //SceneSequenceComponent

			Serializer::SerializeRelativeTransform(out, sequence.GetRelativeTransform());

			if (const auto& asset = sequence.GetAsset())
				out << YAML::Key << "SceneSequence" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "AutoPlay" << YAML::Value << sequence.bAutoPlay;
			out << YAML::Key << "OverrideLooping" << YAML::Value << sequence.DoesOverrideLooping();
			out << YAML::Key << "Looping" << YAML::Value << sequence.IsLoopingEnabled();
			out << YAML::Key << "PlayInWorldSpace" << YAML::Value << sequence.bPlayInWorldSpace;
			out << YAML::Key << "DriveCamera" << YAML::Value << sequence.AllowedToDriveCamera();
			out << YAML::Key << "DrawPathInEditor" << YAML::Value << sequence.bDrawPathInEditor;
			out << YAML::Key << "PlayRate" << YAML::Value << sequence.GetPlayRate();

			out << YAML::EndMap; //SceneSequenceComponent
		}

		void DeserializeSceneSequenceComponent(Entity deserializedEntity, YAML::Node sequenceNode)
		{
			auto& sequence = deserializedEntity.AddComponent<SceneSequenceComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(sequenceNode, relativeTransform);
			sequence.SetRelativeTransform(relativeTransform);

			sequence.SetAsset(GetAsset<AssetSceneSequence>(sequenceNode["SceneSequence"]));

			if (auto node = sequenceNode["AutoPlay"])
				sequence.bAutoPlay = node.as<bool>();
			if (auto node = sequenceNode["OverrideLooping"])
				sequence.SetOverrideLooping(node.as<bool>());
			if (auto node = sequenceNode["Looping"])
				sequence.SetLoopingEnabled(node.as<bool>());
			if (auto node = sequenceNode["PlayInWorldSpace"])
				sequence.bPlayInWorldSpace = node.as<bool>();
			if (auto node = sequenceNode["DriveCamera"])
				sequence.SetDriveCameraAllowed(node.as<bool>());
			if (auto node = sequenceNode["DrawPathInEditor"])
				sequence.bDrawPathInEditor = node.as<bool>();
			if (auto node = sequenceNode["PlayRate"])
				sequence.SetPlayRate(node.as<float>());
		}

		void SerializeDecalComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& decal = entity.GetComponent<DecalComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; //DecalComponent

			Serializer::SerializeRelativeTransform(out, decal.GetRelativeTransform());
			out << YAML::Key << "SortPriority" << YAML::Value << decal.GetSortPriority();
			out << YAML::Key << "AdjustAspectRatio" << YAML::Value << decal.IsAdjustAspectRatioEnabled();
			if (const auto& asset = decal.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << asset->GetGUID();
			out << YAML::Key << "IsVisible" << YAML::Value << decal.IsVisible();

			out << YAML::EndMap; //DecalComponent
		}

		void DeserializeDecalComponent(Entity deserializedEntity, YAML::Node decalNode)
		{
			auto& decal = deserializedEntity.AddComponent<DecalComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(decalNode, relativeTransform);
			decal.SetRelativeTransform(relativeTransform);
			if (auto node = decalNode["SortPriority"])
				decal.SetSortPriority(node.as<uint32_t>());
			if (auto node = decalNode["AdjustAspectRatio"])
				decal.SetAdjustAspectRatioEnabled(node.as<bool>());
			decal.SetMaterialAsset(GetAsset<AssetMaterial>(decalNode["Material"]));
			if (auto node = decalNode["IsVisible"])
				decal.SetVisible(node.as<bool>());
		}

		void SerializeNavigationMeshComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& component = entity.GetComponent<NavigationMeshComponent>();

			out << YAML::Key << key;
			out << YAML::BeginMap; // NavigationMeshComponent

			Serializer::SerializeRelativeTransform(out, component.GetRelativeTransform());
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

		void DeserializeNavigationMeshComponent(Entity deserializedEntity, YAML::Node componentNode)
		{
			auto& component = deserializedEntity.AddComponent<NavigationMeshComponent>();

			Transform relativeTransform;
			Serializer::DeserializeRelativeTransform(componentNode, relativeTransform);
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

		void SerializeNavigationCrowdAgentComponent(YAML::Emitter& out, Entity entity, const char* key)
		{
			auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();

			out << YAML::Key << key;
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

		void DeserializeNavigationCrowdAgentComponent(Entity deserializedEntity, YAML::Node componentNode)
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

		// Calls a deserialize function. It takes the valid masks of collision groups only if it uses them
		template<auto DeserializeFn>
		void DeserializeComponent(Entity deserializedEntity, YAML::Node node, uint32_t collisionGroupValidMasks)
		{
			using DeserializeFnType = decltype(DeserializeFn);
			if constexpr (std::is_invocable_v<DeserializeFnType, Entity, YAML::Node, uint32_t>)
			{
				DeserializeFn(deserializedEntity, node, collisionGroupValidMasks);
			}
			else
			{
				static_assert(std::is_invocable_v<DeserializeFnType, Entity, YAML::Node>,
					"Expected `void(Entity, YAML::Node)` or `void(Entity, YAML::Node, uint32_t collisionGroupValidMasks)`");
				DeserializeFn(deserializedEntity, node);
			}
		}

		struct SerializedComponentType
		{
			const char* Key = nullptr; // Of the component in the map of the entity
			bool (*Has)(const Entity& entity) = nullptr;
			void (*Remove)(Entity& entity) = nullptr;
			void (*Serialize)(YAML::Emitter& out, Entity entity, const char* key) = nullptr;
			void (*Deserialize)(Entity deserializedEntity, YAML::Node node, uint32_t collisionGroupValidMasks) = nullptr;
			bool bCollider = false; // It's attached to the physics actor of the rigid body, so it's recreated along with it
			bool bRigidBody = false;
		};

		template<typename T, void (*SerializeFn)(YAML::Emitter&, Entity, const char*), auto DeserializeFn>
		constexpr SerializedComponentType MakeSerializedComponentType(const char* key)
		{
			SerializedComponentType type;
			type.Key = key;
			type.Has = [](const Entity& entity) { return entity.HasComponent<T>(); };
			if constexpr (std::is_same_v<T, TagComponent>)
			{
				type.Remove = [](Entity& entity) { entity.GetComponent<TagComponent>().Tag.clear(); }; // All entities have it, so it's cleared instead
			}
			else
			{
				type.Remove = [](Entity& entity) { entity.RemoveComponent<T>(); };
			}
			type.Serialize = SerializeFn;
			type.Deserialize = &DeserializeComponent<DeserializeFn>;
			type.bCollider = std::is_base_of_v<BaseColliderComponent, T>;
			type.bRigidBody = std::is_same_v<T, RigidBodyComponent>;
			return type;
		}

		// The key is the name of the component's type. Note: renaming a component type renames its key, so files saved before that won't have the component when they're loaded
		#define EG_SERIALIZED_COMPONENT(Component) MakeSerializedComponentType<Component, &Serialize##Component, &Deserialize##Component>(#Component)

		// Every component that's saved with an entity (besides its name, parent and transform, see `Serializer::SerializeEntity()`).
		// They're written & read in this order: for example, a rigid body is read before the colliders that are attached to it.
		// A component is serialized by adding it here
		constexpr SerializedComponentType s_SerializedComponentTypes[] =
		{
			EG_SERIALIZED_COMPONENT(EntityAssetComponent),
			EG_SERIALIZED_COMPONENT(TagComponent),
			EG_SERIALIZED_COMPONENT(CameraComponent),
			EG_SERIALIZED_COMPONENT(SpriteComponent),
			EG_SERIALIZED_COMPONENT(BillboardComponent),
			EG_SERIALIZED_COMPONENT(StaticMeshComponent),
			EG_SERIALIZED_COMPONENT(SkeletalMeshComponent),
			EG_SERIALIZED_COMPONENT(PointLightComponent),
			EG_SERIALIZED_COMPONENT(DirectionalLightComponent),
			EG_SERIALIZED_COMPONENT(SpotLightComponent),
			EG_SERIALIZED_COMPONENT(ScriptComponent),
			EG_SERIALIZED_COMPONENT(RigidBodyComponent),
			EG_SERIALIZED_COMPONENT(BoxColliderComponent),
			EG_SERIALIZED_COMPONENT(SphereColliderComponent),
			EG_SERIALIZED_COMPONENT(CapsuleColliderComponent),
			EG_SERIALIZED_COMPONENT(MeshColliderComponent),
			EG_SERIALIZED_COMPONENT(CharacterControllerComponent),
			EG_SERIALIZED_COMPONENT(AudioComponent),
			EG_SERIALIZED_COMPONENT(ReverbComponent),
			EG_SERIALIZED_COMPONENT(TextComponent),
			EG_SERIALIZED_COMPONENT(Text2DComponent),
			EG_SERIALIZED_COMPONENT(Image2DComponent),
			EG_SERIALIZED_COMPONENT(ParticleSystemComponent),
			EG_SERIALIZED_COMPONENT(SceneSequenceComponent),
			EG_SERIALIZED_COMPONENT(DecalComponent),
			EG_SERIALIZED_COMPONENT(NavigationMeshComponent),
			EG_SERIALIZED_COMPONENT(NavigationCrowdAgentComponent),
		};

		#undef EG_SERIALIZED_COMPONENT
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
		out << YAML::BeginMap; //Entity

		out << YAML::Key << "EntityID" << YAML::Value << entity.GetID();
		out << YAML::Key << "GUID" << YAML::Value << entity.GetGUID();

		// Note: intentionally kept outside of `SerializeComponents` because Undo system handles parents separately
		if (entity.HasComponent<EntitySceneNameComponent>())
		{
			const auto& sceneNameComponent = entity.GetComponent<EntitySceneNameComponent>();
			const auto& name = sceneNameComponent.GetName();

			int parentID = -1;
			if (Entity parent = entity.GetParent())
				parentID = (int)parent.GetID();

			out << YAML::Key << "EntitySceneParams";
			out << YAML::BeginMap; //EntitySceneName

			out << YAML::Key << "Name" << YAML::Value << name;
			out << YAML::Key << "Parent" << YAML::Value << parentID;

			out << YAML::EndMap; //EntitySceneParams
		}

		// Note: intentionally kept outside of `SerializeComponents` because Undo system handles transforms separately
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

		SerializeComponents(out, entity);

		out << YAML::EndMap; //Entity
	}

	void Serializer::SerializeComponents(YAML::Emitter& out, Entity entity)
	{
		for (const SerializedComponentType& type : s_SerializedComponentTypes)
		{
			if (type.Has(entity))
				type.Serialize(out, entity, type.Key);
		}
	}

	Entity Serializer::DeserializeEntity(const Ref<Scene>& scene, const YAML::Node& entityNode, uint32_t collisionGroupValidMasks, uint32_t* outEntityID, int* outParentID)
	{
		*outEntityID = entityNode["EntityID"].as<uint32_t>();
		GUID guid(0, 0);
		if (auto node = entityNode["GUID"])
			guid = node.as<GUID>();
		else
			guid = GUID{}; // Generate a new one

		if (outParentID)
			*outParentID = -1;

		Entity deserializedEntity = scene->CreateEntityWithGUID(guid);

		// Note: intentionally kept outside of `DeserializeComponents` because Undo system handles parents separately
		if (auto sceneNameComponentNode = entityNode["EntitySceneParams"])
		{
			if (deserializedEntity.HasComponent<EntitySceneNameComponent>() == false)
				deserializedEntity.AddComponent<EntitySceneNameComponent>();
			
			auto& sceneNameComponent = deserializedEntity.GetComponent<EntitySceneNameComponent>();
			sceneNameComponent.SetName(sceneNameComponentNode["Name"].as<std::string>());
			if (outParentID)
				*outParentID = sceneNameComponentNode["Parent"].as<int>();
		}

		// Note: intentionally kept outside of `DeserializeComponents` because Undo system handles parents separately
		if (auto transformComponentNode = entityNode["TransformComponent"])
		{
			//Every entity has a transform component
			Transform worldTransform;

			worldTransform.Location = transformComponentNode["WorldLocation"].as<glm::vec3>();
			worldTransform.Rotation = transformComponentNode["WorldRotation"].as<Rotator>();
			worldTransform.Scale3D = transformComponentNode["WorldScale"].as<glm::vec3>();

			deserializedEntity.SetWorldTransform(worldTransform);
		}

		DeserializeComponents(deserializedEntity, entityNode, collisionGroupValidMasks);

		return deserializedEntity;
	}

	void Serializer::DeserializeComponents(Entity deserializedEntity, const YAML::Node& entityNode, uint32_t collisionGroupValidMasks)
	{
		for (const SerializedComponentType& type : s_SerializedComponentTypes)
		{
			if (YAML::Node node = entityNode[type.Key])
				type.Deserialize(deserializedEntity, node, collisionGroupValidMasks);
		}
	}

	void Serializer::SerializeEntityComponents(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap;

		// Collision groups are stored as indices into the project's collision groups. Like in scene files, the groups are saved too,
		// so that the indices of the groups that have changed since then are invalidated when it's deserialized
		SerializeProjectCollisionGroupGUIDs(out);
		SerializeComponents(out, entity);

		out << YAML::EndMap;
	}

	void Serializer::DeserializeEntityComponents(Entity entity, const YAML::Node& componentsNode)
	{
		EG_CORE_ASSERT(entity, "Invalid entity");

		// The current state, to find out which components differ
		YAML::Emitter currentOut;
		SerializeEntityComponents(currentOut, entity);
		const YAML::Node currentNode = YAML::Load(currentOut.c_str());

		auto Differs = [&currentNode, &componentsNode](const char* key)
		{
			const YAML::Node current = currentNode[key];
			const YAML::Node target = componentsNode[key];
			if (!current || !target)
				return bool(current) != bool(target);
			return YAML::Dump(current) != YAML::Dump(target);
		};

		constexpr size_t typesCount = std::size(s_SerializedComponentTypes);
		bool bRecreate[typesCount] = {};
		bool bRecreateRigidBody = false;
		for (size_t i = 0; i < typesCount; ++i)
		{
			const auto& type = s_SerializedComponentTypes[i];
			bRecreate[i] = Differs(type.Key);
			if (bRecreate[i] && type.bRigidBody)
				bRecreateRigidBody = true;
		}
		if (bRecreateRigidBody)
		{
			for (size_t i = 0; i < typesCount; ++i)
				bRecreate[i] |= s_SerializedComponentTypes[i].bCollider;
		}

		// The nav mesh of the scene is removed along with its component. When a scene is loaded, it's built after its entities were deserialized
		Scene* scene = entity.GetScene();
		bool bRebuildNavMesh = false;
		if (entity.HasComponent<NavigationMeshComponent>())
		{
			const auto& navMesh = entity.GetComponent<NavigationMeshComponent>().GetNavMesh();
			bRebuildNavMesh = navMesh && navMesh == scene->GetNavMesh();
		}

		// Removed in the reverse order, so that, for example, colliders are removed before the rigid body
		YAML::Node recreatedNode(YAML::NodeType::Map);
		for (size_t i = typesCount; i-- > 0;)
		{
			if (!bRecreate[i])
				continue;

			const SerializedComponentType& type = s_SerializedComponentTypes[i];
			if (type.Has(entity))
				type.Remove(entity);
			if (const YAML::Node node = componentsNode[type.Key])
				recreatedNode[type.Key] = node;
		}

		DeserializeComponents(entity, recreatedNode, DeserializeProjectCollisionGroupGUIDs(componentsNode));

		if (bRebuildNavMesh && entity.HasComponent<NavigationMeshComponent>())
		{
			auto& navMesh = entity.GetComponent<NavigationMeshComponent>();
			if (!navMesh.GetNavMesh())
				scene->BuildNavMesh(&navMesh);
		}
	}
}
