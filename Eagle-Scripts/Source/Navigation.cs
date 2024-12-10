using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    public enum AgentObstacleAvoidanceQuality
    {
        Low, Medium, Good, High
    };

    public enum AgentTargetState
    {
        None = 0,
        Failed,
        Valid,
        Requesting,
        WaitingForQueue,
        WaitingForPath,
        Velocity,
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct AgentSettings
    {
        public float AgentRadius;
        public float AgentHeight;
        public float MaxAcceleration;
        public float MaxSpeed;
        public float SeparationWeight;
        public AgentObstacleAvoidanceQuality ObstacleAvoidanceQuality;
        public bool bAnticipateTurns;
        public bool bOptimizeVis;
        public bool bOptimizeTopo;
        public bool bSeparation;
    };

    public class Navigation
    {
        // Returns true if success
        public static bool FindDistanceToWall(Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance)
        {
            return FindDistanceToWall_Native(ref pos, maxRadius, out outHitPos, out outHitNormal, out outHitDistance);
        }

        // Returns true if success
        public static bool FindRandomPoint(out Vector3 outRandomPoint)
        {
            return FindRandomPoint_Native(out outRandomPoint);
        }

        // Returns true if success
        public static bool FindRandomPointInCircle(Vector3 pos, float radius, out Vector3 outRandomPoint)
        {
            return FindRandomPointInCircle_Native(ref pos, radius, out outRandomPoint);
        }

        public static bool IsValidPoint(Vector3 pos)
        {
            return IsValidPoint_Native(ref pos);
        }

        public static Vector3[] FindStraightPath(Vector3 start, Vector3 end, uint maxPolys = 256)
        {
            return FindStraightPath_Native(ref start, ref end, maxPolys);
        }

        public static Vector3[] FindSmoothPath(Vector3 start, Vector3 end, uint maxPolys = 256, uint maxSmooth = 2048)
        {
            return FindSmoothPath_Native(ref start, ref end, maxPolys, maxSmooth);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindDistanceToWall_Native(ref Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindRandomPoint_Native(out Vector3 outRandomPoint);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindRandomPointInCircle_Native(ref Vector3 pos, float radius, out Vector3 outRandomPoint);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValidPoint_Native(ref Vector3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3[] FindStraightPath_Native(ref Vector3 start, ref Vector3 end, uint maxPolys);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3[] FindSmoothPath_Native(ref Vector3 start, ref Vector3 end, uint maxPolys, uint maxSmooth);
    }

    public class CrowdNavigation
    {
        public static void SetMoveTarget(Vector3 pos)
        {
            SetMoveTarget_Native(ref pos);
        }

        public static void ResetMoveTarget()
        {
            ResetMoveTarget_Native();
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMoveTarget_Native(ref Vector3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ResetMoveTarget_Native();
    }
}
