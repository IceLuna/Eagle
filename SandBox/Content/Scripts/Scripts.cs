using Eagle;
using System;

namespace Sandbox
{
    public class Rolling : Entity
    {
        public Vector3 Force = new Vector3(0f, 0f, 2f);
        public float Speed = 2f;

        private Action<Entity> BeginCollisionCallback = ent =>
        {
            Sound2D.Play("../Sandbox/Content/Sounds/BarrelHit.wav", 0.05f);
        };

        void OnCreate()
        {
            AddCollisionBeginCallback(BeginCollisionCallback);
        }

        void OnUpdate(float ts)
        {
            AddForce(Force * Speed * ts, ForceMode.Force);
        }

        void OnDestroy()
        {
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
                DirectionalLightComponent component = GetComponent<DirectionalLightComponent>();
                Vector3 lightColor = component.LightColor;
                float cosCalc = ((float)System.Math.Cos(Speed * time));
                lightColor = new Vector3(cosCalc * cosCalc);
                component.LightColor = lightColor;
            }
        }
    }
}
