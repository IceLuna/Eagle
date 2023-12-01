C# Scene
========
``Scene`` class allows you open other scenes, draw lines, and ray-cast physics.

.. code-block:: csharp

    public struct RaycastHit
    {
        public Entity HitEntity;
        public Vector3 Position;
        public float Distance;
        public Vector3 Normal;
    }

    public struct CollisionInfo
    {
        public Vector3 Position;
        public Vector3 Normal;
        public Vector3 Impulse;
        public Vector3 Force;
    }

    public struct RendererLine
    {
        public Color3 Color;
        public Vector3 StartPos;
        public Vector3 EndPos;
    }

    public class Scene
    {
        // Path to a scene
        public static void OpenScene(string path);

        // @ dir. Direction of a ray. Must be normalized.
        // It returns true if there was a hit. In that case you can use `outHit` to retrieve hit data.
        public static bool Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit outHit);

        public static void DrawLine(RendererLine line);
    }
