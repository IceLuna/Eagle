.. _csharp_entity_guide:

C# Entity
=========

``Entity`` class is one of the most important classes in the C# API.
Using this class, you can write your scripts and do all sorts of things to in-game objects.

In order to write scripts for game objects, you need to create a class and derive it from ``Entity``.
``Entity`` class allows you to override some important functions:

- ``OnCreate``. It is called when an entity is spawned.
- ``OnDestroy``. It is called when an entity is destroyed.
- ``OnUpdate``. It is called on each frame and you can use its first parameter ``float timestamp`` which depends on the FPS.
- ``OnPhysicsUpdate``. It is called each time the physics system is updated. It has the same arguments as ``OnUpdate`` function but the ``timestamp`` value is always the same and it does not depend on the FPS. Currently, ``timestamp`` is always `0.00833` ms (120 FPS).
- ``OnEvent``. It is called each time an event is triggered. It receives an ``Event`` object as its first parameter.

``Entity`` class:

.. code-block:: csharp

    public class Entity
    {
        public GUID ID { get; private set; }

        public virtual void OnCreate() { }
        public virtual void OnDestroy() { }
        public virtual void OnUpdate(float ts) { }
        public virtual void OnPhysicsUpdate(float ts) { }
        public virtual void OnEvent(Event e) { }

        public Entity Parent;
        public Entity[] Children;

        public Transform WorldTransform;
        public Vector3 WorldLocation;
        public Rotator WorldRotation;
        public Vector3 WorldScale;

        // Relative to a `Parent` entity if it is present
        public Transform RelativeTransform;
        public Vector3 RelativeLocation;
        public Rotator RelativeRotation;
        public Vector3 RelativeScale;

        public T AddComponent<T>() where T : Component, new();
        public bool HasComponent<T>() where T : Component, new();
        public bool HasComponent(Type type);
        public T GetComponent<T>() where T : Component, new();

        public GUID GetID() { return ID; }

        public Vector3 GetForwardVector();
        public Vector3 GetRightVector();
        public Vector3 GetUpVector();

        // Even though `OnDestroy` function is triggered immediately,
        // an entity and its data are deleted from memory at the beginning of the next frame
        public void Destroy();

        // Checks if a mouse is hovered over an entity
        // It checks if an entity would be hovered if a mouse was at the given coordinates.
        public bool IsMouseHovered() { return IsMouseHovered_Native(ID); }

        // Checks if a mouse is hovered over an entity
        // It checks if an entity would be hovered if a mouse was at the given coordinates.
        // @ mouseCoord. Mouse coord within a viewport
        public bool IsMouseHovered(ref Vector2 mouseCoord);

        // Use this function to be notified when an entity begins colliding with another one.
        // `Entity` object is passed to callbacks so that you have information with which entity collision happened.
        public void AddCollisionBeginCallback(Action<Entity> callback);

        public void RemoveCollisionBeginCallback(Action<Entity> callback);

        // Same as for `AddCollisionBeginCallback` but this one is triggered when a collision ends
        public void AddCollisionEndCallback(Action<Entity> callback);

        public void RemoveCollisionEndCallback(Action<Entity> callback);

        // Same as for `AddCollisionBeginCallback`
        public void AddTriggerBeginCallback(Action<Entity> callback);

        public void RemoveTriggerBeginCallback(Action<Entity> callback);

        // Same as for `AddCollisionEndCallback`
        public void AddTriggerEndCallback(Action<Entity> callback);

        public void RemoveTriggerEndCallback(Action<Entity> callback);

        // Returns the name of an entity
        public override string ToString();

        public Entity GetChildrenByName(string name);

        // Use this function to spawn new entities in a scene
        static public Entity SpawnEntity(string name = "");
    }
