using Eagle;
using System;

namespace Sandbox
{
    public class Rolling : Entity
    {
        public Vector3 Force  = new Vector3(0f, 0f, 2f);
        public Vector3 RotationForce = new Vector3(0f, 2f, 0f);
        public float Speed = 2f;
        void OnCreate()
        { }

        void OnUpdate(float ts)
        {
            Location = Location + Force * ts * Speed;
            Rotation = Rotation - RotationForce * ts * Speed;
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
            time += ts;
            Location = Location + (Force * ts * ((float)System.Math.Sin(10f * time)));
            Rotation = Rotation - (RotationForce * ts * ((float)System.Math.Sin(10f * time)));
        }
    }
}