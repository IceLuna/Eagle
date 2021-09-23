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
        {
        }

        void OnUpdate(float ts)
        {
            WorldLocation = WorldLocation + Force * ts * Speed;
            WorldRotation = WorldRotation - RotationForce * ts * Speed;
        }

        void OnDestroy()
        {
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
            WorldLocation = WorldLocation + (Force * ts * ((float)System.Math.Sin(10f * time)));
            WorldRotation = WorldRotation - (RotationForce * ts * ((float)System.Math.Sin(10f * time)));
        }
    }

    public class BlinkingDirectionalLight : Entity
    {
        public float Speed = 1f;
        private float time = 0f;
        void OnUpdate(float ts)
        {
            time += ts;
            if (HasComponent<DirectionalLightComponent>())
            {
                Vector3 lightColor = GetComponent<DirectionalLightComponent>().LightColor;
                float cosCalc = ((float)System.Math.Cos(Speed * time));
                lightColor = new Vector3(cosCalc * cosCalc);
                GetComponent<DirectionalLightComponent>().LightColor = lightColor;
            }
        }
    }
}
