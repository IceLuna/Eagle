using System;

namespace Eagle.AI
{
    // Handles AI tasks execution
    public class AITaskManager
    {
        public void Update(float ts)
        {
            m_Root?.Run(ts);
        }

        public void SetRoot(AINode root)
        {
            m_Root = root;
            m_Root?.SetBlackboard(m_Blackboard);
        }

        public AINode GetRoot() { return m_Root; }

        public void SetBlackboard(AIBlackboard blackboard)
        {
            m_Blackboard = blackboard;
            m_Root?.SetBlackboard(blackboard);
        }

        public AIBlackboard GetBlackboard() { return m_Blackboard; }

        protected AINode m_Root = null;
        protected AIBlackboard m_Blackboard = new AIBlackboard();
    }
}
