#include "egpch.h"

#include "ScriptWrappers.h"
#include "ScriptEngineRegistry.h"
#include "ScriptUserComponents.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Core/AsyncSceneLoader.h"
#include "Eagle/Classes/PSAutoDestroyScript.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include <mono/jit/jit.h>

// Note: keep last
#include "ScriptWrappersInternal.h"

namespace Eagle
{
	//--------------Input--------------
	bool Script::Eagle_Input_IsMouseButtonPressed(Mouse button)
	{
		return Input::IsMouseButtonPressed(button);
	}

	bool Script::Eagle_Input_IsKeyPressed(Key keyCode)
	{
		return Input::IsKeyPressed(keyCode);
	}

	void Script::Eagle_Input_GetMousePosition(glm::vec2* outPosition)
	{
		auto [x, y] = Input::GetMousePosition();
		*outPosition = {x, y};
	}

	void Script::Eagle_Input_GetMousePositionInViewport(glm::vec2* outPosition)
	{
		// TODO: Check if works
		const auto& scene = Scene::GetCurrentScene();
		glm::vec2 mousePos = ImGuiLayer::GetMousePos();
		mousePos -= scene->ViewportBounds[0];
		const glm::vec2 viewportSize = scene->ViewportBounds[1] - scene->ViewportBounds[0];
		mousePos = glm::clamp(mousePos, glm::vec2(0.f), viewportSize);
		*outPosition = mousePos;
	}

	void Script::Eagle_Input_SetMousePosition(const glm::vec2* position)
	{
		Input::SetMousePos(position->x, position->y);
	}

	void Script::Eagle_Input_SetMousePositionInViewport(const glm::vec2* position)
	{
		auto& scene = Scene::GetCurrentScene();

		// TODO: Check if works
		const glm::vec2 viewportSize = scene->ViewportBounds[1] - scene->ViewportBounds[0];
		const glm::vec2 mousePos = scene->ViewportBounds[0] + glm::clamp(*position, glm::vec2(0), viewportSize);
		Input::SetMousePos(mousePos.x, mousePos.y);
	}

	void Script::Eagle_Input_SetCursorMode(CursorMode mode)
	{
		Input::SetShowMouse(mode == CursorMode::Normal);
	}

	CursorMode Script::Eagle_Input_GetCursorMode()
	{
		return Input::IsMouseVisible() ? CursorMode::Normal : CursorMode::Hidden;
	}
	
	uint64_t Script::Eagle_Renderer_GetFrameNumber()
	{
		return RenderManager::GetFrameNumber_CPU();
	}

	void Script::Eagle_Renderer_GetGPUInfo(MonoString** vendor, MonoString** device, MonoString** driverVersion, MonoString** apiVersion, uint64_t* deviceVRAM, float* maxAnisotropy,
		bool* bTextureCompressionASTC_LDR, bool* bTextureCompressionETC2, bool* bTextureCompressionBC)
	{
		const auto& caps = Application::Get().GetRenderContext()->GetCapabilities();

		MonoDomain* domain = mono_domain_get();
		*vendor = mono_string_new(domain, caps.Vendor.c_str());
		*device = mono_string_new(domain, caps.Device.c_str());
		*driverVersion = mono_string_new(domain, caps.DriverVersion.c_str());
		*apiVersion = mono_string_new(domain, caps.ApiVersion.c_str());

		*deviceVRAM = caps.DeviceVRAM;
		*maxAnisotropy = caps.MaxAnisotropy;

		*bTextureCompressionASTC_LDR = caps.bTextureCompressionASTC_LDR;
		*bTextureCompressionETC2 = caps.bTextureCompressionETC2;
		*bTextureCompressionBC = caps.bTextureCompressionBC;
	}

	//-------------- Renderer --------------
	void Script::Eagle_Renderer_SetFogSettings(const glm::vec3* color, float minDistance, float maxDistance, float density, FogEquation equation, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.FogSettings.Color = *color;
		options.FogSettings.MinDistance = minDistance;
		options.FogSettings.MaxDistance = maxDistance;
		options.FogSettings.Density = density;
		options.FogSettings.Equation = equation;
		options.FogSettings.bEnable = bEnabled;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetFogSettings(glm::vec3* outcolor, float* outMinDistance, float* outMaxDistance, float* outDensity, FogEquation* outEquation, bool* outbEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outcolor = options.FogSettings.Color;
		*outMinDistance = options.FogSettings.MinDistance;
		*outMaxDistance = options.FogSettings.MaxDistance;
		*outDensity = options.FogSettings.Density;
		*outEquation = options.FogSettings.Equation;
		*outbEnabled = options.FogSettings.bEnable;
	}

	void Script::Eagle_Renderer_SetBloomSettings(GUID dirtID, float threashold, float intensity, float dirtIntensity, float knee, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		
		Ref<Asset> asset;
		Ref<AssetTexture2D> textureAsset;
		if (AssetManager::Get(dirtID, &asset))
		{
			textureAsset = Cast<AssetTexture2D>(asset);
			if (!textureAsset)
				EG_CORE_ERROR("[ScriptEngine] Couldn't set bloom settings dirt texture. Provided asset is not a texture 2D");
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set bloom settings dirt texture. Couldn't find an asset");
		}
		
		options.BloomSettings.Dirt = textureAsset;
		options.BloomSettings.Threshold = threashold;
		options.BloomSettings.Intensity = intensity;
		options.BloomSettings.DirtIntensity = dirtIntensity;
		options.BloomSettings.Knee = knee;
		options.BloomSettings.bEnable = bEnabled;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetBloomSettings(GUID* outDirtTexture, float* outThreashold, float* outIntensity, float* outDirtIntensity, float* outKnee, bool* outbEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outDirtTexture = options.BloomSettings.Dirt ? options.BloomSettings.Dirt->GetGUID() : GUID{ 0, 0 };
		*outThreashold = options.BloomSettings.Threshold;
		*outIntensity = options.BloomSettings.Intensity;
		*outDirtIntensity = options.BloomSettings.DirtIntensity;
		*outKnee = options.BloomSettings.Knee;
		*outbEnabled = options.BloomSettings.bEnable;
	}

	void Script::Eagle_Renderer_SetSSAOSettings(uint32_t samples, float radius, float bias)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.SSAOSettings.SetNumberOfSamples(samples);
		options.SSAOSettings.SetRadius(radius);
		options.SSAOSettings.SetBias(bias);
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetSSAOSettings(uint32_t* outSamples, float* outRadius, float* outBias)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.SSAOSettings.GetNumberOfSamples();
		*outRadius = options.SSAOSettings.GetRadius();
		*outBias = options.SSAOSettings.GetBias();
	}

	void Script::Eagle_Renderer_SetGTAOSettings(uint32_t samples, uint32_t stepsPerSample, float radius, float fallollRange, bool bGenerateBentNormals, bool bHalfRes)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.GTAOSettings.Quality.NumberOfSamples = samples;
		options.GTAOSettings.Quality.StepsPerSample = stepsPerSample;
		options.GTAOSettings.Radius = radius;
		options.GTAOSettings.FalloffRange = fallollRange;
		options.GTAOSettings.bGenerateBentNormals = bGenerateBentNormals;
		options.GTAOSettings.bHalfRes = bHalfRes;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetGTAOSettings(uint32_t* outSamples, uint32_t* outStepsPerSample, float* outRadius, float* outFalloffRange, bool* outGenerateBentNormals, bool* outHalfRes)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.GTAOSettings.Quality.NumberOfSamples;
		*outStepsPerSample = options.GTAOSettings.Quality.StepsPerSample;
		*outRadius = options.GTAOSettings.Radius;
		*outFalloffRange = options.GTAOSettings.FalloffRange;
		*outGenerateBentNormals = options.GTAOSettings.bGenerateBentNormals;
		*outHalfRes = options.GTAOSettings.bHalfRes;
	}

	void Script::Eagle_Renderer_SetMSAASettings(MSAASamples samples, float edgeThreshold)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.MSAAParams.Samples = samples;
		options.MSAAParams.EdgeThreshold = edgeThreshold;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetMSAASettings(MSAASamples* outSamples, float* outEdgeThreshold)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.MSAAParams.Samples;
		*outEdgeThreshold = options.MSAAParams.EdgeThreshold;
	}

	void Script::Eagle_Renderer_SetPhotoLinearTonemappingSettings(float sensitivity, float exposureTime, float fStop)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.PhotoLinearTonemappingParams.Sensitivity = sensitivity;
		options.PhotoLinearTonemappingParams.ExposureTime = exposureTime;
		options.PhotoLinearTonemappingParams.FStop = fStop;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetPhotoLinearTonemappingSettings(float* outSensitivity, float* outExposureTime, float* outfStop)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSensitivity = options.PhotoLinearTonemappingParams.Sensitivity;
		*outExposureTime = options.PhotoLinearTonemappingParams.ExposureTime;
		*outfStop = options.PhotoLinearTonemappingParams.FStop;
	}

	void Script::Eagle_Renderer_SetFilmicTonemappingSettings(float whitePoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.FilmicTonemappingParams.WhitePoint = whitePoint;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetFilmicTonemappingSettings(float* outWhitePoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outWhitePoint = options.FilmicTonemappingParams.WhitePoint;
	}

	void Script::Eagle_Renderer_SetAgXTonemappingSettings(const glm::vec3& slope, const glm::vec3& power, const glm::vec3& offset, float saturation)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.AgXTonemappingParams.Slope = slope;
		options.AgXTonemappingParams.Power = power;
		options.AgXTonemappingParams.Offset = offset;
		options.AgXTonemappingParams.Saturation = saturation;

		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetAgXTonemappingSettings(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		const auto& options = Scene::GetCurrentScene()->GetSceneRenderer()->GetOptions();
		*slope = options.AgXTonemappingParams.Slope;
		*power = options.AgXTonemappingParams.Power;
		*offset = options.AgXTonemappingParams.Offset;
		*saturation = options.AgXTonemappingParams.Saturation;
	}

	float Script::Eagle_Renderer_GetGamma()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.Gamma;
	}

	void Script::Eagle_Renderer_SetGamma(float value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.Gamma = value;
		sceneRenderer->SetOptions(options);
	}

	float Script::Eagle_Renderer_GetExposure()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.Exposure;
	}

	void Script::Eagle_Renderer_SetExposure(float value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.Exposure = value;
		sceneRenderer->SetOptions(options);
	}

	float Script::Eagle_Renderer_GetLineWidth()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.LineWidth;
	}

	void Script::Eagle_Renderer_SetLineWidth(float value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.LineWidth = value;
		sceneRenderer->SetOptions(options);
	}

	TonemappingMethod Script::Eagle_Renderer_GetTonemappingMethod()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.Tonemapping;
	}

	void Script::Eagle_Renderer_SetTonemappingMethod(TonemappingMethod value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.Tonemapping = value;
		sceneRenderer->SetOptions(options);
	}
	
	AAMethod Script::Eagle_Renderer_GetAAMethod()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.AA;
	}

	void Script::Eagle_Renderer_SetAAMethod(AAMethod value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.AA = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsGeometricSpecularAAEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bGeometricSpecularAA;
	}

	void Script::Eagle_Renderer_SetGeometicSpecularAAEnabled(bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bGeometricSpecularAA = bEnabled;
		sceneRenderer->SetOptions(options);
	}

	AmbientOcclusion Script::Eagle_Renderer_GetAO()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->GetSceneRenderer()->GetOptions().AO;
	}

	void Script::Eagle_Renderer_SetAO(AmbientOcclusion value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.AO = value;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_SetVSyncEnabled(bool value)
	{
		Application::Get().GetWindow().SetVSync(value);
	}

	bool Script::Eagle_Renderer_GetVSyncEnabled()
	{
		return Application::Get().GetWindow().IsVSync();
	}

	void Script::Eagle_Renderer_SetSoftShadowsEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bEnableSoftShadows = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetSoftShadowsEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bEnableSoftShadows;
	}

	void Script::Eagle_Renderer_SetCSMSmoothTransitionEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bEnableCSMSmoothTransition = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetCSMSmoothTransitionEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bEnableCSMSmoothTransition;
	}

	void Script::Eagle_Renderer_SetVisualizeCascades(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bVisualizeCascades = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetVisualizeCascades()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bVisualizeCascades;
	}

	void Script::Eagle_Renderer_SetVisualizeLightTiles(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bVisualizeLightTiles = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetVisualizeLightTiles()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bVisualizeLightTiles;
	}

	void Script::Eagle_Renderer_SetTransparencyLayers(uint32_t value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.TransparencyLayers = value;
		sceneRenderer->SetOptions(options);
	}

	uint32_t Script::Eagle_Renderer_GetTransparencyLayers()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.TransparencyLayers;
	}

	void Script::Eagle_Renderer_GetSkySettings(glm::vec3* sunPos, glm::vec3* cloudsColor, float* skyIntensity, float* cloudsIntensity, float* scattering, float* cirrus, float* cumulus, uint32_t* cumulusLayers, bool* bCirrus, bool* bCumulus)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sky = scene->GetSkySettings();

		*sunPos = sky.SunPos;
		*cloudsColor = sky.CloudsColor;
		*skyIntensity = sky.SkyIntensity;
		*cloudsIntensity = sky.CloudsIntensity;
		*scattering = sky.Scattering;
		*cirrus = sky.Cirrus;
		*cumulus = sky.Cumulus;
		*cumulusLayers = sky.CumulusLayers;
		*bCirrus = sky.bEnableCirrusClouds;
		*bCumulus = sky.bEnableCumulusClouds;
	}

	void Script::Eagle_Renderer_SetSkySettings(const glm::vec3* sunPos, const glm::vec3* cloudsColor, float skyIntensity, float cloudsIntensity, float scattering, float cirrus, float cumulus, uint32_t cumulusLayers, bool bEnableCirrusClouds, bool bEnableCumulusClouds)
	{
		const auto& scene = Scene::GetCurrentScene();

		SkySettings sky;
		sky.SunPos = *sunPos;
		sky.CloudsColor = *cloudsColor;
		sky.SkyIntensity = skyIntensity;
		sky.CloudsIntensity = cloudsIntensity;
		sky.Scattering = scattering;
		sky.Cirrus = cirrus;
		sky.Cumulus = cumulus;
		sky.CumulusLayers = cumulusLayers;
		sky.bEnableCirrusClouds = bEnableCirrusClouds;
		sky.bEnableCumulusClouds = bEnableCumulusClouds;
		scene->SetSkybox(sky);
	}

	void Script::Eagle_Renderer_SetUseSkyAsBackground(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->SetUseSkyAsBackground(value);
	}

	bool Script::Eagle_Renderer_GetUseSkyAsBackground()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->GetUseSkyAsBackground();
	}

	void Script::Eagle_Renderer_SetRenderSkyboxEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->SetRenderSkybox(value);
	}

	bool Script::Eagle_Renderer_IsRenderSkyboxEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->IsRenderSkyboxEnabled();
	}

	void Script::Eagle_Renderer_SetSkyboxEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->SetSkyboxEnabled(value);
	}

	bool Script::Eagle_Renderer_IsSkyboxEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->IsSkyboxEnabled();
	}

	void Script::Eagle_Renderer_SetVolumetricLightsSettings(const glm::vec3* albedo, float anisotropy, float density, uint32_t samples, float maxScatteringDist,
		float fogSpeed, float fogScale, const glm::vec3* windDirection, bool bFogEnable, VolumetricLightsSettings::Resolution resolution, bool bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto settings = sceneRenderer->GetOptions();

		settings.VolumetricSettings.Albedo = *albedo;
		settings.VolumetricSettings.Anisotropy = anisotropy;
		settings.VolumetricSettings.Density = density;
		settings.VolumetricSettings.Samples = samples;
		settings.VolumetricSettings.MaxScatteringDistance = maxScatteringDist;
		settings.VolumetricSettings.FogSpeed = fogSpeed;
		settings.VolumetricSettings.FogScale = fogScale;
		settings.VolumetricSettings.WindDirection = *windDirection;
		settings.VolumetricSettings.bFogEnable = bFogEnable;
		settings.VolumetricSettings.EvalResolution = resolution;
		settings.VolumetricSettings.bEnable = bEnable;
		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_GetVolumetricLightsSettings(glm::vec3* albedo, float* anisotropy, float* density, uint32_t* outSamples, float* outMaxScatteringDist,
		float* fogSpeed, float* fogScale, glm::vec3* windDirection, bool* bFogEnable, VolumetricLightsSettings::Resolution* resolution, bool* bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().VolumetricSettings;

		*albedo = settings.Albedo;
		*anisotropy = settings.Anisotropy;
		*density = settings.Density;
		*outSamples = settings.Samples;
		*outMaxScatteringDist = settings.MaxScatteringDistance;
		*fogSpeed = settings.FogSpeed;
		*fogScale = settings.FogScale;
		*windDirection = settings.WindDirection;
		*bFogEnable = settings.bFogEnable;
		*resolution = settings.EvalResolution;
		*bEnable = settings.bEnable;
	}

	MonoArray* Script::Eagle_Renderer_GetShadowMapsSettings(uint32_t* outPointLightSize, uint32_t* outSpotLightSize)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().ShadowsSettings;

		*outPointLightSize = settings.PointLightShadowMapSize;
		*outSpotLightSize = settings.SpotLightShadowMapSize;

		MonoClass* uintClass = mono_get_uint32_class();
		MonoArray* result = mono_array_new(mono_domain_get(), uintClass, RendererConfig::CascadesCount);
		size_t index = 0;
		for (auto& res : settings.DirLightShadowMapSizes)
			mono_array_set(result, uint32_t, index++, res);

		return result;
	}

	void Script::Eagle_Renderer_GetScreenSpaceShadowsSettings(uint32_t* samples, uint32_t* hardShadowSamples, uint32_t* fadeOutSamples, float* surfaceThickness, float* bilinearThreshold,
		float* shadowContrast, bool* bIgnoreEdgePixels, bool* bUsePrecisionOffset, bool* bBilinearSamplingOffsetMode, bool* bUseEarlyOut)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().ScreenSpaceShadows;

		*samples = settings.Samples;
		*hardShadowSamples = settings.HardShadowSamples;
		*fadeOutSamples = settings.FadeOutSamples;
		*surfaceThickness = settings.SurfaceThickness;
		*bilinearThreshold = settings.BilinearThreshold;
		*shadowContrast = settings.ShadowContrast;
		*bIgnoreEdgePixels = settings.bIgnoreEdgePixels;
		*bUsePrecisionOffset = settings.bUsePrecisionOffset;
		*bBilinearSamplingOffsetMode = settings.bBilinearSamplingOffsetMode;
		*bUseEarlyOut = settings.bUseEarlyOut;
	}

	void Script::Eagle_Renderer_GetDepthOfFieldSettings(glm::vec2* apertureShape, float* apertureSize, float* focalLength, float* COCScale, float* maxCOC)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().DOFSettings;

		*apertureShape = settings.ApertureShape;
		*apertureSize = settings.ApertureSize;
		*focalLength = settings.FocalLength;
		*COCScale = settings.COCScale;
		*maxCOC = settings.MaxCOC;
	}

	void Script::Eagle_Renderer_GetMotionBlurSettings(bool* bEnabled, uint32_t* numSamples, float* strength, float* noMotionBlurThreshold, float* lowMotionThreshold, bool* bUseCheapOnLowMotion)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().MotionBlur;

		*bEnabled = settings.bEnable;
		*numSamples = settings.NumSamples;
		*strength = settings.Strength;
		*noMotionBlurThreshold = settings.NoMotionBlurThreshold;
		*lowMotionThreshold = settings.LowMotionThreshold;
		*bUseCheapOnLowMotion = settings.bUseCheapOnLowMotion;
	}

	void Script::Eagle_Renderer_GetAutoExposureSettings(float* minLogLum, float* maxLogLum, float* adaptationSpeed, float* adaptationKey, bool* bEnabled, bool* bHalfResolution)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().AutoExposure;

		*minLogLum = settings.MinLogLum;
		*maxLogLum = settings.MaxLogLum;
		*adaptationSpeed = settings.AdaptationSpeed;
		*adaptationKey = settings.AdaptationKey;
		*bEnabled = settings.bEnable;
		*bHalfResolution = settings.bHalfResolution;
	}

	void Script::Eagle_Renderer_GetScreenSpaceReflectionsSettings(float* varianceThreshold, float* depthBufferThickness, float* temporalStabilityFactor, float* roughnessThreshold,
		uint32_t* samplesPerQuad, uint32_t* maxTraversalIterations, uint32_t* minTraversalOccupancy, bool* bTemporalVarianceGuidedTracing, bool* bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().ScreenSpaceReflections;

		*varianceThreshold = settings.VarianceThreshold;
		*depthBufferThickness = settings.DepthBufferThickness;
		*temporalStabilityFactor = settings.TemporalStabilityFactor;
		*roughnessThreshold = settings.RoughnessThreshold;
		*samplesPerQuad = settings.SamplesPerQuad;
		*maxTraversalIterations = settings.MaxTraversalIterations;
		*minTraversalOccupancy = settings.MinTraversalOccupancy;
		*bTemporalVarianceGuidedTracing = settings.bTemporalVarianceGuidedTracing;
		*bEnabled = settings.bEnable;
	}

	void Script::Eagle_Renderer_GetLensSettings(bool* bChromaticAberration, bool* bVignette, bool* bFilmGrain, float* chromaticIntensity, float* vignetteIntensity, float* filmGrainScale, float* filmGrainAmount, float* filmGrainSeedUpdateRate)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().Lens;

		*bChromaticAberration = settings.bEnableChromaticAberration;
		*bVignette = settings.bEnableVignette;
		*bFilmGrain = settings.bEnableFilmGrain;
		*chromaticIntensity = settings.ChromaticIntensity;
		*vignetteIntensity = settings.VignetteIntensity;
		*filmGrainScale = settings.FilmGrainScale;
		*filmGrainAmount = settings.FilmGrainAmount;
		*filmGrainSeedUpdateRate = settings.FilmGrainSeedUpdateRate;
	}

	void Script::Eagle_Renderer_SetShadowMapsSettings(uint32_t pointLightSize, uint32_t spotLightSize, MonoArray* dirLightSizes)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.ShadowsSettings.PointLightShadowMapSize = glm::max(pointLightSize, ShadowMapsSettings::MinPointLightShadowMapSize);
		settings.ShadowsSettings.SpotLightShadowMapSize = glm::max(spotLightSize, ShadowMapsSettings::MinSpotLightShadowMapSize);

		const uint32_t monoLength = (uint32_t)mono_array_length(dirLightSizes);
		const uint32_t length = glm::min(monoLength, RendererConfig::CascadesCount);

		for (uint32_t i = 0; i < length; ++i)
		{
			uint32_t val = mono_array_get(dirLightSizes, uint32_t, i);
			settings.ShadowsSettings.DirLightShadowMapSizes[i] = glm::max(val, ShadowMapsSettings::MinDirLightShadowMapSize);
		}

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetScreenSpaceShadowsSettings(uint32_t samples, uint32_t hardShadowSamples, uint32_t fadeOutSamples, float surfaceThickness, float bilinearThreshold,
		float shadowContrast, bool bIgnoreEdgePixels, bool bUsePrecisionOffset, bool bBilinearSamplingOffsetMode, bool bUseEarlyOut)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.ScreenSpaceShadows.Samples = samples;
		settings.ScreenSpaceShadows.HardShadowSamples = hardShadowSamples;
		settings.ScreenSpaceShadows.FadeOutSamples = fadeOutSamples;
		settings.ScreenSpaceShadows.SurfaceThickness = surfaceThickness;
		settings.ScreenSpaceShadows.BilinearThreshold = bilinearThreshold;
		settings.ScreenSpaceShadows.ShadowContrast = shadowContrast;
		settings.ScreenSpaceShadows.bIgnoreEdgePixels = bIgnoreEdgePixels;
		settings.ScreenSpaceShadows.bUsePrecisionOffset = bUsePrecisionOffset;
		settings.ScreenSpaceShadows.bBilinearSamplingOffsetMode = bBilinearSamplingOffsetMode;
		settings.ScreenSpaceShadows.bUseEarlyOut = bUseEarlyOut;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetDepthOfFieldSettings(const glm::vec2* apertureShape, float apertureSize, float focalLength, float COCScale, float maxCOC)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.DOFSettings.ApertureShape = *apertureShape;
		settings.DOFSettings.ApertureSize = apertureSize;
		settings.DOFSettings.FocalLength = focalLength;
		settings.DOFSettings.COCScale = COCScale;
		settings.DOFSettings.MaxCOC = maxCOC;
		
		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetMotionBlurSettings(bool bEnabled, uint32_t numSamples, float strength, float noMotionBlurThreshold, float lowMotionThreshold, bool bUseCheapOnLowMotion)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.MotionBlur.bEnable = bEnabled;
		settings.MotionBlur.NumSamples = numSamples;
		settings.MotionBlur.Strength = strength;
		settings.MotionBlur.NoMotionBlurThreshold = noMotionBlurThreshold;
		settings.MotionBlur.LowMotionThreshold = lowMotionThreshold;
		settings.MotionBlur.bUseCheapOnLowMotion = bUseCheapOnLowMotion;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetAutoExposureSettings(float minLogLum, float maxLogLum, float adaptationSpeed, float adaptationKey, bool bEnabled, bool bHalfResolution)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.AutoExposure.MinLogLum = minLogLum;
		settings.AutoExposure.MaxLogLum = maxLogLum;
		settings.AutoExposure.AdaptationSpeed = adaptationSpeed;
		settings.AutoExposure.AdaptationKey = adaptationKey;
		settings.AutoExposure.bEnable = bEnabled;
		settings.AutoExposure.bHalfResolution = bHalfResolution;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetScreenSpaceReflectionsSettings(float varianceThreshold, float depthBufferThickness, float temporalStabilityFactor, float roughnessThreshold,
		uint32_t samplesPerQuad, uint32_t maxTraversalIterations, uint32_t minTraversalOccupancy, bool bTemporalVarianceGuidedTracing, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.ScreenSpaceReflections.VarianceThreshold = varianceThreshold;
		settings.ScreenSpaceReflections.DepthBufferThickness = depthBufferThickness;
		settings.ScreenSpaceReflections.TemporalStabilityFactor = temporalStabilityFactor;
		settings.ScreenSpaceReflections.RoughnessThreshold = roughnessThreshold;
		settings.ScreenSpaceReflections.SamplesPerQuad = samplesPerQuad;
		settings.ScreenSpaceReflections.MaxTraversalIterations = maxTraversalIterations;
		settings.ScreenSpaceReflections.MinTraversalOccupancy = minTraversalOccupancy;
		settings.ScreenSpaceReflections.bTemporalVarianceGuidedTracing = bTemporalVarianceGuidedTracing;
		settings.ScreenSpaceReflections.bEnable = bEnabled;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetLensSettings(bool bChromaticAberration, bool bVignette, bool bFilmGrain, float chromaticIntensity, float vignetteIntensity, float filmGrainScale, float filmGrainAmount, float filmGrainSeedUpdateRate)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.Lens.bEnableChromaticAberration = bChromaticAberration;
		settings.Lens.bEnableVignette = bVignette;
		settings.Lens.bEnableFilmGrain = bFilmGrain;
		settings.Lens.ChromaticIntensity = chromaticIntensity;
		settings.Lens.VignetteIntensity = vignetteIntensity;
		settings.Lens.FilmGrainScale = filmGrainScale;
		settings.Lens.FilmGrainAmount = filmGrainAmount;
		settings.Lens.FilmGrainSeedUpdateRate = filmGrainSeedUpdateRate;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetTranslucentShadowsEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bTranslucentShadows = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetTranslucentShadowsEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bTranslucentShadows;
	}

	void Script::Eagle_Renderer_GetCameraTransform(Transform* outTransform)
	{
		*outTransform = Scene::GetCurrentScene()->GetRuntimeCamera()->GetWorldTransform();
	}

	void Script::Eagle_Renderer_GetViewportSize(glm::vec2* outSize)
	{
		*outSize = glm::vec2(Scene::GetCurrentScene()->GetSceneRenderer()->GetViewportSize());
	}

	void Script::Eagle_Renderer_SetUIScaleSettings(const glm::vec2* referenceResolution, UIScaleMode mode, float matchWidthOrHeight)
	{
		UIScaleSettings settings;
		settings.ReferenceResolution = glm::max(glm::uvec2(glm::round(glm::max(*referenceResolution, glm::vec2(0.f)))), glm::uvec2(1u));
		settings.Mode = mode;
		settings.MatchWidthOrHeight = glm::clamp(matchWidthOrHeight, 0.f, 1.f);
		Scene::GetCurrentScene()->GetSceneRenderer()->SetUIScaleSettingsOverride(settings);
	}

	void Script::Eagle_Renderer_GetUIScaleSettings(glm::vec2* outReferenceResolution, UIScaleMode* outMode, float* outMatchWidthOrHeight)
	{
		const UIScaleSettings& settings = Scene::GetCurrentScene()->GetSceneRenderer()->GetUIScaleSettings();
		*outReferenceResolution = glm::vec2(settings.ReferenceResolution);
		*outMode = settings.Mode;
		*outMatchWidthOrHeight = settings.MatchWidthOrHeight;
	}

	void Script::Eagle_Renderer_ResetUIScaleSettings()
	{
		Scene::GetCurrentScene()->GetSceneRenderer()->SetUIScaleSettingsOverride(std::nullopt);
	}

	float Script::Eagle_Renderer_GetUIScale()
	{
		return Scene::GetCurrentScene()->GetSceneRenderer()->GetUIScale();
	}

	void Script::Eagle_Renderer_SetSkybox(GUID cubemapID)
	{
		auto& scene = Scene::GetCurrentScene();
		if (cubemapID.IsNull())
		{
			scene->SetSkybox(nullptr);
			return;
		}
		
		Ref<Asset> asset;
		if (AssetManager::Get(cubemapID, &asset) == false)
		{
			EG_CORE_ERROR("Could set skybox. The asset is not found!");
			return;
		}
		
		Ref<AssetTextureCube> cubemap = Cast<AssetTextureCube>(asset);
		if (!cubemap)
		{
			EG_CORE_ERROR("Couldn't set the skybox. Provided asset is not a cube texture!");
			return;
		}
		
		scene->SetSkybox(cubemap);
	}

	GUID Script::Eagle_Renderer_GetSkybox()
	{
		const auto& skybox = Scene::GetCurrentScene()->GetSkybox();
		return skybox ? skybox->GetGUID() : GUID{0, 0};
	}

	void Script::Eagle_Renderer_SetCubemapIntensity(float intensity)
	{
		Scene::GetCurrentScene()->SetSkyboxIntensity(intensity);
	}

	float Script::Eagle_Renderer_GetCubemapIntensity()
	{
		return Scene::GetCurrentScene()->GetSkyboxIntensity();
	}

	void Script::Eagle_Renderer_DrawLine(const glm::vec3* startColor, const glm::vec3* endColor, const glm::vec3* start, const glm::vec3* end)
	{
		RendererLine line;
		line.Start.Color = *startColor;
		line.Start.Location = *start;
		line.End.Color = *endColor;
		line.End.Location = *end;
		Scene::GetCurrentScene()->DrawDebugLine(line);
	}

	void Script::Eagle_Renderer_DrawTriangle(const glm::vec3* v0Location, const glm::vec3* v0Color, const glm::vec3* v1Location, const glm::vec3* v1Color, const glm::vec3* v2Location, const glm::vec3* v2Color)
	{
		RendererTriangle triangle;
		triangle.Vertices[0].Location = *v0Location;
		triangle.Vertices[0].Color = *v0Color;
		triangle.Vertices[1].Location = *v1Location;
		triangle.Vertices[1].Color = *v1Color;
		triangle.Vertices[2].Location = *v2Location;
		triangle.Vertices[2].Color = *v2Color;

		Scene::GetCurrentScene()->DrawDebugTriangle(triangle);
	}

	void Script::Eagle_Renderer_DrawArrow(const glm::vec3* start, const glm::vec3* end, const glm::vec3* up)
	{
		Scene::GetCurrentScene()->DrawArrow(*start, *end, *up);
	}

	void Script::Eagle_Renderer_DrawAABB(const AABB* aabb, const Transform* transform)
	{
		Scene::GetCurrentScene()->DrawAABB(*aabb, *transform);
	}

	void Script::Eagle_Renderer_DrawBox(const AABB* aabb, const Transform* transform)
	{
		Scene::GetCurrentScene()->DrawBox(*aabb, *transform);
	}

	void Script::Eagle_Renderer_DrawCone(const glm::vec3* location, const glm::quat* rotation, float distance, float angleRad)
	{
		Scene::GetCurrentScene()->DrawCone(*location, *rotation, distance, angleRad);
	}

	void Script::Eagle_AgXTonemapping_GetDefaultLook(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		AgXTonemappingSettings agx = AgXTonemappingSettings::GetDefaultLook();

		*slope = agx.Slope;
		*power = agx.Power;
		*offset = agx.Offset;
		*saturation = agx.Saturation;
	}

	void Script::Eagle_AgXTonemapping_GetGoldenLook(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		AgXTonemappingSettings agx = AgXTonemappingSettings::GetGoldenLook();

		*slope = agx.Slope;
		*power = agx.Power;
		*offset = agx.Offset;
		*saturation = agx.Saturation;
	}

	void Script::Eagle_AgXTonemapping_GetPunchyLook(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		AgXTonemappingSettings agx = AgXTonemappingSettings::GetPunchyLook();

		*slope = agx.Slope;
		*power = agx.Power;
		*offset = agx.Offset;
		*saturation = agx.Saturation;
	}

	void Script::Eagle_GTAO_GetQuality_Low(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetLowQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}

	void Script::Eagle_GTAO_GetQuality_Medium(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetMediumQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}

	void Script::Eagle_GTAO_GetQuality_High(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetHighQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}

	void Script::Eagle_GTAO_GetQuality_Ultra(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetUltraQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}
	
	void Script::Eagle_Renderer_SetObjectPickingEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bEnableObjectPicking = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsObjectPickingEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bEnableObjectPicking;
	}

	void Script::Eagle_Renderer_Set2DObjectPickingEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bEnable2DObjectPicking = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_Is2DObjectPickingEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bEnable2DObjectPicking;
	}

	void Script::Eagle_Renderer_SetSortOpaqueParticlesEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bSortOpaqueParticles = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsSortOpaqueParticlesEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bSortOpaqueParticles;
	}

	void Script::Eagle_Renderer_SetMaxParticlesBudget(uint32_t value)
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.MaxParticlesBudget = value;
		sceneRenderer->SetOptions(options);
	}

	uint32_t Script::Eagle_Renderer_GetMaxParticlesBudget()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().MaxParticlesBudget;
	}

	void Script::Eagle_Renderer_SetDebugLinesDepthTestEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bEnableDebugLinesDepthTest = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsDebugLinesDepthTestEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bEnableDebugLinesDepthTest;
	}

	//-------------- Project --------------
	MonoString* Script::Eagle_Project_GetProjectPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetProjectPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetBinariesPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetBinariesPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetConfigPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetConfigPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetContentPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetContentPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetCachePath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetCachePath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetRendererCachePath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetRendererCachePath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetSavedPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetSavedPath()).c_str());
	}

	//-------------- SceneLoadRequest --------------
	float Script::Eagle_SceneLoadRequest_GetProgress(uint64_t id)
	{
		const Ref<SceneLoadRequest> request = AsyncSceneLoader::FindRequest(id);
		return request ? request->GetProgress() : 0.f;
	}

	int32_t Script::Eagle_SceneLoadRequest_GetState(uint64_t id)
	{
		const Ref<SceneLoadRequest> request = AsyncSceneLoader::FindRequest(id);
		return int32_t(request ? request->GetState() : SceneLoadState::Cancelled);
	}

	GUID Script::Eagle_SceneLoadRequest_GetSceneAssetGUID(uint64_t id)
	{
		const Ref<SceneLoadRequest> request = AsyncSceneLoader::FindRequest(id);
		return request ? request->GetSceneAsset()->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SceneLoadRequest_Activate(uint64_t id)
	{
		if (const Ref<SceneLoadRequest> request = AsyncSceneLoader::FindRequest(id))
			request->Activate();
		else
			EG_CORE_ERROR("Failed to activate a scene load request. The request wasn't found");
	}

	void Script::Eagle_SceneLoadRequest_Cancel(uint64_t id)
	{
		if (const Ref<SceneLoadRequest> request = AsyncSceneLoader::FindRequest(id))
			request->Cancel();
	}

	//-------------- Scene --------------
	void Script::Eagle_Scene_OpenScene(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't open a scene. Couldn't find an asset");
			return;
		}

		Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset);
		if (!sceneAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't open a scene. It's not a scene asset");
			return;
		}

		Scene::OpenScene(sceneAsset, true, true);
	}

	uint64_t Script::Eagle_Scene_OpenSceneAsync(GUID assetID, bool bActivateWhenReady)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset);
		if (!sceneAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't open a scene asynchronously. {}", asset ? "It's not a scene asset" : "Couldn't find an asset");
			return 0;
		}

		Ref<SceneLoadRequest> request = Scene::OpenSceneAsync(sceneAsset, bActivateWhenReady, true, true);
		return request ? request->GetID() : 0;
	}

	uint64_t Script::Eagle_Scene_GetActiveSceneLoad()
	{
		const Ref<SceneLoadRequest> request = AsyncSceneLoader::GetActiveRequest();
		return request ? request->GetID() : 0;
	}

	void Script::Eagle_Scene_QuitGame()
	{
		WindowCloseEvent e(true);
		Application::Get().OnEvent(e);
	}

	MonoObject* Script::Eagle_Scene_GetUIEntityAt(const glm::vec2* positionInViewport)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (Entity entity = scene->GetUIElementAt(*positionInViewport))
			return ScriptEngine::GetEntityMonoObject(entity);
		return nullptr;
	}

	MonoObject* Script::Eagle_Scene_GetHoveredUIEntity()
	{
		const auto& scene = Scene::GetCurrentScene();
		if (Entity entity = scene->GetHoveredUIElement())
			return ScriptEngine::GetEntityMonoObject(entity);
		return nullptr;
	}

	bool Script::Eagle_Scene_Raycast(const glm::vec3* origin, const glm::vec3* dir, float maxDistance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore,
		MonoObject** outHitEntity, glm::vec3* outPosition, glm::vec3* outNormal, float* outDistance)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		RaycastHit hit{};
		bool bHit = false;

		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			bHit = physicsScene->Raycast(*origin, *dir, maxDistance, query, collisionGroup, &hit, &entitiesToIgnore);
		}
		else
		{
			bHit = physicsScene->Raycast(*origin, *dir, maxDistance, query, collisionGroup, &hit);
		}


		*outHitEntity = hit.HitEntity ? ScriptEngine::GetEntityMonoObject(hit.HitEntity) : nullptr;
		*outPosition = hit.Position;
		*outNormal = hit.Normal;
		*outDistance = hit.Distance;

		return bHit;
	}

	MonoArray* Script::Eagle_Scene_OverlapBox(const Transform* transform, const glm::vec3* boxHalfSize, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->OverlapBox(*transform, *boxHalfSize, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->OverlapBox(*transform, *boxHalfSize, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_OverlapCapsule(const Transform* transform, float radius, float halfHeight, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->OverlapCapsule(*transform, radius, halfHeight, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->OverlapCapsule(*transform, radius, halfHeight, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_OverlapSphere(const Transform* transform, float radius, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->OverlapSphere(*transform, radius, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->OverlapSphere(*transform, radius, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_SweepBox(const Transform* transform, const glm::vec3* boxHalfSize, const glm::vec3* direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->SweepBox(*transform, *boxHalfSize, *direction, distance, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->SweepBox(*transform, *boxHalfSize, *direction, distance, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_SweepCapsule(const Transform* transform, float radius, float halfHeight, const glm::vec3* direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->SweepCapsule(*transform, radius, halfHeight, *direction, distance, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->SweepCapsule(*transform, radius, halfHeight, *direction, distance, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_SweepSphere(const Transform* transform, float radius, const glm::vec3* direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->SweepSphere(*transform, radius, *direction, distance, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->SweepSphere(*transform, radius, *direction, distance, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	void Script::Eagle_Scene_SetGravity(const glm::vec3* gravity)
	{
		Scene::GetCurrentScene()->SetGravity(*gravity);
	}

	void Script::Eagle_Scene_GetGravity(glm::vec3* gravity)
	{
		*gravity = Scene::GetCurrentScene()->GetGravity();
	}

	MonoArray* Script::Eagle_Scene_GetAllEntitiesWithComponent(void* type)
	{
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		const ComponentBindings* bindings = ScriptEngineRegistry::FindComponentBindings(monoType);
		if (!bindings)
		{
			EG_CORE_ERROR("[ScriptEngine] Failed to call 'GetAllEntitiesWithComponent'. Unknown type");
			return mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), 0);
		}

		std::vector<Entity> entities = bindings->GetAllEntitiesWith(Scene::GetCurrentScene());

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), entities.size());
		size_t index = 0;
		for (auto& entity : entities)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(entity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	GUID Script::Eagle_Scene_SpawnEntity(MonoString* monoName)
	{
		auto& scene = Scene::GetCurrentScene();
		const std::string name = MonoStringHandler(monoName).c_str();
		return scene->CreateEntity(name).GetGUID();
	}

	MonoObject* Script::Eagle_Scene_SpawnEntityFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't spawn entity. Couldn't find an Entity asset");
			return nullptr;
		}

		if (Ref<AssetEntity> entityAsset = Cast<AssetEntity>(asset))
		{
			auto& scene = Scene::GetCurrentScene();
			return ScriptEngine::GetEntityMonoObject(scene->CreateFromEntityAsset(entityAsset));
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't spawn entity. It's not an Entity asset");
		return nullptr;
	}

	GUID Script::Eagle_Scene_SpawnParticleSystem(MonoString* monoName, const Transform* transform, GUID assetID, bool bAutoDestroy)
	{
		Ref<AssetParticleSystem> psAsset;
		if (!assetID.IsNull())
		{
			Ref<Asset> asset;
			AssetManager::Get(assetID, &asset);
			if (!asset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't spawn particle system. Couldn't find a Particle System asset");
				return GUID(0, 0);
			}

			psAsset = Cast<AssetParticleSystem>(asset);
			if (!psAsset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't spawn particle system. Provided asset is not a Particle System asset");
				return GUID(0, 0);
			}
		}

		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->CreateEntity(MonoStringHandler(monoName).c_str());
		entity.SetWorldTransform(*transform);
		auto& ps = entity.AddComponent<ParticleSystemComponent>();
		ps.bAutospawn = true;
		ps.SetAsset(psAsset);

		if (bAutoDestroy)
			entity.AddComponent<NativeScriptComponent>().Bind<PSAutoDestroyScript>();

		return entity.GetGUID();
	}

	//-------------- Navigation --------------
	MonoArray* Script::Eagle_Navigation_FindStraightPath(const glm::vec3* start, const glm::vec3* end, uint32_t maxPolys)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();

		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindStraightPath`. There's no a navigation mesh");
			return mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), 0);
		}

		std::vector<glm::vec3> path = navMesh->FindStraightPath(*start, *end, maxPolys);
		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), path.size());
		size_t index = 0;
		for (auto& point : path)
		{
			mono_array_set(result, glm::vec3, index++, point);
		}
		return result;
	}

	MonoArray* Script::Eagle_Navigation_FindSmoothPath(const glm::vec3* start, const glm::vec3* end, uint32_t maxPolys, uint32_t maxSmooth)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindSmoothPath`. There's no a navigation mesh");
			return mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), 0);
		}

		std::vector<glm::vec3> path = navMesh->FindSmoothPath(*start, *end, maxPolys);
		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), path.size());
		size_t index = 0;
		for (auto& point : path)
		{
			mono_array_set(result, glm::vec3, index++, point);
		}
		return result;
	}

	bool Script::Eagle_Navigation_FindDistanceToWall(const glm::vec3* pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindDistanceToWall`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->FindDistanceToWall(*pos, maxRadius, outHitPos, outHitNormal, outHitDistance);
		return bSuccess;
	}

	bool Script::Eagle_Navigation_FindRandomPoint(glm::vec3* outRandomPoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindRandomPoint`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->FindRandomPoint(outRandomPoint);
		return bSuccess;
	}

	bool Script::Eagle_Navigation_FindRandomPointInCircle(const glm::vec3* pos, float radius, glm::vec3* outRandomPoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindRandomPointInCircle`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->FindRandomPointInCircle(*pos, radius, outRandomPoint);
		return bSuccess;
	}

	bool Script::Eagle_Navigation_IsValidPoint(const glm::vec3* pos)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsValidPoint`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->IsValidPoint(*pos);
		return bSuccess;
	}

	//-------------- CrowdNavigation --------------
	void Script::Eagle_CrowdNavigation_SetMoveTarget(const glm::vec3* pos)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMoveTarget`. There's no a navigation mesh");
			return;
		}

		navMesh->GetCrowd().SetMoveTarget(*pos);
	}

	void Script::Eagle_CrowdNavigation_ResetMoveTarget()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMoveTarget`. There's no a navigation mesh");
			return;
		}

		navMesh->GetCrowd().ResetMoveTarget();
	}

	//-------------- Log --------------
	void Script::Eagle_Log_Trace(MonoString* message)
	{
		if (message)
			EG_TRACE(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Info(MonoString* message)
	{
		if (message)
			EG_INFO(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Warn(MonoString* message)
	{
		if (message)
			EG_WARN(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Error(MonoString* message)
	{
		if (message)
			EG_ERROR(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Critical(MonoString* message)
	{
		if (message)
			EG_CRITICAL(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	//--------------Asset--------------
	bool Script::Eagle_Asset_Get(MonoString* path, AssetType* outType, GUID* outGUID)
	{
		Path filepath = Eagle::Utils::AsPath(MonoStringHandler(path).c_str());
		Ref<Asset> asset;
		if (AssetManager::Get(filepath, &asset))
		{
			*outType = asset->GetAssetType();
			*outGUID = asset->GetGUID();
			return true;
		}
		return false;
	}

	MonoString* Script::Eagle_Asset_GetPath(GUID guid)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(guid, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get an asset path. Couldn't find an asset");
			return nullptr;
		}

		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(asset->GetPath()).c_str());
	}

	AssetType Script::Eagle_Asset_GetAssetType(GUID guid)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(guid, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get an asset type. Couldn't find an asset");
			return AssetType::None;
		}

		return asset->GetAssetType();
	}

	//--------------AssetTexture2D--------------
	void Script::Eagle_AssetTexture2D_SetAnisotropy(GUID id, float anisotropy)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set anisotropy. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set anisotropy. It's not a texture 2D asset");
			return;
		}

		textureAsset->GetTexture()->SetAnisotropy(anisotropy);
	}

	float Script::Eagle_AssetTexture2D_GetAnisotropy(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get anisotropy. Couldn't find an asset");
			return 1.f;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get anisotropy. It's not a texture 2D asset");
			return 1.f;
		}

		return textureAsset->GetTexture()->GetAnisotropy();
	}

	void Script::Eagle_AssetTexture2D_SetFilterMode(GUID id, FilterMode filterMode)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set filter mode. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set filter mode. It's not a texture 2D asset");
			return;
		}

		textureAsset->GetTexture()->SetFilterMode(filterMode);
	}

	FilterMode Script::Eagle_AssetTexture2D_GetFilterMode(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get filter mode. Couldn't find an asset");
			return FilterMode::Point;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get filter mode. It's not a texture 2D asset");
			return FilterMode::Point;
		}

		return textureAsset->GetTexture()->GetFilterMode();
	}

	void Script::Eagle_AssetTexture2D_SetAddressMode(GUID id, AddressMode addressMode)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set address mode. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set address mode. It's not a texture 2D asset");
			return;
		}

		textureAsset->GetTexture()->SetAddressMode(addressMode);
	}

	AddressMode Script::Eagle_AssetTexture2D_GetAddressMode(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get address mode. Couldn't find an asset");
			return AddressMode::Wrap;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get address mode. It's not a texture 2D asset");
			return AddressMode::Wrap;
		}

		return textureAsset->GetTexture()->GetAddressMode();
	}

	void Script::Eagle_AssetTexture2D_SetMipsCount(GUID id, uint32_t mipsCount)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mips count. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mips count. It's not a texture 2D asset");
			return;
		}

		constexpr uint32_t minMips = 1u;
		auto& texture = textureAsset->GetTexture();
		const uint32_t maxMips = CalculateMipCount(texture->GetSize());
		mipsCount = glm::clamp(mipsCount, minMips, maxMips);
		texture->GenerateMips(mipsCount);
	}

	uint32_t Script::Eagle_AssetTexture2D_GetMipsCount(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mips count. Couldn't find an asset");
			return 1u;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mips count. It's not a texture 2D asset");
			return 1u;
		}

		return textureAsset->GetTexture()->GetMipsCount();
	}

	void Script::Eagle_AssetTexture2D_SetFormat(GUID id, AssetTexture2DFormat value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set AssetTexture2D format. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set AssetTexture2D format. It's not a texture 2D asset");
			return;
		}

		if (textureAsset->IsCompressed())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set AssetTexture2D format. Compressed textures only support RGBA8 format. Current format: {}", Eagle::Utils::GetEnumName(textureAsset->GetFormat()));
			return;
		}
		textureAsset->SetFormat(value);
	}

	void Script::Eagle_AssetTexture2D_SetIsNormalMap(GUID id, bool value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsNormalMap` for AssetTexture2D. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsNormalMap` for AssetTexture2D. It's not a texture 2D asset");
			return;
		}

		textureAsset->SetIsNormalMap(value);
	}

	void Script::Eagle_AssetTexture2D_SetCompression(GUID id, TextureCompressor::Quality quality)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsCompressed` for AssetTexture2D. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsCompressed` for AssetTexture2D. It's not a texture 2D asset");
			return;
		}

		textureAsset->SetCompression(quality, textureAsset->GetTexture()->GetMipsCount());
	}

	AssetTexture2DFormat Script::Eagle_AssetTexture2D_GetFormat(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` of AssetTexture2D. Couldn't find an asset");
			return AssetTexture2DFormat::Default;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` of AssetTexture2D. It's not a texture 2D asset");
			return AssetTexture2DFormat::Default;
		}

		return textureAsset->GetFormat();
	}

	bool Script::Eagle_AssetTexture2D_IsNormalMap(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsNormalMap` of AssetTexture2D. Couldn't find an asset");
			return false;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsNormalMap` of AssetTexture2D. It's not a texture 2D asset");
			return false;
		}

		return textureAsset->IsNormalMap();
	}

	TextureCompressor::Quality Script::Eagle_AssetTexture2D_GetCompressionQuality(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCompressionQuality` of AssetTexture2D. Couldn't find an asset");
			return TextureCompressor::Quality::Disabled;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCompressionQuality` of AssetTexture2D. It's not a texture 2D asset");
			return TextureCompressor::Quality::Disabled;
		}

		return textureAsset->GetCompressionQuality();
	}

	//--------------AssetTextureCube--------------
	void Script::Eagle_AssetTextureCube_SetLayerSize(GUID id, uint32_t value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetLayerSize` for AssetTextureCube. Couldn't find an asset");
			return;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetLayerSize` for AssetTextureCube. It's not a texture cube asset");
			return;
		}

		textureAsset->SetLayerSize(value);
	}

	void Script::Eagle_AssetTextureCube_SetPrefilterSize(GUID id, uint32_t value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPrefilterSize` for AssetTextureCube. Couldn't find an asset");
			return;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPrefilterSize` for AssetTextureCube. It's not a texture cube asset");
			return;
		}

		textureAsset->SetPrefilterSize(value);
	}

	bool Script::Eagle_AssetTextureCube_SetFormat(GUID id, AssetTextureCubeFormat value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFormat` for AssetTextureCube. Couldn't find an asset");
			return false;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFormat` for AssetTextureCube. It's not a texture cube asset");
			return false;
		}

		return textureAsset->SetFormat(value);
	}

	bool Script::Eagle_AssetTextureCube_SetCompressed(GUID id, bool bCompress)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCompressed` for AssetTextureCube. Couldn't find an asset");
			return false;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCompressed` for AssetTextureCube. It's not a texture cube asset");
			return false;
		}

		return textureAsset->SetCompressed(bCompress);
	}

	uint32_t Script::Eagle_AssetTextureCube_GetLayerSize(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLayerSize` for AssetTextureCube. Couldn't find an asset");
			return 0u;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLayerSize` for AssetTextureCube. It's not a texture cube asset");
			return 0u;
		}

		return textureAsset->GetTexture()->GetSize().x;
	}

	uint32_t Script::Eagle_AssetTextureCube_GetPrefilterSize(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPrefilterSize` for AssetTextureCube. Couldn't find an asset");
			return 0u;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPrefilterSize` for AssetTextureCube. It's not a texture cube asset");
			return 0u;
		}

		return textureAsset->GetTexture()->GetPrefilterSize();
	}

	AssetTextureCubeFormat Script::Eagle_AssetTextureCube_GetFormat(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` for AssetTextureCube. Couldn't find an asset");
			return AssetTextureCubeFormat::Default;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` for AssetTextureCube. It's not a texture cube asset");
			return AssetTextureCubeFormat::Default;
		}

		return textureAsset->GetFormat();
	}

	bool Script::Eagle_AssetTextureCube_IsCompressed(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsCompressed` for AssetTextureCube. Couldn't find an asset");
			return false;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsCompressed` for AssetTextureCube. It's not a texture cube asset");
			return false;
		}

		return textureAsset->IsCompressed();
	}

	//--------------AssetMaterial--------------
	GUID Script::Eagle_AssetMaterial_Create()
	{
		Ref<Material> material = Material::Create();
		Ref<AssetMaterial> materialAsset = AssetMaterial::Create(material);
		AssetManager::AddRuntimeAsset(materialAsset);
		return materialAsset->GetGUID();
	}

	GUID Script::Eagle_AssetMaterial_CreateFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create material asset. Couldn't find an asset");
			return GUID(0, 0);
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
		{
			Ref<Material> material = Material::Create(materialAsset->GetMaterial());
			Ref<AssetMaterial> newAsset = AssetMaterial::Create(material);
			AssetManager::AddRuntimeAsset(newAsset);
			return newAsset->GetGUID();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't create material asset. It's not a material asset");
		return GUID(0, 0);
	}

	void Script::Eagle_AssetMaterial_GetMaterial(GUID assetID,
		GUID* outAlbedoTexture, GUID* outMetalnessTexture, GUID* outNormalTexture, GUID* outRoughnessTexture, GUID* outAOTexture, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec3* albedo, float* metalness, float* roughness, float* ao, glm::vec3* emissive, float* opacity, float* opacityMask,
		bool* bUseAlbedoTexture, bool* bUseMetalnessTexture, bool* bUseRoughnessTexture, bool* bUseAOTexture, bool* bUseEmissiveTexture, bool* bUseOpacityTexture, bool* bUseOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, glm::vec2* outTilingFactor, MaterialBlendMode* outBlendMode, bool* bDoubleSided,
		Material::TextureChannel* outMetalnessTextureChannel, Material::TextureChannel* outRoughnessTextureChannel, Material::TextureChannel* outAOTextureChannel,
		Material::TextureChannel* outOpacityTextureChannel, Material::TextureChannel* outOpacityMaskTextureChannel)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material. Couldn't find an asset");
			return;
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
		{
			Utils::GetMaterial(materialAsset->GetMaterial(),
				outAlbedoTexture, outMetalnessTexture, outNormalTexture, outRoughnessTexture, outAOTexture, outEmissiveTexture, outOpacityTexture, outOpacityMaskTexture,
				albedo, metalness, roughness, ao, emissive, opacity, opacityMask,
				bUseAlbedoTexture, bUseMetalnessTexture, bUseRoughnessTexture, bUseAOTexture, bUseEmissiveTexture, bUseOpacityTexture, bUseOpacityMaskTexture,
				outTint, outEmissiveIntensity, outTilingFactor, outBlendMode, bDoubleSided, outMetalnessTextureChannel, outRoughnessTextureChannel, outAOTextureChannel, outOpacityTextureChannel, outOpacityMaskTextureChannel);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material. It's not a material asset");
	}

	void Script::Eagle_AssetMaterial_SetMaterial(GUID assetID,
		GUID albedoTexture, GUID metalnessTexture, GUID normalTexture, GUID roughnessTexture, GUID aoTexture, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec3* albedo, float metalness, float roughness, float ao, const glm::vec3* emissive, float opacity, float opacityMask,
		bool bUseAlbedoTexture, bool bUseMetalnessTexture, bool bUseRoughnessTexture, bool bUseAOTexture, bool bUseEmissiveTexture, bool bUseOpacityTexture, bool bUseOpacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, const glm::vec2* tilingFactor, MaterialBlendMode blendMode, bool bDoubleSided,
		Material::TextureChannel metalnessTextureChannel, Material::TextureChannel roughnessTextureChannel, Material::TextureChannel aoTextureChannel,
		Material::TextureChannel opacityTextureChannel, Material::TextureChannel opacityMaskTextureChannel)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set material. Couldn't find an asset");
			return;
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
		{
			Utils::SetMaterial(materialAsset->GetMaterial(),
				albedoTexture, metalnessTexture, normalTexture, roughnessTexture, aoTexture, emissiveTexture, opacityTexture, opacityMaskTexture,
				albedo, metalness, roughness, ao, emissive, opacity, opacityMask,
				bUseAlbedoTexture, bUseMetalnessTexture, bUseRoughnessTexture, bUseAOTexture, bUseEmissiveTexture, bUseOpacityTexture, bUseOpacityMaskTexture,
				tint, emissiveIntensity, tilingFactor, blendMode, bDoubleSided, metalnessTextureChannel, roughnessTextureChannel, aoTextureChannel, opacityTextureChannel, opacityMaskTextureChannel);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set material. It's not a material asset");
	}

	//--------------AssetAudio--------------
	void Script::Eagle_AssetAudio_SetVolume(GUID assetID, float volume)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset volume. Couldn't find an asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			audioAsset->GetAudio()->SetVolume(volume);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset volume. It's not an audio asset");
	}

	float Script::Eagle_AssetAudio_GetVolume(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset volume. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			return audioAsset->GetAudio()->GetVolume();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset volume. It's not an audio asset");
		return 0.f;
	}

	void Script::Eagle_AssetAudio_SetPitch(GUID assetID, float pitch)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pitch. Couldn't find an asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			audioAsset->GetAudio()->SetPitch(pitch);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pitch. It's not an audio asset");
	}

	float Script::Eagle_AssetAudio_GetPitch(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pitch. Couldn't find an asset");
			return 1.f;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			return audioAsset->GetAudio()->GetPitch();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pitch. It's not an audio asset");
		return 1.f;
	}

	void Script::Eagle_AssetAudio_SetPan(GUID assetID, float pan)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pan. Couldn't find an asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			audioAsset->GetAudio()->SetPan(pan);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pan. It's not an audio asset");
	}

	float Script::Eagle_AssetAudio_GetPan(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pan. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			return audioAsset->GetAudio()->GetPan();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pan. It's not an audio asset");
		return 0.f;
	}

	void Script::Eagle_AssetAudio_SetSoundGroup(GUID audioID, GUID soundGroupID)
	{
		Ref<Asset> asset;
		AssetManager::Get(audioID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. Couldn't find an audio asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
		{
			AssetManager::Get(soundGroupID, &asset);
			if (!asset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. Couldn't find a SoundGroup asset");
				return;
			}

			if (Ref<AssetSoundGroup> soundGroupAsset = Cast<AssetSoundGroup>(asset))
				audioAsset->SetSoundGroupAsset(soundGroupAsset);
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. It's not a SoundGroup asset");
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. It's not an audio asset");
	}

	GUID Script::Eagle_AssetAudio_GetSoundGroup(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sound group. Couldn't find an asset");
			return GUID(0, 0);
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
		{
			const auto& soundGroup = audioAsset->GetSoundGroupAsset();
			return soundGroup ? soundGroup->GetGUID() : GUID(0, 0);
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't get sound group. It's not an audio asset");
		return GUID(0, 0);
	}

	//--------------AssetPhysicsMaterial--------------
	void Script::Eagle_AssetPhysicsMaterial_SetDynamicFriction(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset dynamic friction. Couldn't find an asset");
			return;
		}

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			materialAsset->GetMaterial()->SetDynamicFriction(value);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset dynamic friction. It's not a PhysicsMaterial asset");
	}

	void Script::Eagle_AssetPhysicsMaterial_SetBounciness(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset bounciness. Couldn't find an asset");
			return;
		}

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			materialAsset->GetMaterial()->SetBounciness(value);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset bounciness. It's not a PhysicsMaterial asset");
	}

	void Script::Eagle_AssetPhysicsMaterial_SetStaticFriction(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset static friction. Couldn't find an asset");
			return;
		}

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			materialAsset->GetMaterial()->SetStaticFriction(value);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset static friction. It's not a PhysicsMaterial asset");
	}

	float Script::Eagle_AssetPhysicsMaterial_GetStaticFriction(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset static friction. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
			return material->GetMaterial()->GetStaticFriction();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset static friction. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	float Script::Eagle_AssetPhysicsMaterial_GetDynamicFriction(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset dynamic friction. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
			return material->GetMaterial()->GetDynamicFriction();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset dynamic friction. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	float Script::Eagle_AssetPhysicsMaterial_GetBounciness(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset bounciness. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
			return material->GetMaterial()->GetBounciness();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset bounciness. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	GUID Script::Eagle_AssetPhysicsMaterial_Create(float staticFriction, float dynamicFriction, float bounciness)
	{
		Ref<PhysicsMaterial> material = PhysicsMaterial::Create(staticFriction, dynamicFriction, bounciness);
		Ref<AssetPhysicsMaterial> asset = AssetPhysicsMaterial::Create(material);
		AssetManager::AddRuntimeAsset(asset);
		return asset->GetGUID();
	}

	GUID Script::Eagle_AssetPhysicsMaterial_CreateFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create physics material asset. Couldn't find an asset");
			return GUID(0, 0);
		}

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			const auto& material = materialAsset->GetMaterial();
			Ref<PhysicsMaterial> newMaterial = PhysicsMaterial::Create(material->GetStaticFriction(), material->GetDynamicFriction(), material->GetBounciness());
			Ref<AssetPhysicsMaterial> newAsset = AssetPhysicsMaterial::Create(newMaterial);
			AssetManager::AddRuntimeAsset(newAsset);
			return newAsset->GetGUID();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't create physics material asset. It's not a PhysicsMaterial asset");
		return GUID(0, 0);
	}

	//--------------AssetSoundGroup--------------
	void Script::Eagle_AssetSoundGroup_Stop(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop a sound group. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop a sound group. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetPaused(GUID assetID, bool value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set paused. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetPaused(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set paused. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetVolume(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set volume. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetVolume(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set volume. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetMuted(GUID assetID, bool value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set muted. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetMuted(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set muted. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetPitch(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set pitch. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetPitch(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set pitch. It's not a SoundGroup asset");
	}

	float Script::Eagle_AssetSoundGroup_GetVolume(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get volume. Couldn't find a SoundGroup asset");
			return 0.f;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->GetVolume();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get volume. It's not a SoundGroup asset");
		return 0.f;
	}

	float Script::Eagle_AssetSoundGroup_GetPitch(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get pitch. Couldn't find a SoundGroup asset");
			return 0.f;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->GetPitch();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get pitch. It's not a SoundGroup asset");
		return 0.f;
	}

	bool Script::Eagle_AssetSoundGroup_IsPaused(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsPaused`. Couldn't find a SoundGroup asset");
			return false;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->IsPaused();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsPaused`. It's not a SoundGroup asset");
		return false;
	}

	bool Script::Eagle_AssetSoundGroup_IsMuted(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsMuted`. Couldn't find a SoundGroup asset");
			return false;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->IsMuted();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsMuted`. It's not a SoundGroup asset");
		return false;
	}

	//--------------AssetAnimation--------------
	void Script::Eagle_AssetAnimation_SetRootMotionMode(GUID id, RootMotionMode mode)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRootMotionEnabled` for AssetAnimation. Couldn't find an asset");
			return;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRootMotionEnabled` for AssetAnimation. It's not an animation asset");
			return;
		}

		const auto& skeletalInfo = animationAsset->GetSkeletal()->GetMesh()->GetSkeletalMeshInfo();
		if (mode != RootMotionMode::Disabled)
			animationAsset->GetAnimation()->ExtractRootMotion(skeletalInfo, mode);
		else
			animationAsset->GetAnimation()->RemoveRootMotion(skeletalInfo);
	}

	float Script::Eagle_AssetAnimation_GetDuration(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetDuration` for AssetAnimation. Couldn't find an asset");
			return 0.f;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetDuration` for AssetAnimation. It's not an animation asset");
			return 0.f;
		}

		return animationAsset->GetAnimation()->Duration;
	}

	float Script::Eagle_AssetAnimation_GetTicksPerSecond(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetTicksPerSecond` for AssetAnimation. Couldn't find an asset");
			return 0.f;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetTicksPerSecond` for AssetAnimation. It's not an animation asset");
			return 0.f;
		}

		return animationAsset->GetAnimation()->TicksPerSecond;
	}

	void Script::Eagle_AssetAnimation_AddAnimationEvent(GUID id, MonoString* name, float time)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `AddAnimationEvent` for AssetAnimation. Couldn't find an asset");
			return;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `AddAnimationEvent` for AssetAnimation. It's not an animation asset");
			return;
		}

		auto& event = animationAsset->GetAnimation()->Events.emplace_back();
		event.Name = MonoStringHandler(name).c_str();
		event.Time = time;
	}

	bool Script::Eagle_AssetAnimation_RemoveAnimationEvent(GUID id, MonoString* monoName)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveAnimationEvent` for AssetAnimation. Couldn't find an asset");
			return false;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveAnimationEvent` for AssetAnimation. It's not an animation asset");
			return false;
		}

		const std::string name = MonoStringHandler(monoName).c_str();

		auto& events = animationAsset->GetAnimation()->Events;
		auto it = std::find_if(events.begin(), events.end(), [&name](const AnimationEvent& a) { return a.Name == name; });
		if (it != events.end())
		{
			events.erase(it);
			return true;
		}

		return false;
	}

	bool Script::Eagle_AssetAnimation_HasAnimationEvent(GUID id, MonoString* monoName)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `HasAnimationEvent` for AssetAnimation. Couldn't find an asset");
			return false;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `HasAnimationEvent` for AssetAnimation. It's not an animation asset");
			return false;
		}

		const std::string name = MonoStringHandler(monoName).c_str();

		const auto& events = animationAsset->GetAnimation()->Events;
		auto it = std::find_if(events.begin(), events.end(), [&name](const AnimationEvent& a) { return a.Name == name; });
		return it != events.end();
	}

	//--------------AssetParticleSystem--------------
	uint32_t Script::Eagle_AssetParticleSystem_GetEmittersCount(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmittersCount()`. Couldn't find a ParticleSystem asset");
			return 0u;
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
			return uint32_t(ps->GetEmitters().size());

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmittersCount()`. It's not a ParticleSystem asset");
		return 0u;
	}

	void Script::Eagle_AssetParticleSystem_RemoveEmitters(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveEmitters()`. Couldn't find a ParticleSystem asset");
			return;
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
			return ps->SetEmitters({});

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveEmitters()`. It's not a ParticleSystem asset");
		return;
	}

	void* Script::Eagle_AssetParticleSystem_SetEmitters_Prepare(GUID assetID, uint32_t count)
	{
		// Start from the asset's current emitters (matched by index), so whatever scripts can't express
		// (curve shapes, mesh animation settings, emitter IDs) survives a `SetEmitters()` call
		std::vector<ParticleEmitter>* emitters = new std::vector<ParticleEmitter>();
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
			*emitters = ps->GetEmitters();
		emitters->resize(count);
		return emitters;
	}

	// Scripts see over-lifetime properties as start/end values. A curve authored in the editor is kept as long as
	// the script doesn't change its start/end values. Otherwise, it becomes a linear curve (or a constant if both values are equal)
	template <typename T>
	static void SetCurvePropertyFromScript(CurveProperty<T>& property, const T& start, const T& end)
	{
		if (property.Evaluate(0.f) == start && property.Evaluate(1.f) == end)
			return;
		property = CurveProperty<T>::FromStartEnd(start, end);
	}

	void Script::Eagle_AssetParticleSystem_SetEmitters_Finish(GUID assetID, void* data)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			delete data;
			return;
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
		{
			std::vector<ParticleEmitter>* emitters = (std::vector<ParticleEmitter>*)data;
			ps->SetEmitters(std::move(*emitters));
		}

		delete data;
	}

	GUID Script::Eagle_AssetParticleSystem_Create()
	{
		Ref<AssetParticleSystem> asset = AssetParticleSystem::Create();
		AssetManager::AddRuntimeAsset(asset);
		return asset->GetGUID();
	}

	void Script::Eagle_AssetParticleSystem_SetEmitter(void* data, uint32_t index, GUID texture, const glm::vec4* colorStart, const glm::vec4* colorEnd, float colorIntensityStart, float colorIntensityEnd,
		const glm::vec3* velocityMin, const glm::vec3* velocityMax, const glm::vec3* velocityCoefStart, const glm::vec3* velocityCoefEnd, ParticleEmitter::VelocitySpaceType velocitySpace,
		float rotationZStart, float rotationZEnd, const glm::vec2* sizeStart, const glm::vec2* sizeEnd, const glm::vec2* colliderSizeRatio,
		float lifetimeMin, float lifetimeMax, float bouncinessMin, float bouncinessMax, MonoString* name, const Transform* relativeTransform,
		const AABB* visibilityAABB, uint32_t loopCount, float loopDuration, uint32_t spawnRate, float fastForwardTo, float radialAcceleration, float tangentialAcceleration,
		float normalVelocityFactor, ParticleEmitter::EmissionShapeType emissionShape, const glm::vec3* sphereRadius, const glm::vec3* boxMin, const glm::vec3* boxMax,
		const glm::vec3* ringRadius, const glm::vec3* ringThickness, GUID mesh, ParticleEmitter::CollisionModeType collisionMode, const glm::uvec2* animationImagesNum,
		float animationSpeed, bool bDestroyImmediately, bool bEmit, bool bExplode, bool bApplyGravity, bool bAlphaBlending, bool bAdditive, bool bBlendAnimation, bool bFaceDirection)
	{
		std::vector<ParticleEmitter>* emitters = (std::vector<ParticleEmitter>*)data;
		EG_CORE_ASSERT(emitters->size() >= index);

		ParticleEmitter& emitter = (*emitters)[index];
		SetCurvePropertyFromScript(emitter.Color, *colorStart, *colorEnd);
		SetCurvePropertyFromScript(emitter.ColorIntensity, colorIntensityStart, colorIntensityEnd);

		emitter.VelocityMin = *velocityMin;
		emitter.VelocityMax = *velocityMax;
		SetCurvePropertyFromScript(emitter.VelocityCoef, *velocityCoefStart, *velocityCoefEnd);
		emitter.VelocitySpace = velocitySpace;

		SetCurvePropertyFromScript(emitter.RotationZ, rotationZStart, rotationZEnd);
		SetCurvePropertyFromScript(emitter.Size, *sizeStart, *sizeEnd);
		emitter.ColliderSizeRatio = *colliderSizeRatio;

		emitter.LifetimeMin = lifetimeMin;
		emitter.LifetimeMax = lifetimeMax;
		emitter.BouncinessMin = bouncinessMin;
		emitter.BouncinessMax = bouncinessMax;
		emitter.Name = MonoStringHandler(name).c_str();
		emitter.RelativeTransform = *relativeTransform;

		emitter.VisibilityAABB = *visibilityAABB;
		emitter.LoopCount = loopCount;
		emitter.LoopDuration = loopDuration;
		emitter.SpawnRate = spawnRate;
		emitter.FastForwardTo = fastForwardTo;
		emitter.RadialAcceleration = radialAcceleration;
		emitter.TangentialAcceleration = tangentialAcceleration;

		emitter.NormalVelocityFactor = normalVelocityFactor;
		emitter.EmissionShape = emissionShape;
		emitter.SphereRadius = *sphereRadius;
		emitter.BoxMin = *boxMin;
		emitter.BoxMax = *boxMax;

		emitter.RingRadius = *ringRadius;
		emitter.RingThickness = *ringThickness;
		emitter.CollisionMode = collisionMode;
		emitter.AnimationImagesNum = *animationImagesNum;
		
		emitter.AnimationSpeed = animationSpeed;
		emitter.bDestroyImmediately = bDestroyImmediately;
		emitter.bEmit = bEmit;
		emitter.bExplode = bExplode;
		emitter.ApplyGravity = CurveProperty<bool>(bApplyGravity); // Scripts see gravity as a single on/off value
		emitter.bAlphaBlending = bAlphaBlending;
		emitter.bAdditive = bAdditive;
		emitter.bBlendAnimation = bBlendAnimation;
		emitter.bFaceDirection = bFaceDirection;

		if (texture.IsNull())
		{
			emitter.Texture.reset();
		}
		else
		{
			Ref<Asset> asset;
			if (AssetManager::Get(texture, &asset))
			{
				Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
				if (textureAsset)
				{
					emitter.Texture = std::move(textureAsset);
				}
				else
				{
					emitter.Texture.reset();
					EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Texture at index {}. Provided asset is not a Texture2D asset", index);
				}
			}
			else
			{
				emitter.Texture.reset();
				EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Texture at index {}. Couldn't find an asset", index);
			}
		}
		
		if (mesh.IsNull())
		{
			emitter.MeshAsset.reset();
		}
		else
		{
			Ref<Asset> asset;
			if (AssetManager::Get(mesh, &asset))
			{
				Ref<AssetStaticMesh> meshAsset = Cast<AssetStaticMesh>(asset);
				if (meshAsset)
				{
					emitter.MeshAsset = std::move(meshAsset);
				}
				else
				{
					emitter.MeshAsset.reset();
					EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Mesh at index {}. Provided asset is not a Static Mesh asset", index);
				}
			}
			else
			{
				emitter.MeshAsset.reset();
				EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Mesh at index {}. Couldn't find an asset", index);
			}
		}
	}

	MonoString* Script::Eagle_AssetParticleSystem_GetEmitter(GUID assetID, uint32_t index, GUID* texture, glm::vec4* colorStart, glm::vec4* colorEnd, float* colorIntensityStart, float* colorIntensityEnd,
		glm::vec3* velocityMin, glm::vec3* velocityMax, glm::vec3* velocityCoefStart, glm::vec3* velocityCoefEnd, ParticleEmitter::VelocitySpaceType* velocitySpace, float* rotationZStart, float* rotationZEnd,
		glm::vec2* sizeStart, glm::vec2* sizeEnd, glm::vec2* colliderSizeRatio, float* lifetimeMin, float* lifetimeMax, float* bouncinessMin, float* bouncinessMax,
		Transform* relativeTransform, AABB* visibilityAABB, uint32_t* loopCount, float* loopDuration, uint32_t* spawnRate, float* fastForwardTo, float* radialAcceleration,
		float* tangentialAcceleration, float* normalVelocityFactor, ParticleEmitter::EmissionShapeType* emissionShape, glm::vec3* sphereRadius, glm::vec3* boxMin,
		glm::vec3* boxMax, glm::vec3* ringRadius, glm::vec3* ringThickness, GUID* meshAsset, ParticleEmitter::CollisionModeType* collisionMode, glm::uvec2* animationImagesNum,
		float* animationSpeed, bool* bDestroyImmediately, bool* bEmit, bool* bExplode, bool* bApplyGravity, bool* bAlphaBlending, bool* bAdditive, bool* bBlendAnimation, bool* bFaceDirection)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmitter()`. Couldn't find a ParticleSystem asset");
			return mono_string_new(mono_domain_get(), "");
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
		{
			const auto& emitters = ps->GetEmitters();
			if (index >= uint32_t(emitters.size()))
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get Emitter at index {}. Size: {}", index, emitters.size());
				return mono_string_new(mono_domain_get(), "");
			}

			const auto& emitter = emitters[index];
			*texture = emitter.Texture ? emitter.Texture->GetGUID() : GUID(0, 0);
			*colorStart = emitter.Color.Evaluate(0.f);
			*colorEnd = emitter.Color.Evaluate(1.f);
			*colorIntensityStart = emitter.ColorIntensity.Evaluate(0.f);
			*colorIntensityEnd = emitter.ColorIntensity.Evaluate(1.f);

			*velocityMin = emitter.VelocityMin;
			*velocityMax = emitter.VelocityMax;
			*velocityCoefStart = emitter.VelocityCoef.Evaluate(0.f);
			*velocityCoefEnd = emitter.VelocityCoef.Evaluate(1.f);
			*velocitySpace = emitter.VelocitySpace;
			*rotationZStart = emitter.RotationZ.Evaluate(0.f);
			*rotationZEnd = emitter.RotationZ.Evaluate(1.f);

			*sizeStart = emitter.Size.Evaluate(0.f);
			*sizeEnd = emitter.Size.Evaluate(1.f);
			*colliderSizeRatio = emitter.ColliderSizeRatio;
			*lifetimeMin = emitter.LifetimeMin;
			*lifetimeMax = emitter.LifetimeMax;
			*bouncinessMin = emitter.BouncinessMin;
			*bouncinessMax = emitter.BouncinessMax;

			*relativeTransform = emitter.RelativeTransform;
			*visibilityAABB = emitter.VisibilityAABB;
			*loopCount = emitter.LoopCount;
			*loopDuration = emitter.LoopDuration;
			*spawnRate = emitter.SpawnRate;
			*fastForwardTo = emitter.FastForwardTo;
			*radialAcceleration = emitter.RadialAcceleration;

			*tangentialAcceleration = emitter.TangentialAcceleration;
			*normalVelocityFactor = emitter.NormalVelocityFactor;
			*emissionShape = emitter.EmissionShape;
			*sphereRadius = emitter.SphereRadius;
			*boxMin = emitter.BoxMin;

			*boxMax = emitter.BoxMax;
			*ringRadius = emitter.RingRadius;
			*ringThickness = emitter.RingThickness;
			*meshAsset = emitter.MeshAsset ? emitter.MeshAsset->GetGUID() : GUID(0, 0);
			*collisionMode = emitter.CollisionMode;
			*animationImagesNum = emitter.AnimationImagesNum;

			*animationSpeed = emitter.AnimationSpeed;
			*bDestroyImmediately = emitter.bDestroyImmediately;
			*bEmit = emitter.bEmit;
			*bExplode = emitter.bExplode;
			*bApplyGravity = emitter.ApplyGravity.Evaluate(0.f); // At the start of the lifetime
			*bAlphaBlending = emitter.bAlphaBlending;
			*bAdditive = emitter.bAdditive;
			*bBlendAnimation = emitter.bBlendAnimation;
			*bFaceDirection = emitter.bFaceDirection;

			return mono_string_new(mono_domain_get(), emitter.Name.c_str());
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmitter()`. It's not a ParticleSystem asset");
		return mono_string_new(mono_domain_get(), "");
	}

	void Script::Eagle_AssetBehaviorGraph_CreateTaskManager(GUID assetID, MonoObject** outTaskManager)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `CreateTaskManager()`. Couldn't find a AssetBehaviorGraph asset");
			*outTaskManager = nullptr;
			return;
		}

		if (Ref<AssetBehaviorGraph> bg = Cast<AssetBehaviorGraph>(asset))
		{
			Scope<MonoInstance> taskManager = ScriptEngine::InstantiateBehaviorGraph(bg->GetRoot());
			*outTaskManager = taskManager->GetInstance();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `CreateTaskManager()`. It's not an AssetBehaviorGraph asset");
			*outTaskManager = nullptr;
		}
	}

	uint32_t Script::Eagle_ParticleEmitter_GetMaxSpawnRate()
	{
		return ParticleEmitter::MaxSpawnRate;
	}
	
	//--------------Math--------------
	glm::vec3 Script::Eagle_Math_GetForwardVector(const Rotator* rotator)
	{
		return Math::GetForwardVector(*rotator);
	}

	glm::vec3 Script::Eagle_Math_GetUpVector(const Rotator* rotator)
	{
		return Math::GetUpVector(*rotator);
	}

	glm::vec3 Script::Eagle_Math_GetRightVector(const Rotator* rotator)
	{
		return Math::GetRightVector(*rotator);
	}

	float Script::Eagle_Math_CalculateDirection(const glm::vec3* velocity, const Rotator* rotator)
	{
		return Math::CalculateDirection(*velocity, *rotator);
	}

	glm::quat Script::Eagle_Math_SlerpQuat(const glm::quat* x, const glm::quat* y, float alpha)
	{
		return glm::slerp(*x, *y, alpha);
	}

	glm::quat Script::Eagle_Math_LookAt(const glm::vec3* dir)
	{
		constexpr glm::vec3 up = glm::vec3(0, 1, 0);
		constexpr glm::vec3 right = glm::vec3(1, 0, 0);

		// If dir is nearly parallel to up, pick a different up vector
		const glm::vec3 axis = glm::abs(glm::dot(*dir, up)) > 0.999f ? right : up;
		return glm::quatLookAt(*dir, axis);
	}

	glm::quat Script::Eagle_Math_LookAtY(const glm::vec3* dir)
	{

		glm::vec3 dirTemp = *dir;
		dirTemp.y = 0.0f;

		const float len2 = glm::length2(dirTemp);
		// Handle degenerate case: if dir is zero (target is directly above/below)
		if (len2 < 1e-6f)
			return Rotator::Unit().GetQuat();

		dirTemp = dirTemp * glm::inversesqrt(len2); // Normalize

		constexpr glm::vec3 forward(0, 0, -1);
		return glm::rotation(forward, dirTemp);
	}

	glm::quat Script::Eagle_Math_UpAt(const glm::vec3* dir)
	{
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		return glm::rotation(up, *dir);
	}

	glm::vec3 Script::Eagle_Math_GetDirectionToPixel(const glm::vec2* pixelCoord)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& renderer = scene->GetSceneRenderer();
		const glm::vec2 size = renderer->GetViewportSize();

		const glm::vec2 uv = glm::clamp(*pixelCoord / size, glm::vec2(0), glm::vec2(1));
		const glm::vec3 ndc = glm::vec3(uv * 2.f - 1.f, 0.f);
		const CameraComponent* camera = scene->GetRuntimeCamera();

		glm::vec4 worldPos = glm::inverse(camera->GetViewProjection()) * glm::vec4(ndc, 1.f);
		worldPos /= worldPos.w;
		return glm::normalize(glm::vec3(worldPos) - camera->GetWorldTransform().Location);
	}

	glm::quat Script::Eagle_Quat_Mul(const glm::quat& left, const glm::quat& right)
	{
		return left * right;
	}
	
	glm::vec3 Script::Eagle_Quat_EulerAngles(const glm::quat* q)
	{
		return glm::eulerAngles(*q);
	}
	
	glm::quat Script::Eagle_Quat_FromEulerAngles(const glm::vec3* rads)
	{
		return Rotator::FromEulerAngles(*rads).GetQuat();
	}
}
