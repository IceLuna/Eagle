using Eagle;
using System;

namespace Sandbox
{
    public class Rolling : Entity
    {
        public Vector3 Force;
        public Vector3 RotationForce;
        void OnCreate()
        {
            Force = new Vector3(0f, 0f, 2f);
            RotationForce = new Vector3(0f, 2f, 0f);
        }

        void OnUpdate(float ts)
        {
            TransformComponent transform = GetComponent<TransformComponent>();
            transform.Translation = transform.Translation + Force * ts;
            transform.Rotation = transform.Rotation - RotationForce * ts;
        }
    }

    public class Shaking : Entity
    {
        public Vector3 Force;
        public Vector3 RotationForce;
        private float time = 0f;

        void OnCreate()
        {
            Force = new Vector3(0f, 0f, 2f);
            RotationForce = new Vector3(0f, 2f, 0f);
        }
        void OnUpdate(float ts)
        {
            TransformComponent transform = GetComponent<TransformComponent>();
            time += ts;
            transform.Translation = transform.Translation + (Force * ts * ((float)System.Math.Sin(10f * time)));
            transform.Rotation = transform.Rotation - (RotationForce * ts * ((float)System.Math.Sin(10f * time)));
        }
    }
}