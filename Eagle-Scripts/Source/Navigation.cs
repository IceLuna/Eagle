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
        [UIName("Waiting for queue")] WaitingForQueue,
        [UIName("Waiting for path")] WaitingForPath,
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

    [StructLayout(LayoutKind.Sequential)]
    public struct CrowdSettings
    {
        public uint MaxAgents;
        public float MaxAgentRadius;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct NavMeshSettings
    {
        public AABB Bounds;
        public uint MaxQueryNodes;
        public uint ExpectedLayersPerTile;
        public uint MaxLayers;
        public uint MaxObstacles;
        public uint TileSize;
        public float CellSize;
        public float CellHeight;
        public float MaxSlope;
        public float AgentHeight;
        public float AgentMaxClimb;
        public float AgentRadius;
        public float EdgeMaxError;
        public uint BorderSize;
        public bool FilterLowHangingObstacles;
        public bool FilterLedgeSpans;
        public bool FilterWalkableLowHeightSpans;
    };

    // State for `Navigation.MoveToTarget`. Used by crowd agents to keep their move request.
    // Requesting a new target every frame throws away Detour's path search, and the agent never arrives.
    public class MoveToTargetState
    {
        public void Reset()
        {
            bHasRequest = false;
        }

        internal bool bHasRequest = false;
        internal Vector3 RequestedTarget = new Vector3(0f);
    }

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

        // Needs to be called every frame. Returns true when finished.
        // - An entity with a valid `NavigationCrowdAgentComponent` is moved by it: its move target is set, and `moveSpeed` becomes the agent's Max Speed.
        //   Pass a `MoveToTargetState` to follow a moving target. Without one, a new target is only requested when the agent has none.
        // - An entity with a dynamic (non-kinematic) rigid body is moved by setting its horizontal velocity, so that physics keeps it on the ground.
        // - Other entities are teleported along the path.
        // Call `StopMoving` if you stop calling this before it returns true.
        public static bool MoveToTarget(Entity entity, Vector3 targetLocation, float acceptanceDistance, float ts, float moveSpeed, float rotationSpeed)
        {
            return MoveToTarget(entity, targetLocation, acceptanceDistance, ts, moveSpeed, rotationSpeed, null);
        }

        public static bool MoveToTarget(Entity entity, Vector3 targetLocation, float acceptanceDistance, float ts, float moveSpeed, float rotationSpeed, MoveToTargetState state)
        {
            NavigationCrowdAgentComponent agent = GetCrowdAgent(entity);
            if (agent != null)
                return MoveCrowdAgentToTarget(entity, agent, targetLocation, acceptanceDistance, ts, moveSpeed, rotationSpeed, state);

            Vector3 location = entity.WorldLocation;

            if (Mathf.Length2(targetLocation - location) <= (acceptanceDistance * acceptanceDistance))
            {
                StopMoving(entity);
                return true;
            }

            Vector3[] path = Navigation.FindSmoothPath(location, targetLocation);
            if (path.Length == 0)
            {
                StopMoving(entity);
                return true;
            }

            // Path heights are approximate. The nav mesh has no detail mesh, so on slopes its surface can be far above or below the real ground.
            // So path points are compared and followed on XZ only.
            Vector3 target = path[0];
            if (HorizontalLength2(target - location) < 0.01f && path.Length > 1)
            {
                target = path[1];
            }

            Vector3 horizontalDir = target - location;
            horizontalDir.Y = 0f;
            float horizontalLength = Mathf.Length(horizontalDir);
            if (horizontalLength < 0.0001f)
            {
                // At the end of the path, but not at the target (it's unreachable, or above/below). Wait here
                StopMoving(entity);
                return false;
            }
            horizontalDir /= horizontalLength;

            RigidBodyComponent rigidBody = GetDynamicRigidBody(entity);
            if (rigidBody != null)
            {
                // Teleporting a dynamic body fights the physics (and can push it into thin colliders). Its vertical velocity is left to physics
                Vector3 velocity = rigidBody.GetLinearVelocity();
                rigidBody.SetLinearVelocity(new Vector3(horizontalDir.X * moveSpeed, velocity.Y, horizontalDir.Z * moveSpeed));
            }
            else
            {
                // No physics to keep it on the ground, so it follows the path's heights
                Vector3 dir = Mathf.Normalize(target - location);
                entity.WorldLocation = location + dir * (moveSpeed * ts);
            }
            entity.WorldRotation = Mathf.Slerp(entity.WorldRotation, Mathf.LookAtY(horizontalDir), ts * rotationSpeed);

            return false;
        }

        // Stops the movement started by `MoveToTarget`.
        // Resets a crowd agent's move target, or the horizontal velocity of a dynamic rigid body.
        public static void StopMoving(Entity entity)
        {
            NavigationCrowdAgentComponent agent = GetCrowdAgent(entity);
            if (agent != null)
            {
                agent.ResetMoveTarget();
                return;
            }

            RigidBodyComponent rigidBody = GetDynamicRigidBody(entity);
            if (rigidBody != null)
            {
                Vector3 velocity = rigidBody.GetLinearVelocity();
                rigidBody.SetLinearVelocity(new Vector3(0f, velocity.Y, 0f));
            }
        }

        private static bool MoveCrowdAgentToTarget(Entity entity, NavigationCrowdAgentComponent agent, Vector3 targetLocation, float acceptanceDistance, float ts, float moveSpeed, float rotationSpeed, MoveToTargetState state)
        {
            // A new move target is requested when the target moves more than this since the last request
            const float CrowdTargetUpdateDistance = 0.25f;

            AgentSettings settings = agent.GetSettings();
            if (!agent.GetLocation(out Vector3 location))
                location = entity.WorldLocation;

            // Arrival is checked on XZ, and within the agent's height vertically
            Vector3 toTarget = targetLocation - location;
            if (HorizontalLength2(toTarget) <= (acceptanceDistance * acceptanceDistance) && Math.Abs(toTarget.Y) <= settings.AgentHeight)
            {
                agent.ResetMoveTarget();
                state?.Reset();
                return true;
            }

            if (settings.MaxSpeed != moveSpeed)
            {
                settings.MaxSpeed = moveSpeed;
                agent.SetSettings(settings);
            }

            AgentTargetState targetState = agent.GetTargetState();
            bool bHasTarget = targetState != AgentTargetState.None && targetState != AgentTargetState.Failed && targetState != AgentTargetState.Velocity;
            bool bRequest = !bHasTarget;
            if (state != null)
                bRequest |= !state.bHasRequest || Mathf.Length2(targetLocation - state.RequestedTarget) > (CrowdTargetUpdateDistance * CrowdTargetUpdateDistance);

            if (bRequest)
            {
                agent.SetMoveTarget(targetLocation);
                if (state != null)
                {
                    state.bHasRequest = true;
                    state.RequestedTarget = targetLocation;
                }
            }

            // The crowd moves the entity, but doesn't rotate it. Face the direction of movement
            if (agent.GetVelocity(out Vector3 velocity))
            {
                velocity.Y = 0f;
                float speed = Mathf.Length(velocity);
                if (speed > 0.01f)
                    entity.WorldRotation = Mathf.Slerp(entity.WorldRotation, Mathf.LookAtY(velocity / speed), ts * rotationSpeed);
            }

            return false;
        }

        private static NavigationCrowdAgentComponent GetCrowdAgent(Entity entity)
        {
            if (entity == null || !entity.HasComponent<NavigationCrowdAgentComponent>())
                return null;

            NavigationCrowdAgentComponent agent = entity.GetComponent<NavigationCrowdAgentComponent>();
            return agent.IsValid() ? agent : null;
        }

        private static RigidBodyComponent GetDynamicRigidBody(Entity entity)
        {
            if (entity == null || !entity.HasComponent<RigidBodyComponent>())
                return null;

            RigidBodyComponent rigidBody = entity.GetComponent<RigidBodyComponent>();
            if (rigidBody.GetBodyType() != PhysicsBodyType.Dynamic || rigidBody.IsKinematic())
                return null;

            return rigidBody;
        }

        private static float HorizontalLength2(Vector3 v)
        {
            return v.X * v.X + v.Z * v.Z;
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
        // Controls all existing agents
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
