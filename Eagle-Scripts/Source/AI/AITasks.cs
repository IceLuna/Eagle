using System;
using System.Collections.Generic;

namespace Eagle
{
    public class AITask : AINode
    { }

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
            if (m_Blackboard.TryGetValue(Owner, out Entity entity))
                m_Owner = entity;
            if (m_Blackboard.TryGetValue(Target, out Vector3 target))
                m_Target = target;
            if (m_Blackboard.TryGetValue(MoveSpeed, out float moveSpeed))
                m_MoveSpeed = moveSpeed;
            if (m_Blackboard.TryGetValue(RotationSpeed, out float rotationSpeed))
                m_RotationSpeed = rotationSpeed;
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null)
                return AINodeStatus.Failed;

            if (!CanRun(ts, out AINodeStatus status))
                return status;

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
            if (m_Blackboard.TryGetValue(Owner, out Entity ownerEntity))
                m_Owner = ownerEntity;
            if (m_Blackboard.TryGetValue(TargetEntity, out Entity targetEntity))
                m_TargetEntity = targetEntity;
            if (m_Blackboard.TryGetValue(MoveSpeed, out float moveSpeed))
                m_MoveSpeed = moveSpeed;
            if (m_Blackboard.TryGetValue(RotationSpeed, out float rotationSpeed))
                m_RotationSpeed = rotationSpeed;
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null)
                return AINodeStatus.Failed;

            if (m_TargetEntity == null)
                return AINodeStatus.Failed;

            if (!CanRun(ts, out AINodeStatus status))
                return status;

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
            if (m_Blackboard.TryGetValue(Owner, out Entity entity))
                m_Owner = entity;
            if (m_Blackboard.TryGetValue(Radius, out float radius))
                Navigation.FindRandomPointInCircle(entity.WorldLocation, radius, out m_Target);
            if (m_Blackboard.TryGetValue(MoveSpeed, out float moveSpeed))
                m_MoveSpeed = moveSpeed;
            if (m_Blackboard.TryGetValue(RotationSpeed, out float rotationSpeed))
                m_RotationSpeed = rotationSpeed;
        }

        protected override AINodeStatus Update(float ts)
        {
            if (m_Owner == null)
                return AINodeStatus.Failed;

            if (!CanRun(ts, out AINodeStatus status))
                return status;

            bool bFinished = Navigation.MoveToTarget(m_Owner, m_Target, 0.2f, ts, m_MoveSpeed, m_RotationSpeed);
            return bFinished ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        protected Entity m_Owner = null;
        protected Vector3 m_Target = new Vector3(0);
        protected float m_MoveSpeed = 1f;
        protected float m_RotationSpeed = 3.5f;
    }
}
