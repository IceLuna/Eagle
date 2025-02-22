C# Components
=============

Components are objects that can be attached to entities. Think of it as of building blocks, if you want to add a certain functionality to an entity, just attach a component to it.

.. note::

	An entity can only have one component of a given type. Currently, there is no support for user-defines components.
	
`Component`
-----------
It is an abstract base class for all components. It contains a `Parent` member of type `Entity`. It is the entity a component is attached to.

.. code-block:: csharp

    public abstract class Component
    {
        public Entity Parent { get; internal set; }
    }

`Scene Component`
-----------------
It is also an abstract class that inherits `Component`. The difference here is that this class has transforms of a component, meaning a ``Scene Component`` can be placed somewhere on a scene

.. code-block:: csharp

    public abstract class SceneComponent : Component
    {
        public Transform WorldTransform;
        public Vector3 WorldLocation;
        public Rotator WorldRotation;
        public Vector3 WorldScale;

        // Relative to an entity it is attached to
        public Transform RelativeTransform;
        public Vector3 RelativeLocation;
        public Rotator RelativeRotation;
        public Vector3 RelativeScale;

        public Vector3 GetForwardVector();
        public Vector3 GetRightVector();
        public Vector3 GetUpVector();
    }

`Camera Component`
------------------
Use it to add cameras to entities.

One of the most important members of the component is ``IsPrimary``. If it is set to true, the engine will you this camera for rendering.
If multiple cameras have this flag set to true, the engine will pick one of them.

.. code-block:: csharp

    public enum CameraProjectionMode
    {
        Perspective = 0,
        Orthographic = 1
    };

    public class CameraComponent : SceneComponent
    {
        public bool IsPrimary;

        public float PerspectiveVerticalFOV;
        
        // Everything closer won't be rendered
        public float PerspectiveNearClip;
        
        // Everything further won't be rendered
        public float PerspectiveFarClip;
        
        // Beyond this distance from camera, shadows won't be rendered.
        public float ShadowFarClip;
        
        // It is used to determine how to split cascades for directional light shadows.
        public float CascadesSplitAlpha;
        
        // The blend amount between cascades of directional light shadows (if smooth transition is enabled). Try to keep it as low as possible
        public float CascadesSmoothTransitionAlpha;

        public float GetAspectRatio();

        public CameraProjectionMode ProjectionMode;
    }

`Script Component`
------------------
It can be used to determine what script is used by an entity.

Use ``GetScriptType()`` function that returns C# `System.Type` class.
For example, if you have a ``Character`` script which is attached to an entity, you can use a collision callback to check whether an entity is a ``Character`` using the following code:

.. code-block:: csharp

    entity.GetComponent<ScriptComponent>().GetScriptType() == typeof(Character)

The safest way of getting script instance is by calling ``GetInstance()`` and casting it to a script type.

.. code-block:: csharp

    ScriptComponent sc = entity.GetComponent<ScriptComponent>();
    // Check if types match if you're not 100% sure where the entity comes from
    if (sc.GetScriptType() == typeof(Character))
    {
        Character character = sc.GetInstance() as Character;
        ...
    }

In order to set a script to an entity, use ``SetScript`` function.
For example, you can create a ``Projectile`` script class that sets everything up, and when you want to spawn a projectile, run this code:

.. code-block:: csharp

    // Create an entity and set transformation
    Entity entity = Entity.SpawnEntity("Projectile");
    entity.WorldLocation = m_Camera.WorldLocation + m_CameraForward * 0.1f;
    entity.WorldRotation = m_Camera.WorldRotation;
    entity.WorldScale = new Vector3(0.05f);

    // Attach `Projectile` script to it
    ScriptComponent sc = entity.AddComponent<ScriptComponent>();
    sc.SetScript(typeof(Projectile));

    // Get instance of the script and shoot
    Projectile projectile = sc.GetInstance() as Projectile;
    projectile.SetSettings(m_ShootSound, ProjectileColorIntensity, ProjectileSpeed);
    projectile.Shoot(m_CameraForward);

.. note::
    In the editor, a ``Script Component`` exposes public variables from the script so that you can easily change default values without modifying scripts.
    Currently supported types are: ``bool``, ``int``, ``uint``, ``float``, ``string``, ``Vector2``, ``Vector3``, ``Vector4``, ``Color3``, ``Color4``, ``Enum`` (any enums).

.. code-block:: csharp

    public class ScriptComponent : Component
    {
        public void SetScript(Type type);
        public Type GetScriptType();
        public Entity GetInstance();
    }

`Light Component`
-----------------
It is an abstract base class for all light components.

.. code-block:: csharp

    public abstract class LightComponent : SceneComponent
    {
        public Color3 LightColor;
        public float Intensity;
        public float VolumetricFogIntensity;
        public bool bAffectsWorld;
        public bool bCastsShadows;
        public bool bVolumetricLight;
    }

`Point Light Component`
-----------------------
Inherits ``LightComponent``. Additionally, it has a ``Radius`` member.

.. code-block:: csharp

    public class PointLightComponent : LightComponent
    {
        public float Radius;
    }

`Directional Light Component`
-----------------------------
Inherits ``LightComponent``. Additionally, it has an ``Ambient`` member.

.. code-block:: csharp

    public class DirectionalLightComponent : LightComponent
    {
        public Color3 Ambient;
    }

`Spot Light Component`
----------------------
Inherits ``LightComponent``. Additionally, it has the following members: ``InnerCutoffAngle``, ``OuterCutoffAngle``, ``Distance``.

.. code-block:: csharp

    public class SpotLightComponent : LightComponent
    {
        public float InnerCutoffAngle;
        public float OuterCutoffAngle;
        public float Distance;
    }

`Static Mesh Component`
-----------------------
Allows you to add meshes to entities.

.. note::
    If an object has a translucent material, it won't cast shadows unless ``Translucent shadows`` feature is enabled. Translucent materials do not cast shadows on other translucent materials!

.. code-block:: csharp

    public class StaticMeshComponent : SceneComponent
    {
        public AssetStaticMesh MeshAsset;

        public bool bCastsShadows;
        public bool bReceivesDecals;

        public AssetMaterial GetMaterialAsset(uint index);
        public void SetMaterialAsset(uint index, AssetMaterial value);

        public uint GetMaterialsSlotsCount();
    }
    

`Skeletal Mesh Component`
-----------------------
Allows you to add animated meshes to entities.

.. note::
    If an object has a translucent material, it won't cast shadows unless ``Translucent shadows`` feature is enabled. Translucent materials do not cast shadows on other translucent materials!

.. code-block:: csharp

    public enum RootMotionLockFlag
    {
        None = 0,
        PositionX = 1 << 0, PositionY = 1 << 1, PositionZ = 1 << 2,
        Position = PositionX | PositionY | PositionZ
    }

    public enum AnimationType
    {
        Clip,
		Graph
    }

    public class SkeletalMeshComponent : SceneComponent
    {
        public AssetSkeletalMesh MeshAsset;

        public bool bCastsShadows;
        public bool bReceivesDecals;

        public AssetMaterial GetMaterialAsset(uint index);
        public void SetMaterialAsset(AssetMaterial value, uint index);

        public uint GetMaterialsSlotsCount();

        public AssetAnimation GetAnimationAsset();
        public void SetAnimationAsset(AssetAnimation value);
        
        public AssetAnimationGraph GetAnimationGraphAsset();
        public void SetAnimationAssetGraph(AssetAnimationGraph value);

        public void SetRagdollEnabled(bool bEnabled);
        public bool IsRagdollEnabled();

        public Transform GetRagdollBoneWorldTransform(string name);
        
        public Transform GetBoneWorldTransform(string name);
        public Vector3 GetBoneWorldLocation(string name);
        public Rotator GetBoneWorldRotation(string name);
        public Vector3 GetBoneWorldScale(string name);

        public AnimationType AnimType;

        // These are used only if `AnimType` == `AnimationType::Clip`
        public float CurrentClipPlayTime;
        public float ClipPlaybackSpeed;
        public bool bClipLooping;

        // These are used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, bool value);
        public void SetAnimGraphVariable(string name, float value);
        public void SetAnimGraphVariable(string name, AssetAnimation value);
        public void SetAnimGraphVariable(string name, string value);
        public bool GetAnimGraphVariableBool(string name);
        public float GetAnimGraphVariableFloat(string name);
        public AssetAnimation GetAnimGraphVariableAnimation(string name);
        public string GetAnimGraphVariableString(string name);

        public bool IsRootMotionLockFlagSet(RootMotionLockFlag flag);
        public void SetRootMotionLockFlag(RootMotionLockFlag flag, bool value);
        public void SetRootMotionLockFlag(RootMotionLockFlag flag);
        public RootMotionLockFlag GetRootMotionLockFlags();
    }

`Sprite Component`
------------------
Allows you to add sprites to entities and manipulate its material.

.. note::
    If an object has a translucent material, it won't cast shadows unless ``Translucent shadows`` feature is enabled. Translucent materials do not cast shadows on other translucent materials!

.. code-block:: csharp

    public class SpriteComponent : SceneComponent
    {
        public AssetMaterial GetMaterialAsset();
        public void SetMaterialAsset(AssetMaterial value);

        // These are atlas parameters (sprite sheets)
        public Vector2 AtlasSpriteCoords;
        public Vector2 AtlasSpriteSize;
        public Vector2 AtlasSpriteSizeCoef;

        public bool bAtlas;
        public bool bCastsShadows;
        public bool bReceivesDecals;
    }

`Billboard Component`
---------------------
Just a texture that always faces the camera

.. code-block:: csharp

    public class BillboardComponent : SceneComponent
    {
        public AssetTexture2D TextureAsset;
    }

`Text Component`
----------------
Allows you to render 3D Text

.. note::

    If ``bLit`` is set to true, ``TextComponent`` will react to lighting and will be lit correspondingly

.. code-block:: csharp

    public class TextComponent : SceneComponent
    {
        public AssetFont Font;
        public AssetMaterial Material; // Used only if "bLit" is true.

        public string Text;
        public Color3 Color; // Used only if bLit is false. It's an HDR value
        public float LineSpacing;
        public float Kerning;
        public float MaxWidth;
        public bool bCastsShadows;
        public bool bReceivesDecals;
        public bool bLit;
    }

`Text2D Component`
------------------
Allows you to render screen-space 2D Text (useful for in-game UI)

.. code-block:: csharp

    public class Text2DComponent : Component
    {
        public AssetFont Font;
        public string Text;

        public Color3 Color; // HDR
        public float LineSpacing;
        public float Kerning;
        public float MaxWidth;

        // Normalized device coords
        // It is the position of the top left vertex of the first symbol.
        // Text2D will try to be at the same position of the screen no matter the resolution.
        // Also, it'll try to occupy the same amount of space.
        // `(-1; -1)` is the top left corner of the screen; `(0; 0)` is the center; `(1; 1)` is the bottom right corner of the screen.
        public Vector2 Position;

        public Vector2 Scale;
        public float Rotation;

        public float Opacity;
        public bool IsVisible;
    }

`Image2D Component`
-------------------
Allows you to render textures in screen-space (useful for in-game UI)

.. code-block:: csharp

    public class Image2DComponent : Component
    {
        public AssetTexture2D Texture;
        public Color3 Tint; // HDR

        // Same as for `Text2DComponent`
        public Vector2 Position;
        public Vector2 Scale;
        public float Rotation;

        public float Opacity;
        public bool IsVisible;
    }

`Audio Component`
-----------------
Allows you to play 3D audio.

.. code-block:: csharp

    public class AudioComponent : SceneComponent
    {
        // The minimum distance is the point at which the sound starts attenuating.
        // If the listener is any closer to the source than the minimum distance, the sound will play at full volume.
        public void SetMinDistance(float minDistance);

        // The maximum distance is the point at which the sound stops attenuating and its volume remains constant (a volume which is not necessarily zero)
        public void SetMaxDistance(float maxDistance);

        public void SetMinMaxDistance(float minDistance, float maxDistance);
        
        public float GetMinDistance();
        public float GetMaxDistance();
        
        public void SetRollOffModel(RollOffModel rollOff);
        public RollOffModel GetRollOffModel();
        
        public void SetVolume(float volume);
        public float GetVolume();
        
        public void SetPitch(float pitch);
        public float GetPitch();
        
        public void SetPan(float pan);
        public float GetPan();
        
        // @loopCount. `-1` = `Loop Endlessly`; `0` = `Play once`; `1` = `Play twice`, etc...
        public void SetLoopCount(int loopCount);
        public int GetLoopCount();
        
        public void SetLooping(bool bLooping);
        public bool IsLooping();
        
        public void SetMuted(bool bMuted);
        public bool IsMuted();
        
        public void SetAudioAsset(AssetAudio asset);
        public AssetAudio GetAudioAsset();
        
        // When you stream a sound, you can only have one instance of it playing at any time.
        // This limitation exists because there is only one decode buffer per stream.
        // As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks, while most sound effects should be loaded into memory.
        public void SetStreaming(bool bStreaming);
        public bool IsStreaming();
        
        public void Play();
        public void Stop();
        
        public void SetPaused(bool bPaused);
        public bool IsPlaying();

        public void SetDopplerEffectEnabled(bool bEnable);
        public bool IsDopplerEffectEnabled();

        public void SetFFTEnabled(bool bEnable);
        public bool IsFFTEnabled();

        public void SetFFTSamples(uint samples);
        public uint GetFFTSamples();

        public void SetFFTType(FFTWindowType type);
        public FFTWindowType GetFFTType();

		// channelIndex. Allows to get data from a specific audio channel. Starts from 0. If -1, get average over all channels
        public bool GetSpectrumData(ref float[] data, int channelIndex = -1);
        public float GetSampleRate();
        public int GetChannelsCount();

        public void SetPosition(uint ms);
        public uint GetPosition();

        public void SetIs3D(bool b3D);
        public bool Is3D();
    }

`Reverb Component`
------------------
Allows you to apply some effects to 3D sounds.

.. code-block:: csharp

    public class ReverbComponent : SceneComponent
    {
        public bool bActive;
        public ReverbPreset Preset;
        public float MinDistance; // Reverb is at full volume within that radius
        public float MaxDistance; // Reverb is disabled outside that radius
    }

`RigidBody Component`
---------------------
It represents a physics object, and each physics object must have it.

.. note::

    By default, physics bodies are static.
    So, if you want a dynamic object, you must add ``RigidBodyComponent`` first,
    set its body type to dynamic and only after that add any other collider component.
    That is because the body type is read the moment a collider component is initialized. So it can't be changed afterwards.

.. code-block:: csharp

    public enum PhysicsBodyType
    {
        Static = 0, // Immovable objects. They don't react to any forces applied to them.
        Dynamic
    }

    public enum ForceMode
    {
        Force = 0,      // Unit of `mass * distance / time^2`
        Impulse,        // Unit of `mass * distance / time`
        VelocityChange, // Unit of `distance / time`, i.e. the effect is mass independent: a velocity change
        Acceleration    // Unit of `distance / time^2`. It gets treated just like a force except the mass is not divided out before integration
    }

    // Can be used to lock transformations of physics object.
    // You can still change it manually.
    // It is just the physics system that won't be able to change locked transforms during the simulation
    public enum ActorLockFlag
    {
        LocationX = 1 << 0, LocationY = 1 << 1, LocationZ = 1 << 2, Location = LocationX | LocationY | LocationZ,
        RotationX = 1 << 3, RotationY = 1 << 4, RotationZ = 1 << 5, Rotation = RotationX | RotationY | RotationZ
    }

    public class RigidBodyComponent : Component
    {
        public void SetBodyType(PhysicsBodyType bodyType);
        public PhysicsBodyType GetBodyType();

        // If the mass is set to 0, the body will have infinite mass so its linear velocity cannot be changed by any constraints
        public void SetMass(float mass);
        public float GetMass();

        public void SetLinearDamping(float linearDamping);
        public float GetLinearDamping();

        public void SetAngularDamping(float angularDamping);
        public float GetAngularDamping();

        public void SetEnableGravity(bool bEnable);
        public bool IsGravityEnabled();

        // Sometimes controlling an actor using forces or constraints is not sufficiently robust, precise or flexible.
        // For example, moving platforms or character controllers often need to manipulate an actor's position or make it exactly follow a specific path.
        // Such a control scheme is provided by kinematic actors. A kinematic actor is controlled using the `SetKinematicTarget` function.
        // Each simulation step PhysX moves the actor to its target position, regardless of external forces, gravity, collision, etc.
        // Thus, one must continually call `SetKinematicTarget`, every time step, for each kinematic actor, to make them move along their desired paths.
        // The movement of a kinematic actor affects dynamic actors with which it collides or to which it is constrained with a joint.
        // The actor will appear to have infinite mass and will push regular dynamic actors out of the way.
        public void SetIsKinematic(bool bKinematic);
        public bool IsKinematic();

        // The actor will get woken up and might cause other touching actors to wake up as well during the next simulation step.
        public void WakeUp();

        // The actor will stay asleep during the next simulation step if not touched by another non-sleeping actor.
        // Note that when an actor does not move for a period of time, it is no longer simulated in order to save time.
        // This state is called sleeping. The object automatically wakes up when it is either touched by an awake object, or one of its properties is changed by the user
        public void PutToSleep();

        public void AddForce(in Vector3 force, ForceMode forceMode);
        public void AddTorque(in Vector3 torque, ForceMode forceMode);

        public void SetLinearVelocity(in Vector3 velocity);
        public Vector3 GetLinearVelocity();

        public void SetAngularVelocity(in Vector3 velocity);
        public Vector3 GetAngularVelocity();

        public void SetMaxLinearVelocity(float maxVelocity);
        public float GetMaxLinearVelocity();

        public void SetMaxAngularVelocity(float maxVelocity);
        public float GetMaxAngularVelocity();

        public bool IsDynamic();

        public void SetKinematicTarget(Vector3 location, Rotator rotation);
        public void SetKinematicTargetLocation(Vector3 location);
        public void SetKinematicTargetRotation(Rotator rotation);
        public Transform GetKinematicTarget();
        public Vector3 GetKinematicTargetLocation();
        public Rotator GetKinematicTargetRotation();

        public void SetLockFlag(ActorLockFlag flag, bool value); // For example, to lock position, call `SetLockFlag(ActorLockFlag.Location, true)`. Or pass `false` to unlock
        public bool IsLockFlagSet(ActorLockFlag flag);
        public ActorLockFlag GetLockFlags();
    }

`Base Collider Component`
-------------------------
It is an abstract base class for all physics collider components (Inherits ``SceneComponent``).

.. code-block:: csharp

    abstract public class BaseColliderComponent : SceneComponent
    {
        // Its role is to report that there has been an overlap with another shape.
        // Trigger shapes play no part in the simulation of the scene.
        public void SetIsTrigger(bool bTrigger);
        public bool IsTrigger();

        public void SetAffectsNavMeshBuild(bool bAffects);
        public bool DoesAffectNavMeshBuild();

        public void SetIsObstacle(bool bObstacle);
        public bool IsObstacle();
        
        public AssetPhysicsMaterial GetPhysicsMaterial();
        public void SetPhysicsMaterial(AssetPhysicsMaterial material);

        public void SetCollisionVisible(bool bVisible);
        public bool IsCollisionVisible(bool bVisible);
    }

`Box Collider Component`
------------------------
Inherits ``BaseColliderComponent``. Additionally, it has ``SetSize`` and ``GetSize`` functions.

.. code-block:: csharp

    public class BoxColliderComponent : BaseColliderComponent
    {
        public void SetSize(ref Vector3 size);
        public Vector3 GetSize();
    }

`Sphere Collider Component`
---------------------------
Inherits ``BaseColliderComponent``. Additionally, it has ``SetRadius`` and ``GetRadius`` functions.

.. code-block:: csharp

    public class SphereColliderComponent : BaseColliderComponent
    {
        public void SetRadius(float radius);
        public float GetRadius();
    }

`Capsule Collider Component`
----------------------------
Inherits ``BaseColliderComponent``. Additionally, it has ``SetRadius``, ``GetRadius``, ``SetHeight``, and ``GetHeight``  functions.

.. code-block:: csharp

    public class CapsuleColliderComponent : BaseColliderComponent
    {
        public void SetRadius(float radius);
        public float GetRadius();
        public void SetHeight(float height);
        public float GetHeight();
    }

`Mesh Collider Component`
-------------------------
Allows you to select a static mesh to create a collider from. Inherits ``BaseColliderComponent``.

.. code-block:: csharp

    public class MeshColliderComponent : BaseColliderComponent
    {
        public void SetCollisionMesh(AssetStaticMesh mesh);
        public AssetStaticMesh GetCollisionMesh();

        // Collider will be created using a rough approximation of the mesh.
        // Non-convex mesh collider can be used only with kinematic or static actors.
        public void SetIsConvex(bool bConvex);
        public bool IsConvex();

        // Only affects non-convex mesh colliders. Non-convex meshes are one-sided meaning collision won't be registered from the back side. For example, that might be a problem for windows.
        // So to fix this problem, you can set this flag to true
        public void SetIsTwoSided(bool bConvex);
        public bool IsTwoSided();
    }

`Particle System Component`
---------------------------
Allows you to spawn and control particle systems.

.. code-block:: csharp

    public class ParticleSystemComponent : SceneComponent
    {
        public void SetAsset(AssetParticleSystem ps);
        public AssetParticleSystem GetAsset();
    }
    
`Decal Component`
-----------------
Allows you to spawn decals.

.. code-block:: csharp

    public class DecalComponent : SceneComponent
    {
        public AssetMaterial MaterialAsset;
        public uint SortPriority;
        public bool bAdjustAspectRatio;
    }

`Navigation Mesh Component`
---------------------------
Allows you to spawn decals.

.. code-block:: csharp

    public class NavigationMeshComponent : SceneComponent
    {
        public bool bAutoRebuild; // Rebuilds on changes if set to true

        public void SetSettings(NavMeshSettings settings);
        public NavMeshSettings GetSettings();

        public void SetCrowdSettings(CrowdSettings settings);
        public CrowdSettings GetCrowdSettings();

        // Currently, only one nav mesh is supported.
        // Building it will invalidate existing nav mesh.
        public void Build();
    }


`Navigation Crowd Agent Component`
----------------------------------
Allows you to mark entities as agents which can be controlled by the :ref:`crowd navigation system <csharp_navigation_guide>`.

.. code-block:: csharp

    public class NavigationCrowdAgentComponent : Component
    {
        // Agents are controlled by the crowd system. But if teleportation is required,
        // this function can be used. It'll recreate an agent at a new location
        public void TeleportAgent(Vector3 location);

        public void SetMoveTarget(Vector3 location);
        public void ResetMoveTarget();

        public bool IsValid();

        // Entity.WorldLocation should be the same because agents control entities.
        public bool GetLocation(out Vector3 outLocation);
        public bool GetVelocity(out Vector3 outVelocity);

        public void SetSettings(AgentSettings settings);
        public AgentSettings GetSettings();

        public AgentTargetState GetTargetState();
    }
