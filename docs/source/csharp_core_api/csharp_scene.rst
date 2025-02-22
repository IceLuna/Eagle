C# Scene
========
``Scene`` class allows you open other scenes, quit game, change gravity, get entities, and ray-cast physics.

.. code-block:: csharp

    public struct RaycastHit
    {
        public Entity HitEntity;
        public Vector3 Position;
        public float Distance;
        public Vector3 Normal;
    }

    public class Scene
    {
        public static void OpenScene(AssetScene scene);

        public static void QuitGame();

        // @ dir. Direction of a ray. Must be normalized.
        // It returns true if there was a hit. In that case you can use `outHit` to retrieve hit data.
        public static bool Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit outHit);

        public static void SetGravity(Vector3 gravity);
        public static Vector3 GetGravity();

        public static Entity[] GetAllEntitiesWithComponent<T>() where T : Component;
    }
