using System;
using System.Collections.Generic;

namespace Eagle
{
    public class AITask : AINode
    { }

    [UIName("Look at Entity")]
    public class AITaskLookAtEntity : AITask
    {
        [Tooltip("Entity that should be moved")]
        public string Owner;

        [UIName("Target Entity")]
        [Tooltip("Entity to look at")]
        public string TargetEntity;

        [UIName("Rotation speed")]
        public string RotationSpeed;

        public float Epsilon = 0.050f;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_Blackboard.TryGetValue(TargetEntity, out m_TargetEntity);
            m_RotationSpeed = m_Blackboard.GetValue(RotationSpeed, DefaultRotationSpeed);
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null || m_TargetEntity == null)
                return AINodeStatus.Failed;

            Vector3 location = m_Owner.GetWorldLocation();
            Vector3 target = m_TargetEntity.GetWorldLocation();

            // Only turns around Y
            Vector3 dir = target - location;
            dir.Y = 0f;
            float length = Mathf.Length(dir);
            if (length < 0.0001f)
                return AINodeStatus.Succeeded;

            Rotator targetRot = Mathf.LookAtY(dir / length);
            Rotator result = Mathf.Slerp(m_Owner.GetWorldRotation(), targetRot, Mathf.Clamp(ts * m_RotationSpeed, 0f, 1f));
            m_Owner.SetWorldRotation(result, PhysicsMove.Move);

            // `q` and `-q` are the same rotation, and Slerp can approach either of them
            Quat negated = new Quat(-result.Rotation.W, -result.Rotation.X, -result.Rotation.Y, -result.Rotation.Z);
            bool bFacing = targetRot.Rotation.Equals(result.Rotation, Epsilon) || targetRot.Rotation.Equals(negated, Epsilon);
            return bFacing ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        private Entity m_TargetEntity = null;
        private Entity m_Owner = null;
        private float m_RotationSpeed = DefaultRotationSpeed;

        private const float DefaultRotationSpeed = 3.5f;
    }

    [UIName("Go to Point")]
    public class AITaskGoToPoint : AITask
    {
        [Tooltip("Entity that should be moved")]
        public string Owner;

        [UIName("Point to go to")]
        public string Target;

        [UIName("Move speed")]
        public string MoveSpeed;

        [UIName("Rotation speed")]
        [Tooltip("Ignored for crowd agents with `Face Movement Direction` on (their `Turn Rate` is used)")]
        public string RotationSpeed;

        [UIName("Acceptance Distance")]
        public float AcceptanceDistance = 0.5f;

        [UIName("Fallback Agent Type")]
        [Tooltip("Nav mesh to move on if the owner doesn't have a Navigation Crowd Agent Component (crowd agents always move on the nav mesh of their agent).\nAuto - the default agent type")]
        public NavAgentType FallbackAgentType;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_bHasTarget = m_Blackboard.TryGetValue(Target, out m_Target);
            m_MoveSpeed = m_Blackboard.GetValue(MoveSpeed, DefaultMoveSpeed);
            m_RotationSpeed = m_Blackboard.GetValue(RotationSpeed, DefaultRotationSpeed);
            m_MoveState.Reset();
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null || !m_bHasTarget)
                return AINodeStatus.Failed;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_Target, AcceptanceDistance, ts, m_MoveSpeed, m_RotationSpeed, m_MoveState, FallbackAgentType);
            if (!bFinished)
                return AINodeStatus.Running;

            return m_MoveState.Result == MoveToTargetResult.NoPath ? AINodeStatus.Failed : AINodeStatus.Succeeded;
        }

        public override void OnEnd(AINodeStatus status)
        {
            base.OnEnd(status);

            if (m_Owner != null)
                Navigation.StopMoving(m_Owner);
            m_MoveState.Reset();
        }

        protected Vector3 m_Target = new Vector3(0);
        protected bool m_bHasTarget = false;
        private MoveToTargetState m_MoveState = new MoveToTargetState();
        private Entity m_Owner = null;
        private float m_MoveSpeed = DefaultMoveSpeed;
        private float m_RotationSpeed = DefaultRotationSpeed;

        private const float DefaultMoveSpeed = 1f;
        private const float DefaultRotationSpeed = 3.5f;
    }

    [UIName("Go to Entity")]
    public class AITaskGoToEntity : AITask
    {
        [Tooltip("Entity that should be moved")]
        public string Owner;

        [UIName("Target Entity")]
        [Tooltip("Entity to go to")]
        public string TargetEntity;

        [UIName("Move speed")]
        public string MoveSpeed;

        [UIName("Rotation speed")]
        [Tooltip("Ignored for crowd agents with `Face Movement Direction` on (their `Turn Rate` is used)")]
        public string RotationSpeed;

        [UIName("Acceptance Distance")]
        public float AcceptanceDistance = 0.5f;

        [UIName("Fallback Agent Type")]
        [Tooltip("Nav mesh to move on if the owner doesn't have a Navigation Crowd Agent Component (crowd agents always move on the nav mesh of their agent).\nAuto - the default agent type")]
        public NavAgentType FallbackAgentType;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_Blackboard.TryGetValue(TargetEntity, out m_TargetEntity);
            m_MoveSpeed = m_Blackboard.GetValue(MoveSpeed, DefaultMoveSpeed);
            m_RotationSpeed = m_Blackboard.GetValue(RotationSpeed, DefaultRotationSpeed);
            m_MoveState.Reset();
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null || m_TargetEntity == null)
                return AINodeStatus.Failed;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_TargetEntity.GetWorldLocation(), AcceptanceDistance, ts, m_MoveSpeed, m_RotationSpeed, m_MoveState, FallbackAgentType);
            if (!bFinished)
                return AINodeStatus.Running;

            return m_MoveState.Result == MoveToTargetResult.NoPath ? AINodeStatus.Failed : AINodeStatus.Succeeded;
        }

        public override void OnEnd(AINodeStatus status)
        {
            base.OnEnd(status);

            if (m_Owner != null)
                Navigation.StopMoving(m_Owner);
            m_MoveState.Reset();
        }

        private MoveToTargetState m_MoveState = new MoveToTargetState();
        private Entity m_TargetEntity = null;
        private Entity m_Owner = null;
        private float m_MoveSpeed = DefaultMoveSpeed;
        private float m_RotationSpeed = DefaultRotationSpeed;

        private const float DefaultMoveSpeed = 1f;
        private const float DefaultRotationSpeed = 3.5f;
    }

    [UIName("Go to a random point")]
    public class AITaskGoToRandomPoint : AITask
    {
        [Tooltip("Entity that should be moved")]
        public string Owner;

        [UIName("Radius")]
        [Tooltip("A random point within this radius will be selected")]
        public string Radius;

        [UIName("Move speed")]
        public string MoveSpeed;

        [UIName("Rotation speed")]
        [Tooltip("Ignored for crowd agents with `Face Movement Direction` on (their `Turn Rate` is used)")]
        public string RotationSpeed;

        [UIName("Acceptance Distance")]
        public float AcceptanceDistance = 0.5f;

        [UIName("Fallback Agent Type")]
        [Tooltip("Nav mesh to move on if the owner doesn't have a Navigation Crowd Agent Component (crowd agents always move on the nav mesh of their agent).\nAuto - the default agent type")]
        public NavAgentType FallbackAgentType;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_MoveSpeed = m_Blackboard.GetValue(MoveSpeed, DefaultMoveSpeed);
            m_RotationSpeed = m_Blackboard.GetValue(RotationSpeed, DefaultRotationSpeed);
            m_MoveState.Reset();

            m_bHasTarget = false;
            if (m_Owner != null && m_Blackboard.TryGetValue(Radius, out float radius))
            {
                NavMesh navMesh = NavMesh.GetNavMeshFor(m_Owner, FallbackAgentType);
                if (navMesh.IsValid() && navMesh.FindRandomPointInCircle(m_Owner.GetWorldLocation(), radius, out m_Target))
                    m_bHasTarget = true;
            }
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null || !m_bHasTarget)
                return AINodeStatus.Failed;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_Target, AcceptanceDistance, ts, m_MoveSpeed, m_RotationSpeed, m_MoveState, FallbackAgentType);
            if (!bFinished)
                return AINodeStatus.Running;

            return m_MoveState.Result == MoveToTargetResult.NoPath ? AINodeStatus.Failed : AINodeStatus.Succeeded;
        }

        public override void OnEnd(AINodeStatus status)
        {
            base.OnEnd(status);

            if (m_Owner != null)
                Navigation.StopMoving(m_Owner);
            m_MoveState.Reset();
        }

        protected MoveToTargetState m_MoveState = new MoveToTargetState();
        protected Entity m_Owner = null;
        protected Vector3 m_Target = new Vector3(0);
        protected bool m_bHasTarget = false;
        protected float m_MoveSpeed = DefaultMoveSpeed;
        protected float m_RotationSpeed = DefaultRotationSpeed;

        private const float DefaultMoveSpeed = 1f;
        private const float DefaultRotationSpeed = 3.5f;
    }

    [UIName("Play Sound")]
    [Tooltip("Fails if no audio is provided, or a 3D sound has no position. Succeeds immediately when the sound is spawned")]
    public class AITaskPlaySound : AITask
    {
        public AssetAudio Audio = null;

        [UIName("Audio Blackboard Key")]
        [Tooltip("If specified, an audio from the blackboard will be used")]
        public string AudioBlackboardKey = "";

        public float Volume = 1.0f;
        public float Pan = 0.0f;
        public float Pitch = 1.0f;
        [UIName("Loop Count")]
        public int LoopCount = -1;

        [UIName("Spawn as 3D")]
        public bool b3D = false;

        [UIName("Position")]
        [Tooltip("Blackboard key of the 3D position (Vector3). Used only if it's a 3D sound")]
        public string Position = "";

        [UIName("Rolloff Model")]
        [Tooltip("Used only if it's a 3D sound")]
        public RollOffModel RollOff = RollOffModel.Default;

        public override void OnBegin()
        {
            if (AudioBlackboardKey != null && AudioBlackboardKey.Length > 0)
            {
                m_Blackboard.TryGetValue(AudioBlackboardKey, out m_AudioToUse);
            }
            else
            {
                m_AudioToUse = Audio;
            }

            m_bHasPosition = b3D && m_Blackboard.TryGetValue(Position, out m_Position);
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_AudioToUse == null || (b3D && !m_bHasPosition))
                return AINodeStatus.Failed;

            SoundSettings settings = new SoundSettings(Volume);
            settings.Pan = Pan;
            settings.Pitch = Pitch;
            settings.LoopCount = LoopCount;

            Sound fireSound;
            if (b3D)
            {
                fireSound = new Sound3D(m_AudioToUse, m_Position, RollOff, settings);
            }
            else
            {
                fireSound = new Sound2D(m_AudioToUse, settings);
            }
            fireSound.Play();

            return AINodeStatus.Succeeded;
        }

        protected AssetAudio m_AudioToUse = null;
        protected Vector3 m_Position;
        protected bool m_bHasPosition = false;
    }

    [UIName("Wait")]
    public class AITaskWait : AITask
    {
        public float Delay = 1.0f;

        [UIName("Delay Blackboard Key")]
        [Tooltip("If specified, a delay from the blackboard will be used. If the blackboard doesn't have it, Delay is used")]
        public string DelayBlackboardKey = "";

        public override void OnBegin()
        {
            m_Delay = m_Blackboard.GetValue(DelayBlackboardKey, Delay);
        }

        public override void OnEnd(AINodeStatus status)
        {
            base.OnEnd(status);
            m_Time = 0.0f;
        }

        protected override AINodeStatus Update(float ts)
        {
            m_Time += ts;
            return m_Time >= m_Delay ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        private float m_Time = 0.0f;
        private float m_Delay = 1.0f;
    }
}
