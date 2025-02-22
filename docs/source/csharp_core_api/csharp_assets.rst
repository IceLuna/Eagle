.. _csharp_assets_guide:

C# Assets
=========

This API represents all engine asset types.
All asset classes are derived from ``Asset`` class.

.. note::
    Changing assets affects the whole asset, meaning it will affect the editor and other objects that use it.
    Material, Physics Material, and Particle System assets allow you to create new copies so that you are able to generate unique assets at runtime.

.. code-block:: csharp

    public enum FilterMode
    {
        Point,
        Bilinear,
        Trilinear
    };

    public enum AddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        ClampToOpaqueBlack,
        ClampToOpaqueWhite
    };

    public enum AssetType
    {
        None,
		Texture2D,
		TextureCube,
		StaticMesh,
		SkeletalMesh,
		Audio,
		SoundGroup,
		Font,
		Material,
		PhysicsMaterial,
        Entity,
        Scene,
        Animation,
        AnimationGraph,
        ParticleSystem,
    };

    public enum AssetTexture2DFormat
    {
        RGBA8,
		RG8,
		R8,

		Default = RGBA8
    };

    public enum AssetTextureCubeFormat
    {
        RGBA32,
		RGBA16,
		R11G11B10,

		Default = R11G11B10
    };

    public class Asset
    {
        public string GetPath();
        public AssetType GetAssetType();
        public GUID GetGUID();

        // Can be used to get assets by path.
        // But ideally you'd create public variable of Asset type and assign a value to it using an editor.
        public static Asset Get(string path);
    }

    public class AssetTexture2D : Asset
    {
        public void SetAnisotropy(float anisotropy);
        public void SetFilterMode(FilterMode filterMode);
        public void SetAddressMode(AddressMode addressMode);
        public void SetMipsCount(uint mips);
        public void SetFormat(AssetTexture2DFormat format);
        public void SetIsNormalMap(bool bNormalMap);
        public void SetNeedsAlpha(bool bNeedsAlpha);
        public void SetIsCompressed(bool bCompressed);

        public float GetAnisotropy();
        public FilterMode GetFilterMode();
        public AddressMode GetAddressMode();
        public uint GetMipsCount();
        public AssetTexture2DFormat GetFormat();
        public bool IsNormalMap();
        public bool DoesNeedAlpha();
        public bool IsCompressed();
    }

    public class AssetTextureCube : Asset
    {
        public void SetLayerSize(uint layerSize);
        public void SetPrefilterSize(uint prefilter);
        public bool SetFormat(AssetTextureCubeFormat format);

        public uint GetLayerSize();
        public uint GetPrefilterSize();
        public AssetTextureCubeFormat GetFormat();
    }

    public class AssetStaticMesh : Asset
    {
    }

    public class AssetSkeletalMesh : Asset
    {
    }

    public class AssetMaterial : Asset
    {
        public static AssetMaterial Create();
        public static AssetMaterial Create(Material value);
        public static AssetMaterial Create(AssetMaterial material);

        public Material GetMaterial();
        public void SetMaterial(Material value);
    }

    public class AssetAudio : Asset
    {
        public void SetVolume(float volume);
        public float GetVolume();

        public void SetPitch(float pitch);
        public float GetPitch();

        public void SetPan(float pan);
        public float GetPan();

        public void SetSoundGroup(AssetSoundGroup soundGroup);
        public AssetSoundGroup GetSoundGroup();
    }

    public class AssetPhysicsMaterial : Asset
    {
        public static AssetPhysicsMaterial Create(float staticFriction = 0.6f, float dynamicFriction = 0.6f, float bounciness = 0.5f);
        public static AssetPhysicsMaterial Create(AssetPhysicsMaterial material);

        public float GetStaticFriction();
        public float GetDynamicFriction();
        public float GetBounciness();

        public void SetStaticFriction(float value);
        public void SetDynamicFriction(float value);
        public void SetBounciness(float value);
    }

    public class AssetFont : Asset
    {
    }

    public class AssetSoundGroup : Asset
    {
        public void Stop();

        public void SetPaused(bool bPaused);
        public void SetVolume(float volume);
        public void SetMuted(bool bMuted);

        //@ Pitch. Any value between 0 and 10
        public void SetPitch(float pitch);

        public float GetVolume();
        public float GetPitch();
        public bool IsPaused();
        public bool IsMuted();
    }

    public class AssetEntity: Asset
    {
    }

    public class AssetScene: Asset
    {
    }

    public class AssetAnimation: Asset
    {
        public void SetRootMotionEnabled(bool bEnabled);

        // Duration in ticks
        public float GetDuration();
        public float GetTicksPerSecond();

        // Note: there can be multiple events with the same name
        // @name. When the event is triggered, C# `Entity.OnAnimationEvent()` is called with this as a parameter.
        // @time. A value between [0; Duration] when an event should be triggered.
        public void AddAnimationEvent(string name, float time);

        // Returns true if event was removed
        public bool RemoveAnimationEvent(string name);

        public bool HasAnimationEvent(string name);
    }

    public class AssetAnimationGraph: Asset
    {
    }

    public class AssetParticleSystem : Asset
    {
        public static AssetParticleSystem Create();
        public static AssetParticleSystem Create(AssetParticleSystem asset);

        public uint GetEmittersCount();

        public ParticleEmitter[] GetEmitters();
        public void SetEmitters(ParticleEmitter[] emitters);
    }
