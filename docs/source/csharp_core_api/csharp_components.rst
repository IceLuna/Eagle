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

.. note::
    In the editor, a ``Script Component`` exposes public variables from the script so that you can easily change default values without modifying scripts.
    Currently supported types are: ``bool``, ``int``, ``uint``, ``float``, ``string``, ``Vector2``, ``Vector3``, ``Vector4``, ``Color3``, ``Color4``, ``Enum`` (any enums).

.. code-block:: csharp

    public class ScriptComponent : Component
    {
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

`StaticMesh Component`
-----------------------
Allows you to add meshes to entities and manipulate its material.

.. note::
    If an object has a translucent material, it won't cast shadows unless ``Translucent shadows`` feature is enabled. Translucent materials do not cast shadows on other translucent materials!

.. code-block:: csharp

    public class StaticMeshComponent : SceneComponent
    {
        public StaticMesh Mesh;

        public bool bCastsShadows;

        public Material GetMaterial();
        public void SetMaterial(Material value);
    }

`Sprite Component`
------------------
Allows you to add sprites to entities and manipulate its material.

.. note::
    If an object has a translucent material, it won't cast shadows unless ``Translucent shadows`` feature is enabled. Translucent materials do not cast shadows on other translucent materials!

.. code-block:: csharp

    public class SpriteComponent : SceneComponent
    {
        public Material GetMaterial();
        void SetMaterial(Material value);
    
        public bool bCastsShadows;
    
        // These are atlas parameters (sprite sheets)
        public Vector2 AtlasSpriteCoords;
        public Vector2 AtlasSpriteSize;
        public Vector2 AtlasSpriteSizeCoef;
        public bool bAtlas;
    }

`Billboard Component`
---------------------
Just a texture that always faces the camera

.. code-block:: csharp

    public class BillboardComponent : SceneComponent
    {
        public Texture2D Texture;
    }

`Text Component`
----------------
Allows you to render 3D Text

.. note::

    If ``bLit`` is set to true, ``TextComponent`` will react to lighting and will be lit correspondingly

.. code-block:: csharp

    public class TextComponent : SceneComponent
    {
        public string Text;

        public MaterialBlendMode BlendMode;
        public bool bCastsShadows;

        public Color3 Color; // Used only if bLit is false. It's an HDR value
        public float LineSpacing;
        public float Kerning;
        public float MaxWidth;

        // Values below are used only if bLit is true
        public bool bLit;
        public Color3 Albedo;
        public Color3 Emissive;
        public float Metallness;
        public float Roughness;
        public float AmbientOcclusion;
        public float Opacity; // To use it, `BlendMode` must be set to `Translucent`
        public float OpacityMask; // To use it, `BlendMode` must be set to `Masked`
    }

`Text2D Component`
------------------
Allows you to render screen-space 2D Text (useful for in-game UI)

.. code-block:: csharp

    public class Text2DComponent : Component
    {
        public string Text;

        public Color3 Color; // HDR
        public float LineSpacing;
        public float Kerning;
        public float MaxWidth;

        // Normalized device coords
        // It is the position of the bottom left vertex of the first symbol.
        // Text2D will try to be at the same position of the screen no matter the resolution.
        // Also, it'll try to occupy the same amount of space.
        // `(-1; -1)` is the bottom left corner of the screen; `(0; 0)` is the center; `(1; 1)` is the top right corner of the screen.
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
        public Texture2D Texture;
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
        public void SetSound(in string filepath);

        public void Play();
        public void Stop();
        public void SetPaused(bool bPaused);
        public bool IsPlaying();
        
        // The minimum distance is the point at which the sound starts attenuating.
        // If the listener is any closer to the source than the minimum distance, the sound will play at full volume.
        public void SetMinDistance(float minDistance);
        public float GetMinDistance();

        // The maximum distance is the point at which the sound stops attenuating and its volume remains constant (a volume which is not necessarily zero)
        public void SetMaxDistance(float maxDistance);
        public float GetMaxDistance();

        public void SetMinMaxDistance(float minDistance, float maxDistance);
        
        public void SetRollOffModel(RollOffModel rollOff);
        public RollOffModel GetRollOffModel();
        
        public void SetVolume(float volume);
        public float GetVolume();
        
        // @loopCount. `-1` = `Loop Endlessly`; `0` = `Play once`; `1` = `Play twice`, etc...
        public void SetLoopCount(int loopCount);
        public int GetLoopCount();
        
        public void SetLooping(bool bLooping);
        public bool IsLooping();
        
        public void SetMuted(bool bMuted);
        public bool IsMuted();
        
        // When you stream a sound, you can only have one instance of it playing at any time.
        // This limitation exists because there is only one decode buffer per stream.
        // As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks, while most sound effects should be loaded into memory.
        public void SetStreaming(bool bStreaming);
        public bool IsStreaming();

        public void SetDopplerEffectEnabled(bool bEnable);
        public bool IsDopplerEffectEnabled();
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
        
        // Static friction defines the amount of friction that is applied between surfaces that are not moving lateral to each-other.
        public void SetStaticFriction(float staticFriction);
        public float GetStaticFriction();
        
        // Dynamic friction defines the amount of friction applied between surfaces that are moving relative to each-other.
        public void SetDynamicFriction(float dynamicFriction);
        public float GetDynamicFriction();
        
        public void SetBounciness(float bounciness);
        public float GetBounciness();
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
        public void SetCollisionMesh(StaticMesh mesh);
        public StaticMesh GetCollisionMesh();

        // Collider will be created using a rough approximation of the mesh.
        // Non-convex mesh collider can be used only with kinematic or static actors.
        public void SetIsConvex(bool bConvex);
        public bool IsConvex();

        // Only affects non-convex mesh colliders. Non-convex meshes are one-sided meaning collision won't be registered from the back side. For example, that might be a problem for windows.
        // So to fix this problem, you can set this flag to true
        public void SetIsTwoSided(bool bConvex);
        public bool IsTwoSided();
    }
