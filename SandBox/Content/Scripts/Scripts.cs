using Eagle;
using System;

namespace Sandbox
{
    public class Rolling : Entity
    {
        public Vector3 Force = new Vector3(0f, 0f, 2f);
        public float Speed = 2f;
        void OnCreate()
        {
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
                Vector3 lightColor = GetComponent<DirectionalLightComponent>().LightColor;
                float cosCalc = ((float)System.Math.Cos(Speed * time));
                lightColor = new Vector3(cosCalc * cosCalc);
                GetComponent<DirectionalLightComponent>().LightColor = lightColor;
            }
        }
    }
}
