#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngineRegistry.h"
#include "ScriptUserComponents.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Physics/PhysicsActor.h"

#include <mono/jit/jit.h>

// Note: keep last
#include "ScriptWrappersInternal.h"

namespace Eagle
{
	//--------------Sound--------------
	void Script::Eagle_Sound_SetSettings(GUID id, const SoundSettings* settings)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			sound->SetVolumeMultiplier(settings->VolumeMultiplier);
			sound->SetPitch(settings->Pitch);
			sound->SetPan(settings->Pan);
			sound->SetLoopCount(settings->LoopCount);
			sound->SetFFTSamples(settings->FFTSamples);
			sound->SetFFTType(settings->FFTType);
			sound->SetLooping(settings->IsLooping);
			sound->SetStreaming(settings->IsStreaming);
			sound->SetMuted(settings->IsMuted);
			sound->SetFFTEnabled(settings->bEnableFFT);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound settings. Sound is not found");
	}

	void Script::Eagle_Sound_GetSettings(GUID id, SoundSettings* outSettings)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			*outSettings = sound->GetSettings();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sound settings. Sound is not found");
	}

	void Script::Eagle_Sound_Play(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play sound. Sound is not found");
	}

	void Script::Eagle_Sound_Stop(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop sound. Sound is not found");
	}

	void Script::Eagle_Sound_SetPaused(GUID id, bool bPaused)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPaused`. Sound is not found");
	}

	bool Script::Eagle_Sound_IsPlaying(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->IsPlaying();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsPlaying`. Sound is not found");
		return false;
	}

	void Script::Eagle_Sound_SetPosition(GUID id, uint32_t ms)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetPosition(ms);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPosition`. Sound is not found");
	}

	uint32_t Script::Eagle_Sound_GetPosition(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetPosition();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPosition`. Sound is not found");
		return 0u;
	}

	void Script::Eagle_Sound_SetFFTEnabled(GUID id, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetFFTEnabled(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFFTEnabled`. Sound is not found");
	}

	bool Script::Eagle_Sound_IsFFTEnabled(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->IsFFTEnabled();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsFFTEnabled`. Sound is not found");
		return false;
	}

	void Script::Eagle_Sound_SetFFTSamples(GUID id, uint32_t value)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetFFTSamples(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFFTSamples`. Sound is not found");
	}

	uint32_t Script::Eagle_Sound_GetFFTSamples(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetFFTSamples();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFFTSamples`. Sound is not found");
		return 0u;
	}

	void Script::Eagle_Sound_SetFFTType(GUID id, FFTWindowType value)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetFFTType(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFFTType`. Sound is not found");
	}

	FFTWindowType Script::Eagle_Sound_GetFFTType(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetFFTType();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFFTType`. Sound is not found");
		return FFTWindowType::Rect;
	}

	bool Script::Eagle_Sound_GetSpectrumData(GUID id, MonoArray* data, int channelIndex)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			const uint32_t monoLength = (uint32_t)mono_array_length(data);
			void* sampleData = mono_array_addr(data, float, 0);
			return sound->GetSpectrumData((float*)sampleData, monoLength, channelIndex);
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSpectrumData`. Sound is not found");
		return false;
	}

	float Script::Eagle_Sound_GetSampleRate(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			return sound->GetSampleRate();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSampleRate`. Sound is not found");
		return 0.f;
	}

	float Script::Eagle_Sound_GetSpectrumSampleRate()
	{
		// Same for every sound: spectrum data is computed at the FMOD mixer rate
		return AudioEngine::GetSpectrumSampleRate();
	}

	int Script::Eagle_Sound_GetChannelsCount(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			return sound->GetChannelsCount();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetChannelsCount`. Sound is not found");
		return 0;
	}

	//--------------Sound2D--------------
	GUID Script::Eagle_Sound2D_Create(GUID assetID, const SoundSettings* settings)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Couldn't find an asset");
			return GUID(0, 0);
		}

		Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset);
		if (!audioAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Provided asset is not an audio asset");
			return GUID(0, 0);
		}

		auto& scene = Scene::GetCurrentScene();
		return scene->SpawnSound2D(audioAsset, *settings).ID;
	}

	//--------------Sound3D--------------
	GUID Script::Eagle_Sound3D_Create(GUID assetID, const glm::vec3* position, RollOffModel rolloff, const SoundSettings* settings)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Couldn't find an asset");
			return GUID(0, 0);
		}

		Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset);
		if (!audioAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Provided asset is not an audio asset");
			return GUID(0, 0);
		}

		auto& scene = Scene::GetCurrentScene();
		return scene->SpawnSound3D(audioAsset, *position, rolloff, *settings).ID;
	}

	void Script::Eagle_Sound3D_SetMinDistance(GUID id, float min)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMinDistance(min);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMinDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetMaxDistance(GUID id, float max)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMaxDistance(max);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMaxDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetMinMaxDistance(GUID id, float min, float max)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMinMaxDistance(min, max);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMinMaxDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetWorldPosition(GUID id, const glm::vec3* position)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetWorldPosition(*position);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetWorldPosition`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetVelocity(GUID id, const glm::vec3* velocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetVelocity(*velocity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVelocity`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetRollOffModel(GUID id, RollOffModel rollOff)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRollOffModel`. Sound is not found");
	}

	float Script::Eagle_Sound3D_GetMinDistance(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetMinDistance();
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMinDistance`. Sound is not found");
		return 0.f;
	}

	float Script::Eagle_Sound3D_GetMaxDistance(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetMaxDistance();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMaxDistance`. Sound is not found");
		return 0.f;
	}

	void Script::Eagle_Sound3D_GetWorldPosition(GUID id, glm::vec3* outPosition)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			*outPosition = sound->GetWorldPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetWorldPosition`. Sound is not found");
	}

	void Script::Eagle_Sound3D_GetVelocity(GUID id, glm::vec3* outVelocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			*outVelocity = sound->GetVelocity();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetVelocity`. Sound is not found");
	}

	RollOffModel Script::Eagle_Sound3D_GetRollOffModel(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetRollOffModel();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetRollOffModel`. Sound is not found");
		return RollOffModel::Default;
	}

	//--------------AudioComponent--------------
	void Script::Eagle_AudioComponent_SetMinDistance(GUID entityID, float minDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinDistance(minDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMaxDistance(GUID entityID, float maxDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMaxDistance(maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's max distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMinMaxDistance(GUID entityID, float minDistance, float maxDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinMaxDistance(minDistance, maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min-max distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetRollOffModel(GUID entityID, RollOffModel rollOff)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's roll off model. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetVolume(GUID entityID, float volume)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetVolume(volume);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's volume. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetPitch(GUID entityID, float pitch)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetPitch(pitch);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's pitch. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetLoopCount(GUID entityID, int loopCount)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLoopCount(loopCount);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's loop count. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetLooping(GUID entityID, bool bLooping)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLooping(bLooping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's looping mode. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMuted(GUID entityID, bool bMuted)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMuted(bMuted);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetMuted' function. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetAudioAsset(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio asset of AudioComponent. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<AudioComponent>();
		if (assetID.IsNull())
		{
			component.SetAudioAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio asset of AudioComponent. Couldn't find an asset");
			return;
		}

		Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset);
		if (!audioAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio asset of AudioComponent. Provided asset is not an audio 2D asset");
			return;
		}

		component.SetAudioAsset(audioAsset);
	}

	GUID Script::Eagle_AudioComponent_GetAudioAsset(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio asset of AudioComponent. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<AudioComponent>();
		const auto& asset = component.GetAudioAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_AudioComponent_SetStreaming(GUID entityID, bool bStreaming)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetStreaming(bStreaming);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's streaming mode. Entity is null");
	}

	void Script::Eagle_AudioComponent_Play(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play audio component's sound. Entity is null");
	}

	void Script::Eagle_AudioComponent_Stop(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop audio component's sound. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetPaused(GUID entityID, bool bPaused)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPaused' function. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetDopplerEffectEnabled(GUID entityID, bool bEnable)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().bEnableDopplerEffect = bEnable;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetDopplerEffectEnabled' function. Entity is null");
	}

	float Script::Eagle_AudioComponent_GetMinDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetMinDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's min distance. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_AudioComponent_GetMaxDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetMaxDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's max distance. Entity is null");
			return 0.f;
		}
	}

	RollOffModel Script::Eagle_AudioComponent_GetRollOffModel(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetRollOffModel();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's roll off model. Entity is null");
			return RollOffModel::Default;
		}
	}

	float Script::Eagle_AudioComponent_GetVolume(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetVolume();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's volume. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_AudioComponent_GetPitch(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetPitch();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's pitch. Entity is null");
			return 1.f;
		}
	}

	int Script::Eagle_AudioComponent_GetLoopCount(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetLoopCount();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's loop count. Entity is null");
			return 0;
		}
	}

	bool Script::Eagle_AudioComponent_IsLooping(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsLooping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's looping mode. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsMuted(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsMuted();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsMuted' function. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsStreaming(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsStreaming();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsStreaming' function. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsPlaying(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsPlaying();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsPlaying' function. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsDopplerEffectEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().bEnableDopplerEffect;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsDopplerEffectEnabled' function. Entity is null");
			return false;
		}
	}

	void Script::Eagle_AudioComponent_SetFFTEnabled(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetFFTEnabled(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetFFTEnabled' function. Entity is null");
	}

	bool Script::Eagle_AudioComponent_IsFFTEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsFFTEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsFFTEnabled' function. Entity is null");
			return false;
		}
	}

	void Script::Eagle_AudioComponent_SetFFTSamples(GUID entityID, uint32_t value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetFFTSamples(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetFFTSamples' function. Entity is null");
	}

	uint32_t Script::Eagle_AudioComponent_GetFFTSamples(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetFFTSamples();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetFFTSamples' function. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_AudioComponent_SetFFTType(GUID entityID, FFTWindowType value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetFFTType(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetFFTType' function. Entity is null");
	}

	FFTWindowType Script::Eagle_AudioComponent_GetFFTType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetFFTType();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetFFTType' function. Entity is null");
			return FFTWindowType::Rect;
		}
	}

	bool Script::Eagle_AudioComponent_GetSpectrumData(GUID entityID, MonoArray* data, int channelIndex)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const uint32_t monoLength = (uint32_t)mono_array_length(data);
			void* sampleData = mono_array_addr(data, float, 0);
			return entity.GetComponent<AudioComponent>().GetSpectrumData((float*)sampleData, monoLength, channelIndex);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetSpectrumData' function. Entity is null");
			return false;
		}
	}

	float Script::Eagle_AudioComponent_GetSampleRate(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetSampleRate();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetSampleRate' function. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_AudioComponent_SetPosition(GUID entityID, uint32_t ms)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().SetPosition(ms);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPosition' function. Entity is null");
		}
	}

	uint32_t Script::Eagle_AudioComponent_GetPosition(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetPosition();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetPosition' function. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_AudioComponent_SetIs3D(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().SetIs3D(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetIs3D' function. Entity is null");
		}
	}

	bool Script::Eagle_AudioComponent_Is3D(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().Is3D();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'Is3D' function. Entity is null");
			return false;
		}
	}

	void Script::Eagle_AudioComponent_SetPan(GUID entityID, float pan)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().SetPan(pan);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPan' function. Entity is null");
		}
	}

	float Script::Eagle_AudioComponent_GetPan(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetPan();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetPan' function. Entity is null");
			return 0.f;
		}
	}

	int Script::Eagle_AudioComponent_GetChannelsCount(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetChannelsCount();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetChannelsCount' function. Entity is null");
			return 0;
		}
	}

	//--------------RigidBodyComponent--------------
	void Script::Eagle_RigidBodyComponent_SetBodyType(GUID entityID, PhysicsBodyType type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetBodyType(type);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics body type. Entity is null");
	}

	PhysicsBodyType Script::Eagle_RigidBodyComponent_GetBodyType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetBodyType();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get physics body type. Entity is null");
			return PhysicsBodyType::Static;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetCollisionDetectionType(GUID entityID, CollisionDetectionType type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetCollisionDetectionType(type);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision detection type. Entity is null");
	}

	CollisionDetectionType Script::Eagle_RigidBodyComponent_GetCollisionDetectionType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetCollisionDetectionType();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get collision detection type. Entity is null");
			return CollisionDetectionType::Discrete;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetPositionSolverIterations(GUID entityID, uint32_t iterations)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetPositionSolverIterations(iterations);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set position solver iterations. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetVelocitySolverIterations(GUID entityID, uint32_t iterations)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetVelocitySolverIterations(iterations);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set velocity solver iterations. Entity is null");
	}

	uint32_t Script::Eagle_RigidBodyComponent_GetPositionSolverIterations(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetPositionSolverIterations();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get position solver iterations. Entity is null");
			return 0u;
		}
	}

	uint32_t Script::Eagle_RigidBodyComponent_GetVelocitySolverIterations(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetVelocitySolverIterations();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get velocity solver iterations. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMass(GUID entityID, float mass)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetMass(mass);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mass. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetMass(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetMass();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mass. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLinearDamping(GUID entityID, float linearDamping)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLinearDamping(linearDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear damping. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetLinearDamping(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetLinearDamping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get linear damping. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetAngularDamping(GUID entityID, float angularDamping)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetAngularDamping(angularDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular damping. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetAngularDamping(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetAngularDamping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get angular damping. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetEnableGravity(GUID entityID, bool bEnable)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetEnableGravity(bEnable);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetEnableGravity`. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsGravityEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsGravityEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsGravityEnabled`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetIsKinematic(GUID entityID, bool bKinematic)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetIsKinematic(bKinematic);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsKinematic`. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsKinematic(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsKinematic();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsKinematic`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_RigidBodyComponent_WakeUp(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->WakeUp();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_PutToSleep(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->PutToSleep();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_AddForce(GUID entityID, const glm::vec3* force, ForceMode forceMode)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddForce(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_AddForceAtLocation(GUID entityID, const glm::vec3* location, const glm::vec3* force, ForceMode forceMode)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddForceAtLocation(*location, *force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_AddTorque(GUID entityID, const glm::vec3* force, ForceMode forceMode)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddTorque(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetLinearVelocity(GUID entityID, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*result = physicsActor->GetLinearVelocity();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLinearVelocity(GUID entityID, const glm::vec3* velocity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetLinearVelocity(*velocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set linear velocity of Entity. Entity is not a physics actor");
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetAngularVelocity(GUID entityID, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*result = physicsActor->GetAngularVelocity();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetAngularVelocity(GUID entityID, const glm::vec3* velocity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetAngularVelocity(*velocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_RigidBodyComponent_GetMaxLinearVelocity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().GetMaxLinearVelocity();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max linear velocity of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMaxLinearVelocity(GUID entityID, float maxVelocity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<RigidBodyComponent>().SetMaxLinearVelocity(maxVelocity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max linear velocity of Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_RigidBodyComponent_GetMaxAngularVelocity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().GetMaxAngularVelocity();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max angular velocity of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMaxAngularVelocity(GUID entityID, float maxVelocity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<RigidBodyComponent>().SetMaxAngularVelocity(maxVelocity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max angular velocity of Entity. Entity is null");
			return;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsDynamic(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->IsDynamic();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDynamic`. It's not a physics actor: {}", entity.GetName());
				return false;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDynamic` of Entity. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsLockFlagSet(GUID entityID, ActorLockFlag flag)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().IsLockFlagSet(flag);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsLockFlagSet`. Entity is null");
			return false;
		}
	}

	ActorLockFlag Script::Eagle_RigidBodyComponent_GetLockFlags(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().GetLockFlags();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLockFlags`. Entity is null");
			return ActorLockFlag();
		}
	}

	void Script::Eagle_RigidBodyComponent_GetKinematicTarget(GUID entityID, Transform* outTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*outTransform = physicsActor->GetKinematicTarget();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTarget` of Entity. It's is not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTarget` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetKinematicTargetLocation(GUID entityID, glm::vec3* outLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*outLocation = physicsActor->GetKinematicTargetLocation();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetLocation` of Entity. It's is not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetLocation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetKinematicTargetRotation(GUID entityID, Rotator* outRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*outRotation = physicsActor->GetKinematicTargetRotation();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetRotation` of Entity. It's is not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetRotation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetKinematicTarget(GUID entityID, const glm::vec3* location, const Rotator* rotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematicTarget(*location, *rotation);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTarget` of Entity. It's is not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTarget` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetKinematicTargetLocation(GUID entityID, const glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematicTargetLocation(*location);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetLocation` of Entity. It's is not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetLocation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetKinematicTargetRotation(GUID entityID, const Rotator* rotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematicTargetRotation(*rotation);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetRotation` of Entity. It's is not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetRotation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLockFlag(GUID entityID, ActorLockFlag flag, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<RigidBodyComponent>().SetLockFlag(flag, value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetLockFlag`. Entity is null");
			return;
		}
	}

	CharacterControllerCollisionFlags Script::Eagle_CharacterControllerComponent_Move(GUID id, const glm::vec3* disp, float minDist, float elapsedTime)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().Move(*disp, minDist, elapsedTime);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Move`. Entity is null");
			return CharacterControllerCollisionFlags::None;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetSlopeLimit(GUID id, float degrees)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetSlopeLimit(degrees);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetSlopeLimit`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetSlopeLimit(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetSlopeLimit();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSlopeLimit`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetContactOffset(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetContactOffset(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetContactOffset`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetContactOffset(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetContactOffset();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetContactOffset`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetStepOffset(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetStepOffset(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetStepOffset`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetStepOffset(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetStepOffset();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetStepOffset`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetPhysicsMaterialAsset(GUID id, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material asset of CharacterControllerComponent. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<CharacterControllerComponent>();
		if (assetID.IsNull())
		{
			component.SetPhysicsMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material asset of CharacterControllerComponent. Couldn't find an asset");
			return;
		}

		Ref<AssetPhysicsMaterial> physicsMaterialAsset = Cast<AssetPhysicsMaterial>(asset);
		if (!physicsMaterialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material asset of CharacterControllerComponent. Provided asset is not a physics material asset");
			return;
		}

		component.SetPhysicsMaterialAsset(physicsMaterialAsset);
	}

	GUID Script::Eagle_CharacterControllerComponent_GetPhysicsMaterialAsset(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get physics material asset of CharacterControllerComponent. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<CharacterControllerComponent>();
		const auto& asset = component.GetPhysicsMaterialAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_CharacterControllerComponent_SetShapeType(GUID id, CharacterControllerShape value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetShapeType(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetShapeType`. Entity is null");
		}
	}

	CharacterControllerShape Script::Eagle_CharacterControllerComponent_GetShapeType(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetShapeType();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetShapeType`. Entity is null");
			return CharacterControllerShape::Box;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCapsuleClimbingMode(GUID id, CapsuleClimbingMode value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCapsuleClimbingMode(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCapsuleClimbingMode`. Entity is null");
		}
	}

	CapsuleClimbingMode Script::Eagle_CharacterControllerComponent_GetCapsuleClimbingMode(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCapsuleClimbingMode();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCapsuleClimbingMode`. Entity is null");
			return CapsuleClimbingMode::Easy;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCapsuleRadius(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCapsuleRadius(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCapsuleRadius`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetCapsuleRadius(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCapsuleRadius();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCapsuleRadius`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCapsuleHeight(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCapsuleHeight(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCapsuleHeight`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetCapsuleHeight(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCapsuleHeight();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCapsuleHeight`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetBoxSize(GUID id, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetBoxSize(*value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetBoxSize`. Entity is null");
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetBoxSize(GUID id, glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*value = entity.GetComponent<CharacterControllerComponent>().GetBoxSize();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetBoxSize`. Entity is null");
			*value = glm::vec3(0);
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetControllerWorldLocation(GUID id, glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*value = entity.GetComponent<CharacterControllerComponent>().GetControllerWorldLocation();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetControllerWorldLocation`. Entity is null");
			*value = glm::vec3(0);
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetControllerFootWorldLocation(GUID id, glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*value = entity.GetComponent<CharacterControllerComponent>().GetControllerFootWorldLocation();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetControllerFootWorldLocation`. Entity is null");
			*value = glm::vec3(0);
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetShowCollision(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetShowCollision(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetShowCollision`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_IsCollisionVisible(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().IsCollisionVisible();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsCollisionVisible`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCollisionGroup(GUID id, CollisionGroup value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCollisionGroup(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCollisionGroup`. Entity is null");
		}
	}

	CollisionGroup Script::Eagle_CharacterControllerComponent_GetCollisionGroup(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCollisionGroup();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCollisionGroup`. Entity is null");
			return CollisionGroup::Object;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetInteractingCollisionGroup(GUID id, CollisionGroup value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetInteractingCollisionGroup(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetInteractingCollisionGroup`. Entity is null");
		}
	}

	CollisionGroup Script::Eagle_CharacterControllerComponent_GetInteractingCollisionGroup(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetInteractingCollisionGroup();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetInteractingCollisionGroup`. Entity is null");
			return CollisionGroup::Object;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetControllerUpDirection(GUID id, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetControllerUpDirection(*value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetControllerUpDirection`. Entity is null");
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetControllerUpDirection(GUID id, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*result = entity.GetComponent<CharacterControllerComponent>().GetControllerUpDirection();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetControllerUpDirection`. Entity is null");
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetDoesCollideWithOtherControllers(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetDoesCollideWithOtherControllers(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetDoesCollideWithOtherControllers`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_DoesCollideWithOtherControllers(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().DoesCollideWithOtherControllers();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesCollideWithOtherControllers`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetMoveWholeEntity(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().bMoveWholeEntity = value;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bMoveWholeEntity`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_GetMoveWholeEntity(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().bMoveWholeEntity;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMoveWholeEntity`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetUseFootLocation(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().bUseFootLocation = value;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bUseFootLocation`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_GetUseFootLocation(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().bUseFootLocation;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetUseFootLocation`. Entity is null");
			return false;
		}
	}

	//--------------BaseColliderComponent--------------
	void Script::Eagle_BaseColliderComponent_SetCollisionGroup(GUID entityID, void* type, CollisionGroup groups)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetCollisionGroup(groups);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCollisionGroup'. Entity is null");
	}

	CollisionGroup Script::Eagle_BaseColliderComponent_GetCollisionGroup(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->GetCollisionGroup();
			return CollisionGroup{};
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetCollisionGroup'. Entity is null");
			return s_DefaultCollisionGroup;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetInteractingCollisionGroup(GUID entityID, void* type, CollisionGroup groups)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetInteractingCollisionGroup(groups);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetInteractingCollisionGroup'. Entity is null");
	}

	CollisionGroup Script::Eagle_BaseColliderComponent_GetInteractingCollisionGroup(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->GetInteractingCollisionGroup();
			return CollisionGroup{};
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetInteractingCollisionGroup'. Entity is null");
			return s_DefaultInteractingCollisionGroup;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetIsTrigger(GUID entityID, void* type, bool bTrigger)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetIsTrigger(bTrigger);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTrigger'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsTrigger(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->IsTrigger();
			return false;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsTrigger'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetCollisionEnabled(GUID entityID, void* type, bool bEnabled)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetCollisionEnabled(bEnabled);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCollisionEnabled'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsCollisionEnabled(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->IsCollisionEnabled();
			return false;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsCollisionEnabled'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetCollisionVisible(GUID entityID, void* type, bool bShow)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetShowCollision(bShow);
			return;
		}
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCollisionVisible'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsCollisionVisible(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->IsCollisionVisible();
			return false;
		}
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsCollisionVisible'. Entity is null");
		return false;
	}

	GUID Script::Eagle_BaseColliderComponent_GetPhysicsMaterial(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
			{
				const auto& asset = comp->GetPhysicsMaterialAsset();
				return asset ? asset->GetGUID() : GUID(0, 0);
			}
			return GUID(0, 0);
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't get PhysicsMaterial. Entity is null");
		return GUID(0, 0);
	}

	void Script::Eagle_BaseColliderComponent_SetPhysicsMaterial(GUID entityID, void* type, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material. Entity is null");
			return;
		}

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (assetID.IsNull())
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetPhysicsMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material. Couldn't find an asset");
			return;
		}

		Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset);
		if (!material)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material. Provided asset is not a PhysicsMaterial asset");
			return;
		}

		if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
			comp->SetPhysicsMaterialAsset(material);
	}

	void Script::Eagle_BaseColliderComponent_SetAffectsNavMeshBuild(GUID entityID, void* type, bool bAffects)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetAffectsNavMeshBuild(bAffects);
			return;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAffectsNavMeshBuild'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_DoesAffectNavMeshBuild(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->DoesAffectNavMeshBuild();
			return false;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesAffectNavMeshBuild'. Entity is null");
		return false;
	}

	void Script::Eagle_BaseColliderComponent_SetIsObstacle(GUID entityID, void* type, bool bObstacle)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				comp->SetIsObstacle(bObstacle);
			return;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsObstacle'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsObstacle(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (BaseColliderComponent* comp = ScriptEngineRegistry::GetColliderComponent(entity, monoType))
				return comp->IsObstacle();
			return false;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsObstacle'. Entity is null");
		return false;
	}

	//--------------BoxColliderComponent--------------
	void Script::Eagle_BoxColliderComponent_SetSize(GUID entityID, const glm::vec3* size)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<BoxColliderComponent>().SetSize(*size);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set size of BoxCollider. Entity is null");
	}

	void Script::Eagle_BoxColliderComponent_GetSize(GUID entityID, glm::vec3* outSize)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			*outSize = entity.GetComponent<BoxColliderComponent>().GetSize();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get size of BoxCollider. Entity is null");
			*outSize = glm::vec3{0.f};
		}
	}

	//--------------SphereColliderComponent--------------
	void Script::Eagle_SphereColliderComponent_SetRadius(GUID entityID, float val)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<SphereColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of SphereCollider. Entity is null");
	}

	float Script::Eagle_SphereColliderComponent_GetRadius(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<SphereColliderComponent>().GetRadius();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get radius of SphereCollider. Entity is null");
			return 0.f;
		}
	}

	//--------------CapsuleColliderComponent--------------
	void Script::Eagle_CapsuleColliderComponent_SetRadius(GUID entityID, float val)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of CapsuleCollider. Entity is null");
	}

	float Script::Eagle_CapsuleColliderComponent_GetRadius(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CapsuleColliderComponent>().GetRadius();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get radius of CapsuleCollider. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CapsuleColliderComponent_SetHeight(GUID entityID, float val)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetHeight(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set height of CapsuleCollider. Entity is null");
	}

	float Script::Eagle_CapsuleColliderComponent_GetHeight(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CapsuleColliderComponent>().GetHeight();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get height of CapsuleCollider. Entity is null");
			return 0.f;
		}
	}

	//--------------MeshColliderComponent--------------
	void Script::Eagle_MeshColliderComponent_SetIsConvex(GUID entityID, bool val)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsConvex(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsConvex'. Entity is null");
	}

	bool Script::Eagle_MeshColliderComponent_IsConvex(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<MeshColliderComponent>().IsConvex();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsConvex'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_MeshColliderComponent_SetIsTwoSided(GUID entityID, bool val)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsTwoSided(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTwoSided'. Entity is null");
	}

	bool Script::Eagle_MeshColliderComponent_IsTwoSided(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<MeshColliderComponent>().IsTwoSided();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsTwoSided'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_MeshColliderComponent_SetCollisionMesh(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<MeshColliderComponent>();
		if (assetID.IsNull())
		{
			component.SetCollisionMeshAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Couldn't find an asset");
			return;
		}

		Ref<AssetStaticMesh> meshAsset = Cast<AssetStaticMesh>(asset);
		if (!meshAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Provided asset is not a mesh asset");
			return;
		}

		component.SetCollisionMeshAsset(meshAsset);
	}

	GUID Script::Eagle_MeshColliderComponent_GetCollisionMesh(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get collision mesh. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<MeshColliderComponent>();
		const auto& asset = component.GetCollisionMeshAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}
}
