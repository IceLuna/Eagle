using System;

namespace Eagle
{
    // Allows to run checks before/when a node is running
    public class AINodeDecorator
    {
        // The task should continue execution only if `Succeeded` is returned.
        // The task should fail if `Failed` is returned.
        // The task should skip its execution and wait if `Running` is returned.
        public virtual AINodeStatus Update(float ts) { return AINodeStatus.Failed; }

        public virtual void Reset() { } // Resets the state

        // Called when the node it's attached to finishes (succeeds, fails, or is aborted), before `Reset`.
        // @bNodeRan. False if the node didn't run, because a decorator didn't let it
        public virtual void OnNodeEnd(AINodeStatus status, bool bNodeRan) { }
        
        internal void SetBlackboard(AIBlackboard blackboard)
        {
            m_Blackboard = blackboard;
            OnBlackboardChanged();
        }

        protected virtual void OnBlackboardChanged() { }

        protected AIBlackboard m_Blackboard = null;
    }

    [UIName("Delay")]
    public class AINodeDecoratorDelay : AINodeDecorator
    {
        public string Delay;

        public override AINodeStatus Update(float ts)
        {
            m_Time += ts;
            if (!m_Blackboard.TryGetValue(Delay, out float delay))
                delay = 0f;

            return m_Time >= delay ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        public override void Reset()
        {
            m_Time = 0f;
        }

        private float m_Time = 0f;
    }

    [UIName("Cooldown")]
    [Tooltip("Prevents a node from running until its cooldown ends. The cooldown starts when the node finishes: succeeds, fails, or is aborted.\n"
        + "It's checked when the node starts, so a running node isn't stopped. The time is the AI Task Manager's, so it only passes while the tree is updated")]
    public class AINodeDecoratorCooldown : AINodeDecorator
    {
        [UIName("Cooldown Time")]
        [Tooltip("Seconds")]
        public float CooldownTime = 5f;

        public override AINodeStatus Update(float ts)
        {
            if (m_bLetRun)
                return AINodeStatus.Succeeded;

            if (m_bCoolingDown)
            {
                AITaskManager manager = AITaskManager.GetCurrent();
                if (manager == null)
                {
                    Log.Error("Cooldown decorator: failed to get AI Task Manager. Ignored");
                }
                else if (manager.GetTime() < m_CooldownEnd)
                {
                    return AINodeStatus.Failed;
                }
                else
                {
                    m_bCoolingDown = false;
                }
            }

            m_bLetRun = true;
            return AINodeStatus.Succeeded;
        }

        public override void OnNodeEnd(AINodeStatus status, bool bNodeRan)
        {
            if (!bNodeRan)
                return;

            AITaskManager manager = AITaskManager.GetCurrent();
            if (manager == null)
            {
                Log.Error("Cooldown decorator: failed to get AI Task Manager. Ignored");
                return;
            }

            m_CooldownEnd = manager.GetTime() + CooldownTime;
            m_bCoolingDown = true;
        }

        public override void Reset()
        {
            m_bLetRun = false;
        }

        private double m_CooldownEnd = 0.0;
        private bool m_bCoolingDown = false;
        private bool m_bLetRun = false;
    }

    [UIName("Is Blackboard Value Set")]
    public class AINodeDecoratorIsBlackboardValueSet : AINodeDecorator
    {
        public string Key;

        [UIName("Check if set")]
        [Tooltip("If false, it'll do the opposite: check if the key is not set")]
        public bool bCheckIfSet = true;

        [UIName("Abort if changed")]
        [Tooltip("If false, the task won't abort its execution if the value changes")]
        public bool bAbortIfChanged = true;

        public override AINodeStatus Update(float ts)
        {
            if (m_DidRun && !bAbortIfChanged)
            {
                return AINodeStatus.Succeeded;
            }

            bool bCanRun = true;
            bool bHasValue = m_Blackboard.HasValue(Key);
            m_Blackboard.TryGetValue(Key, out object currentVal);

            if (m_DidRun)
            {
                if (!Equals(m_PrevValue, currentVal))
                    return AINodeStatus.Failed;
            }
            else
            {
                m_PrevValue = currentVal;
                bCanRun = bCheckIfSet ? bHasValue : !bHasValue;
            }


            m_DidRun = true;
            return bCanRun ? AINodeStatus.Succeeded : AINodeStatus.Failed;
        }

        public override void Reset()
        {
            m_DidRun = false;
        }

        private bool m_DidRun = false;
        private object m_PrevValue = null;
    }

    [UIName("Random Chance")]
    [Tooltip("Succeeds with a random chance")]
    public class AINodeDecoratorRandomChance : AINodeDecorator
    {
        [Tooltip("Percent to succeed")]
        public int Chance = 50;

        public override AINodeStatus Update(float ts)
        {
            if (m_DidRun)
            {
                return m_Status;
            }

            m_DidRun = true;

            float rand = (float)m_Random.NextDouble();
            m_Status = rand < (Chance / 100.0f) ? AINodeStatus.Succeeded : AINodeStatus.Failed;

            return m_Status;
        }

        public override void Reset()
        {
            m_DidRun = false;
        }

        static private Random m_Random = new Random();
        private AINodeStatus m_Status = AINodeStatus.Succeeded;
        private bool m_DidRun = false;
    }
}
