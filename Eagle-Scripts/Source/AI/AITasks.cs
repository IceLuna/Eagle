using System;
using System.Collections.Generic;

namespace Eagle
{
    public class AITask : AINode
    { }

    public class AITaskGoToPoint : AITask
    {
        public string Owner;
        public string Target;
        public string MoveSpeed;
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

    public class AITaskGoToEntity : AITask
    {
        public string Owner;
        public string TargetEntity;
        public string MoveSpeed;
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

    public class AITaskGoToRandomPoint : AITask
    {
        public string Owner;
        public string Radius;
        public string MoveSpeed;
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
