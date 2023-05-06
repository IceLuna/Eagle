using Eagle;
using System;
using System.Threading;

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
                Color3 lightColor = component.LightColor;
                float cosCalc = ((float)System.Math.Cos(Speed * time));
                lightColor = new Color3(cosCalc * cosCalc);
                component.LightColor = lightColor;
            }
        }
    }

    public class Fog : Entity
    {
        public Color3 Color = new Color3(0.6f, 0.4f, 0.3f);
        public float Speed = 1f;
        public bool bEnabled = true;

        private static float s_DefaultMinFog = 3f;
        private static float s_DefaultMaxFog = 7.5f;
        private float time = s_DefaultMaxFog;

        void OnCreate()
        {
            Renderer.FogMinDistance = s_DefaultMinFog;
        }

        void OnUpdate(float ts)
        {
            time += ts * Speed;
            Renderer.FogMaxDistance = Math.Min(time, 41f);
            Renderer.FogColor = Color;
            Renderer.bFogEnabled = bEnabled;
        }
    }
}
