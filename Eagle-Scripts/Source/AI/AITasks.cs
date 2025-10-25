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
            m_Blackboard.TryGetValue(RotationSpeed, out m_RotationSpeed);
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null || m_TargetEntity == null)
                return AINodeStatus.Failed;

            Vector3 location = m_Owner.WorldLocation;
            Vector3 target = m_TargetEntity.WorldLocation;

            Vector3 dir = Mathf.Normalize(target - location);
            Rotator targetRot = Mathf.LookAtY(dir);
            Rotator result = Mathf.Slerp(m_Owner.WorldRotation, targetRot, ts * m_RotationSpeed);
            m_Owner.WorldRotation = result;

            return targetRot.Equals(result, Epsilon) ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        private Entity m_TargetEntity = null;
        private Entity m_Owner = null;
        private float m_RotationSpeed = 3.5f;
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
        public string RotationSpeed;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_Blackboard.TryGetValue(Target, out m_Target);
            m_Blackboard.TryGetValue(MoveSpeed, out m_MoveSpeed);
            m_Blackboard.TryGetValue(RotationSpeed, out m_RotationSpeed);
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null)
                return AINodeStatus.Failed;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_Target, 0.2f, ts, m_MoveSpeed, m_RotationSpeed);
            return bFinished ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        protected Vector3 m_Target = new Vector3(0);
        private Entity m_Owner = null;
        private float m_MoveSpeed = 1f;
        private float m_RotationSpeed = 3.5f;
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
        public string RotationSpeed;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_Blackboard.TryGetValue(TargetEntity, out m_TargetEntity);
            m_Blackboard.TryGetValue(MoveSpeed, out m_MoveSpeed);
            m_Blackboard.TryGetValue(RotationSpeed, out m_RotationSpeed);
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null || m_TargetEntity == null)
                return AINodeStatus.Failed;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_TargetEntity.WorldLocation, 0.2f, ts, m_MoveSpeed, m_RotationSpeed);
            return bFinished ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        private Entity m_TargetEntity = null;
        private Entity m_Owner = null;
        private float m_MoveSpeed = 1f;
        private float m_RotationSpeed = 3.5f;
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
        public string RotationSpeed;

        public override void OnBegin()
        {
            m_Blackboard.TryGetValue(Owner, out m_Owner);
            m_Blackboard.TryGetValue(MoveSpeed, out m_MoveSpeed);
            m_Blackboard.TryGetValue(RotationSpeed, out m_RotationSpeed);

            if (m_Owner != null && m_Blackboard.TryGetValue(Radius, out float radius))
                Navigation.FindRandomPointInCircle(m_Owner.WorldLocation, radius, out m_Target);
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null)
                return AINodeStatus.Failed;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_Target, 0.2f, ts, m_MoveSpeed, m_RotationSpeed);
            return bFinished ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        protected Entity m_Owner = null;
        protected Vector3 m_Target = new Vector3(0);
        protected float m_MoveSpeed = 1f;
        protected float m_RotationSpeed = 3.5f;
    }

    [UIName("Play Sound")]
    [Tooltip("Failes, if no audio is provided. Succeeds immeditely when sound is spawned")]
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
        [Tooltip("Blackboard key of the 3D position. Used only if it's a 3D sound")]
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

            if (b3D)
            {
                m_Blackboard.TryGetValue(Position, out m_AudioToUse);
            }
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_AudioToUse == null)
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
    }

    [UIName("Wait")]
    public class AITaskWait : AITask
    {
        public float Delay = 1.0f;

        [UIName("Delay Blackboard Key")]
        [Tooltip("If specified, a delay from the blackboard will be used")]
        public string DelayBlackboardKey = "";

        public override void OnBegin()
        {
            if (DelayBlackboardKey != null && DelayBlackboardKey.Length > 0)
            {
                m_Blackboard.TryGetValue(DelayBlackboardKey, out m_Delay);
            }
            else
            {
                m_Delay = Delay;
            }
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
