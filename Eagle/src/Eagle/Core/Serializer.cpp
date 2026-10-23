#include "egpch.h"
#include "Serializer.h"
#include "SerializerInternal.h"

#include "Eagle/Audio/Reverb3D.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Utils/SerializerUtils.h"

namespace Eagle
{
	void Serializer::EmitBoneNode(YAML::Emitter& out, const BoneNode& node)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Transformation" << YAML::Value << node.Transformation;
		out << YAML::Key << "Name" << YAML::Value << node.GetName();
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
		node.SetName(baseNode["Name"].as<std::string>());
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
		const auto& screenSpaceShadowsSettings = settings.ScreenSpaceShadows;
		const auto& photoLinearParams = settings.PhotoLinearTonemappingParams;
		const auto& filmicParams = settings.FilmicTonemappingParams;
		const auto& agxParams = settings.AgXTonemappingParams;
		const auto& dofSettings = settings.DOFSettings;
		const auto& motionBlurSettings = settings.MotionBlur;
		const auto& autoExposureSettings = settings.AutoExposure;
		const auto& sssr = settings.ScreenSpaceReflections;

		out << YAML::Key << "RendererSettings" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "SoftShadows" << YAML::Value << settings.bEnableSoftShadows;
		out << YAML::Key << "TranslucentShadows" << YAML::Value << settings.bTranslucentShadows;
		out << YAML::Key << "ShadowsSmoothTransition" << YAML::Value << settings.bEnableCSMSmoothTransition;
		out << YAML::Key << "EnableObjectPicking" << YAML::Value << settings.bEnableObjectPicking;
		out << YAML::Key << "Enable2DObjectPicking" << YAML::Value << settings.bEnable2DObjectPicking;
		out << YAML::Key << "SortOpaqueParticles" << YAML::Value << settings.bSortOpaqueParticles;
		out << YAML::Key << "MaxParticlesBudget" << YAML::Value << settings.MaxParticlesBudget;
		out << YAML::Key << "LineWidth" << YAML::Value << settings.LineWidth;
		out << YAML::Key << "GridCellSize" << YAML::Value << settings.GridCellSize;
		out << YAML::Key << "TransparencyLayers" << YAML::Value << settings.TransparencyLayers;
		out << YAML::Key << "AO" << YAML::Value << Utils::GetEnumName(settings.AO);
		out << YAML::Key << "AA" << YAML::Value << Utils::GetEnumName(settings.AA);
		out << YAML::Key << "GeometricSpecularAA" << YAML::Value << settings.bGeometricSpecularAA;
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
		out << YAML::Key << "Samples" << YAML::Value << gtaoSettings.Quality.NumberOfSamples;
		out << YAML::Key << "StepsPerSample" << YAML::Value << gtaoSettings.Quality.StepsPerSample;
		out << YAML::Key << "StepsPerSample" << YAML::Value << gtaoSettings.Quality.NumberOfBlurPasses;
		out << YAML::Key << "Radius" << YAML::Value << gtaoSettings.Radius;
		out << YAML::Key << "FalloffRange" << YAML::Value << gtaoSettings.FalloffRange;
		out << YAML::Key << "bGenerateBentNormals" << YAML::Value << gtaoSettings.bGenerateBentNormals;
		out << YAML::Key << "bHalfRes" << YAML::Value << gtaoSettings.bHalfRes;
		out << YAML::EndMap; // GTAO Settings

		out << YAML::Key << "MSAA Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << Utils::GetEnumName(settings.MSAAParams.Samples);
		out << YAML::Key << "EdgeThreshold" << YAML::Value << settings.MSAAParams.EdgeThreshold;
		out << YAML::EndMap; // MSAA Settings

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
		out << YAML::Key << "FogScale" << YAML::Value << volumetricSettings.FogScale;
		out << YAML::Key << "WindDirection" << YAML::Value << volumetricSettings.WindDirection;
		out << YAML::Key << "Anisotropy" << YAML::Value << volumetricSettings.Anisotropy;
		out << YAML::Key << "Density" << YAML::Value << volumetricSettings.Density;
		out << YAML::Key << "bFogEnable" << YAML::Value << volumetricSettings.bFogEnable;
		out << YAML::Key << "Resolution" << YAML::Value << Utils::GetEnumName(volumetricSettings.EvalResolution);
		out << YAML::Key << "bEnable" << YAML::Value << volumetricSettings.bEnable;
		out << YAML::EndMap; // Volumetric Light Settings

		out << YAML::Key << "Shadow Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "PointLightSize" << YAML::Value << shadowSettings.PointLightShadowMapSize;
		out << YAML::Key << "SpotLightSize" << YAML::Value << shadowSettings.SpotLightShadowMapSize;
		out << YAML::Key << "DirLightSizes" << YAML::Value << shadowSettings.DirLightShadowMapSizes;
		out << YAML::EndMap; // Shadow Settings

		out << YAML::Key << "Screen Space Shadow Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << screenSpaceShadowsSettings.Samples;
		out << YAML::Key << "HardShadowSamples" << YAML::Value << screenSpaceShadowsSettings.HardShadowSamples;
		out << YAML::Key << "FadeOutSamples" << YAML::Value << screenSpaceShadowsSettings.FadeOutSamples;
		out << YAML::Key << "SurfaceThickness" << YAML::Value << screenSpaceShadowsSettings.SurfaceThickness;
		out << YAML::Key << "BilinearThreshold" << YAML::Value << screenSpaceShadowsSettings.BilinearThreshold;
		out << YAML::Key << "ShadowContrast" << YAML::Value << screenSpaceShadowsSettings.ShadowContrast;
		out << YAML::Key << "bIgnoreEdgePixels" << YAML::Value << screenSpaceShadowsSettings.bIgnoreEdgePixels;
		out << YAML::Key << "bUsePrecisionOffset" << YAML::Value << screenSpaceShadowsSettings.bUsePrecisionOffset;
		out << YAML::Key << "bBilinearSamplingOffsetMode" << YAML::Value << screenSpaceShadowsSettings.bBilinearSamplingOffsetMode;
		out << YAML::Key << "bUseEarlyOut" << YAML::Value << screenSpaceShadowsSettings.bUseEarlyOut;
		out << YAML::EndMap; // Screen Space Shadow Settings

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

		out << YAML::Key << "AgX Tonemapping";
		out << YAML::BeginMap;
		out << YAML::Key << "Slope" << YAML::Value << agxParams.Slope;
		out << YAML::Key << "Power" << YAML::Value << agxParams.Power;
		out << YAML::Key << "Offset" << YAML::Value << agxParams.Offset;
		out << YAML::Key << "Saturation" << YAML::Value << agxParams.Saturation;
		out << YAML::EndMap; //AgX Tonemapping

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
		out << YAML::Key << "NoMotionBlurThreshold" << YAML::Value << motionBlurSettings.NoMotionBlurThreshold;
		out << YAML::Key << "LowMotionThreshold" << YAML::Value << motionBlurSettings.LowMotionThreshold;
		out << YAML::Key << "bUseCheapOnLowMotion" << YAML::Value << motionBlurSettings.bUseCheapOnLowMotion;
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
		out << YAML::Key << "VarianceThreshold" << YAML::Value << sssr.VarianceThreshold;
		out << YAML::Key << "DepthBufferThickness" << YAML::Value << sssr.DepthBufferThickness;
		out << YAML::Key << "TemporalStabilityFactor" << YAML::Value << sssr.TemporalStabilityFactor;
		out << YAML::Key << "RoughnessThreshold" << YAML::Value << sssr.RoughnessThreshold;
		out << YAML::Key << "SamplesPerQuad" << YAML::Value << sssr.SamplesPerQuad;
		out << YAML::Key << "MaxTraversalIterations" << YAML::Value << sssr.MaxTraversalIterations;
		out << YAML::Key << "MinTraversalOccupancy" << YAML::Value << sssr.MinTraversalOccupancy;
		out << YAML::Key << "bTemporalVarianceGuidedTracing" << YAML::Value << sssr.bTemporalVarianceGuidedTracing;
		out << YAML::Key << "bEnable" << YAML::Value << sssr.bEnable;
		out << YAML::EndMap; //SSSR

		out << YAML::EndMap;
	}

	void Serializer::SerializeProjectCollisionGroupGUIDs(YAML::Emitter& out)
	{
		const auto& collisionGroupGUIDs = Project::GetProjectInfo().CollisionGroupGUIDs;
		out << YAML::Key << "CollisionGroupGUIDs" << YAML::Value << YAML::BeginSeq;
		for (const auto& guid : collisionGroupGUIDs)
			out << guid;
		out << YAML::EndSeq;
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
		if (auto objectPicking = data["EnableObjectPicking"])
			settings.bEnableObjectPicking = objectPicking.as<bool>();
		if (auto objectPicking = data["Enable2DObjectPicking"])
			settings.bEnable2DObjectPicking = objectPicking.as<bool>();
		if (auto sortOpaqueParticles = data["SortOpaqueParticles"])
			settings.bSortOpaqueParticles = sortOpaqueParticles.as<bool>();
		if (auto maxParticlesBudget = data["MaxParticlesBudget"])
			settings.MaxParticlesBudget = maxParticlesBudget.as<uint32_t>();
		if (auto lineWidthNode = data["LineWidth"])
			settings.LineWidth = lineWidthNode.as<float>();
		if (auto gridCellSizeNode = data["GridCellSize"])
			settings.GridCellSize = gridCellSizeNode.as<float>();
		if (auto layersNode = data["TransparencyLayers"])
			settings.TransparencyLayers = layersNode.as<uint32_t>();
		if (auto node = data["AO"])
			settings.AO = Utils::GetEnumFromName<AmbientOcclusion>(node.as<std::string>());
		if (auto node = data["AA"])
			settings.AA = Utils::GetEnumFromName<AAMethod>(node.as<std::string>());
		if (auto node = data["GeometricSpecularAA"])
			settings.bGeometricSpecularAA = node.as<bool>();
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
			settings.GTAOSettings.Quality.NumberOfSamples = gtaoSettingsNode["Samples"].as<uint32_t>();
			if (auto node = gtaoSettingsNode["StepsPerSample"])
				settings.GTAOSettings.Quality.StepsPerSample = node.as<uint32_t>();
			if (auto node = gtaoSettingsNode["NumberOfBlurPasses"])
				settings.GTAOSettings.Quality.NumberOfBlurPasses = node.as<uint32_t>();
			settings.GTAOSettings.Radius = gtaoSettingsNode["Radius"].as<float>();
			if (auto node = gtaoSettingsNode["FalloffRange"])
				settings.GTAOSettings.FalloffRange = node.as<float>();
			if (auto node = gtaoSettingsNode["bGenerateBentNormals"])
				settings.GTAOSettings.bGenerateBentNormals = node.as<bool>();
			if (auto node = gtaoSettingsNode["bHalfRes"])
				settings.GTAOSettings.bHalfRes = node.as<bool>();
		}

		if (auto msaaSettingsNode = data["MSAA Settings"])
		{
			settings.MSAAParams.Samples = Utils::GetEnumFromName<MSAASamples>(msaaSettingsNode["Samples"].as<std::string>());
			if (auto node = msaaSettingsNode["EdgeThreshold"])
				settings.MSAAParams.EdgeThreshold = node.as<float>();
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
			if (auto node = volumetricSettingsNode["FogScale"])
				settings.VolumetricSettings.FogScale = node.as<float>();
			if (auto node = volumetricSettingsNode["WindDirection"])
				settings.VolumetricSettings.WindDirection = node.as<glm::vec3>();
			if (auto node = volumetricSettingsNode["Anisotropy"])
				settings.VolumetricSettings.Anisotropy = node.as<float>();
			if (auto node = volumetricSettingsNode["Density"])
				settings.VolumetricSettings.Density = node.as<float>();
			settings.VolumetricSettings.bFogEnable = volumetricSettingsNode["bFogEnable"].as<bool>();
			if (auto node = volumetricSettingsNode["Resolution"])
				settings.VolumetricSettings.EvalResolution = Utils::GetEnumFromName<VolumetricLightsSettings::Resolution>(node.as<std::string>());
			settings.VolumetricSettings.bEnable = volumetricSettingsNode["bEnable"].as<bool>();
		}

		if (auto shadowSettingsNode = data["Shadow Settings"])
		{
			settings.ShadowsSettings.PointLightShadowMapSize = shadowSettingsNode["PointLightSize"].as<uint32_t>();
			settings.ShadowsSettings.SpotLightShadowMapSize = shadowSettingsNode["SpotLightSize"].as<uint32_t>();
			settings.ShadowsSettings.DirLightShadowMapSizes = shadowSettingsNode["DirLightSizes"].as<std::vector<uint32_t>>();
		}

		if (auto shadowSettingsNode = data["Screen Space Shadow Settings"])
		{
			settings.ScreenSpaceShadows.Samples = shadowSettingsNode["Samples"].as<uint32_t>();
			settings.ScreenSpaceShadows.HardShadowSamples = shadowSettingsNode["HardShadowSamples"].as<uint32_t>();
			settings.ScreenSpaceShadows.FadeOutSamples = shadowSettingsNode["FadeOutSamples"].as<uint32_t>();
			settings.ScreenSpaceShadows.SurfaceThickness = shadowSettingsNode["SurfaceThickness"].as<float>();
			settings.ScreenSpaceShadows.BilinearThreshold = shadowSettingsNode["BilinearThreshold"].as<float>();
			settings.ScreenSpaceShadows.ShadowContrast = shadowSettingsNode["ShadowContrast"].as<float>();
			settings.ScreenSpaceShadows.bIgnoreEdgePixels = shadowSettingsNode["bIgnoreEdgePixels"].as<bool>();
			settings.ScreenSpaceShadows.bUsePrecisionOffset = shadowSettingsNode["bUsePrecisionOffset"].as<bool>();
			settings.ScreenSpaceShadows.bBilinearSamplingOffsetMode = shadowSettingsNode["bBilinearSamplingOffsetMode"].as<bool>();
			settings.ScreenSpaceShadows.bUseEarlyOut = shadowSettingsNode["bUseEarlyOut"].as<bool>();
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

		if (auto agxNode = data["AgX Tonemapping"])
		{
			settings.AgXTonemappingParams.Slope = agxNode["Slope"].as<glm::vec3>();
			settings.AgXTonemappingParams.Power = agxNode["Power"].as<glm::vec3>();
			settings.AgXTonemappingParams.Offset = agxNode["Offset"].as<glm::vec3>();
			settings.AgXTonemappingParams.Saturation = agxNode["Saturation"].as<float>();
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
			if (auto node = motionBlurNode["NoMotionBlurThreshold"])
				settings.MotionBlur.NoMotionBlurThreshold = node.as<float>();
			if (auto node = motionBlurNode["LowMotionThreshold"])
				settings.MotionBlur.LowMotionThreshold = node.as<float>();
			if (auto node = motionBlurNode["bUseCheapOnLowMotion"])
				settings.MotionBlur.bUseCheapOnLowMotion = node.as<bool>();
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
			if (auto node = sssrNode["VarianceThreshold"])
				settings.ScreenSpaceReflections.VarianceThreshold = node.as<float>();
			if (auto node = sssrNode["DepthBufferThickness"])
				settings.ScreenSpaceReflections.DepthBufferThickness = node.as<float>();
			if (auto node = sssrNode["TemporalStabilityFactor"])
				settings.ScreenSpaceReflections.TemporalStabilityFactor = node.as<float>();
			settings.ScreenSpaceReflections.RoughnessThreshold = sssrNode["RoughnessThreshold"].as<float>();
			settings.ScreenSpaceReflections.SamplesPerQuad = sssrNode["SamplesPerQuad"].as<uint32_t>();
			settings.ScreenSpaceReflections.MaxTraversalIterations = sssrNode["MaxTraversalIterations"].as<uint32_t>();
			if (auto node = sssrNode["MinTraversalOccupancy"])
				settings.ScreenSpaceReflections.MinTraversalOccupancy = node.as<uint32_t>();
			if (auto node = sssrNode["bTemporalVarianceGuidedTracing"])
				settings.ScreenSpaceReflections.bTemporalVarianceGuidedTracing = node.as<bool>();
			settings.ScreenSpaceReflections.bEnable = sssrNode["bEnable"].as<bool>();
		}
	}

	uint32_t Serializer::DeserializeProjectCollisionGroupGUIDs(const YAML::Node& node)
	{
		uint32_t collisionGroupValidMasks = uint32_t(s_CollisionGroupAny);

		auto groupsNode = node["CollisionGroupGUIDs"];
		if (!groupsNode)
			return collisionGroupValidMasks;

		std::vector<GUID64> collisionGroupGUIDs;
		collisionGroupGUIDs.reserve(groupsNode.size());
		for (const auto& groupNode : groupsNode)
		{
			collisionGroupGUIDs.emplace_back(groupNode.as<GUID64>());
		}

		const auto& currentGUIDs = Project::GetProjectInfo().CollisionGroupGUIDs;
		const size_t count = collisionGroupGUIDs.size();
		EG_CORE_ASSERT(count == currentGUIDs.size());
		for (size_t i = 0; i < count; ++i)
		{
			if (collisionGroupGUIDs[i] != currentGUIDs[i]) // Collision group has changed since the scene was saved, invalidate the mask
				collisionGroupValidMasks &= ~(1 << i);
		}

		return collisionGroupValidMasks;
	}

	AssetType Serializer::GetAssetType(const DataBuffer& assetData)
	{
		AssetType actualType = AssetType::None;
#if 0
		YAML::Node baseNode;
		Utils::ReadYAML(assetData, &baseNode);

		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to get an asset type");
			return AssetType::None;
		}

		if (auto node = baseNode["Type"])
			actualType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());
#else
		// This approach is much faster since we avoid parsing the whole YAML hierarchy
		// But this is less safer
		std::string_view yaml = Utils::ReadYAMLAsCString(assetData);

		constexpr char searchKey[] = "Type: ";
		const size_t pos = yaml.find(searchKey);
		if (pos != std::string::npos)
		{
			const size_t endLinePos = yaml.find_first_of('\n', pos);
			constexpr size_t keySize = sizeof(searchKey) - 1; // -1 to remove '\0'
			std::string_view type = yaml.substr(pos + keySize, endLinePos - pos - keySize);
			actualType = Utils::GetEnumFromName<AssetType>(type);
		}
#endif

		return actualType;
	}

	AssetType Serializer::GetAssetType(const Path& pathToAsset)
	{
		ScopedDataBuffer data = FileSystem::Read(pathToAsset);
		if (!data)
			return AssetType::None;
		return GetAssetType(data.GetDataBuffer());
	}

	template<typename T>
	void SerializeField(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Value << YAML::BeginMap;
		
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(field.Type);
		out << YAML::Key << "ArrayLength" << YAML::Value << field.ArrayLength;
		out << YAML::Key << "Values" << YAML::Value << YAML::BeginSeq;
		for (size_t i = 0; i < field.ArrayLength; ++i)
		{
			out << field.GetStoredValue<T>(i);
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	static void SerializeStructField(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(field.Type);
		out << YAML::Key << "StructType" << YAML::Value << field.TypeName;
		out << YAML::Key << "ArrayLength" << YAML::Value << field.ArrayLength;
		out << YAML::Key << "Values" << YAML::Value << YAML::BeginSeq;
		for (size_t i = 0; i < field.ArrayLength; ++i)
		{
			out << YAML::BeginMap;
			for (const auto& member : field.GetStructMembers(i))
			{
				if (Serializer::HasSerializableType(member))
					Serializer::SerializePublicFieldValue(out, member);
			}
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
	}

	void Serializer::SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Key << field.FullName;
		switch (field.Type)
		{
			case FieldType::Struct:
				SerializeStructField(out, field);
				break;
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
			case FieldType::AssetBehaviorGraph:
				SerializeField<GUID>(out, field);
				break;
		}
	}

	template<typename T>
	static void SetStoredValue(YAML::Node& node, PublicField& field, size_t idx)
	{
		if (idx < field.ArrayLength)
		{
			T value = node.as<T>();
			field.SetStoredValue<T>(value, idx);
		}
	}

	void Serializer::DeserializePublicFieldValues(YAML::Node& publicFieldsNode, std::vector<PublicField>& publicFields)
	{
		for (auto it : publicFieldsNode)
		{
			std::string fullName = it.first.as<std::string>();
			FieldType fieldType = Utils::GetEnumFromName<FieldType>(it.second["Type"].as<std::string>());

			auto fieldIt = std::find_if(publicFields.begin(), publicFields.end(), [&fullName](const PublicField& field)
			{
				return fullName == field.FullName;
			});

			if ((fieldIt != publicFields.end()) && (fieldType == fieldIt->Type))
			{
				// Can differ from the actual `ArrayLength` if scripts were changed
				const size_t savedArrayLength = it.second["ArrayLength"].as<size_t>();

				PublicField& field = *fieldIt;
				auto valuesNode = it.second["Values"];

				field.ResizeArray(savedArrayLength); // Does nothing for non-arrays

				if (fieldType == FieldType::Struct)
				{
					// `ResizeArray` does nothing for non-arrays, so the lengths can still differ
					// if a field was changed from `MyStruct[]` to `MyStruct` in scripts
					const size_t count = std::min(savedArrayLength, field.ArrayLength);
					for (size_t i = 0; i < count; ++i)
					{
						YAML::Node elementNode = valuesNode[i];
						if (elementNode && elementNode.IsMap())
							DeserializePublicFieldValues(elementNode, field.GetStructMembers(i));
					}
					field.ValidateEnumValues();
					continue;
				}

				for (size_t i = 0; i < savedArrayLength; ++i)
				{
					auto node = valuesNode[i];
					switch (fieldType)
					{
						case FieldType::Int:
							SetStoredValue<int>(node, field, i);
							break;
						case FieldType::UnsignedInt:
							SetStoredValue<unsigned int>(node, field, i);
							break;
						case FieldType::Float:
							SetStoredValue<float>(node, field, i);
							break;
						case FieldType::String:
							SetStoredValue<std::string>(node, field, i);
							break;
						case FieldType::Vec2:
							SetStoredValue<glm::vec2>(node, field, i);
							break;
						case FieldType::Vec3:
						case FieldType::Color3:
							SetStoredValue<glm::vec3>(node, field, i);
							break;
						case FieldType::Vec4:
						case FieldType::Color4:
							SetStoredValue<glm::vec4>(node, field, i);
							break;
						case FieldType::Bool:
							SetStoredValue<bool>(node, field, i);
							break;
						case FieldType::Enum:
							SetStoredValue<int>(node, field, i);
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
						case FieldType::AssetBehaviorGraph:
							SetStoredValue<GUID>(node, field, i);
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
			case FieldType::AssetBehaviorGraph:
			case FieldType::Struct:
				return true;
			default: return false;
		}
	}
}
