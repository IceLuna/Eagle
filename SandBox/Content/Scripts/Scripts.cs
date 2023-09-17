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

        public override void OnCreate()
        {
            AddCollisionBeginCallback(BeginCollisionCallback);

            // Example of how to check entities script.
            //Type type = GetComponent<ScriptComponent>().GetScriptType();
            //Log.Trace((type == typeof(Rolling)).ToString());
        }

        public override void OnUpdate(float ts)
        {
            AddForce(Force * Speed * ts, ForceMode.Force);
        }
    }

    public class Character : Entity
    {
        private Entity m_Camera;
        private Vector3 m_CameraForward;

        private Vector2 m_MousePos;
        private StaticMesh m_Projectile;
        public float WalkSpeed = 1f;
        public float RunSpeed = 2.25f;
        public float JumpStrength = 50f;
        private float m_HeightBeforeCrouch = 1f;

        public Color3 ProjectileColor = new Color3(5f, 0f, 5f);
        public float ProjectileSpeed = 10f;

        public override void OnCreate()
        {
            Input.SetCursorMode(CursorMode.Hidden);
            m_Projectile = new StaticMesh(Project.GetContentPath() + "/Meshes/sphere.fbx");
        }

        public override void OnDestroy()
        {
            Input.SetCursorMode(CursorMode.Normal);
        }

        public override void OnEvent(Event e)
        {
            if (e.GetEventType() == EventType.KeyPressed)
            {
                KeyPressedEvent keyPressed = e as KeyPressedEvent;
                if (keyPressed.Key == KeyCode.F1)
                    Scene.OpenScene(Project.GetContentPath() + "/Scenes/3DScene.eagle");
                else if (keyPressed.Key == KeyCode.F2)
                    Scene.OpenScene(Project.GetContentPath() + "/Scenes/Demo.eagle");
                else if (keyPressed.Key == KeyCode.Space && keyPressed.RepeatCount == 0) // Jumping
                {
                    float yVel = GetLinearVelocity().Y;
                    yVel = Math.Abs(yVel);
                    bool bOnTheGround = yVel < 0.0001f;
                    if (bOnTheGround)
                    {
                        AddForce(new Vector3(0, 1, 0) * JumpStrength, ForceMode.Impulse);
                    }
                }
                if (keyPressed.Key == KeyCode.LeftControl && keyPressed.RepeatCount == 0) // Crouch
                {
                    CapsuleColliderComponent capsule = GetComponent<CapsuleColliderComponent>();
                    m_HeightBeforeCrouch = capsule.GetHeight();
                    capsule.SetHeight(m_HeightBeforeCrouch * 0.5f);
                }
            }
            else if (e.GetEventType() == EventType.KeyReleased)
            {
                KeyReleasedEvent keyReleased = e as KeyReleasedEvent;
                if (keyReleased.Key == KeyCode.LeftControl) // Crouch
                {
                    CapsuleColliderComponent capsule = GetComponent<CapsuleColliderComponent>();
                    capsule.SetHeight(m_HeightBeforeCrouch);
                }
            }
            else if (e.GetEventType() == EventType.MouseButtonPressed)
            {
                MouseButtonPressedEvent mousePressed = e as MouseButtonPressedEvent;
                if (mousePressed.Key == MouseButton.Button0)
                    SpawnProjectile();
            }
        }

        public override void OnUpdate(float ts)
        {
            HandleMovement(ts);
            m_MousePos = Input.GetMousePosition();
        }

        void SpawnProjectile()
        {
            Entity projectile = Entity.SpawnEntity("Projectile");
            projectile.WorldLocation = m_Camera.WorldLocation + m_CameraForward * 0.1f;
            projectile.WorldRotation = m_Camera.WorldRotation;
            projectile.WorldScale = new Vector3(0.05f);

            StaticMeshComponent sm = projectile.AddComponent<StaticMeshComponent>();
            sm.Mesh = m_Projectile;
            sm.bCastsShadows = false;

            Material smMaterial = sm.GetMaterial();
            smMaterial.EmissiveTexture = Texture2D.White;
            smMaterial.EmissiveIntensity = ProjectileColor;
            sm.SetMaterial(smMaterial);

            // Must be created first in order to set body type. Because it's checked once and for all when a collider is added
            RigidBodyComponent body = projectile.AddComponent<RigidBodyComponent>();
            body.SetBodyType(PhysicsBodyType.Dynamic);
            body.SetEnableGravity(true);

            SphereColliderComponent collider = projectile.AddComponent<SphereColliderComponent>();
            collider.SetRadius(1f);

            projectile.AddForce(m_CameraForward * ProjectileSpeed, ForceMode.Impulse);
        }

        void HandleMovement(float ts)
        {
            float speed = (Input.IsKeyPressed(KeyCode.LeftShift) ? RunSpeed : WalkSpeed) * ts;
            m_Camera = GetChildrenByName("Camera");
            m_CameraForward = m_Camera.GetForwardVector();
            Vector3 right = m_Camera.GetRightVector();

            if (Input.IsKeyPressed(KeyCode.W))
            {
                WorldLocation = WorldLocation + m_CameraForward * speed;
            }
            if (Input.IsKeyPressed(KeyCode.S))
            {
                WorldLocation = WorldLocation - m_CameraForward * speed;
            }
            if (Input.IsKeyPressed(KeyCode.D))
            {
                WorldLocation = WorldLocation + right * speed;
            }
            if (Input.IsKeyPressed(KeyCode.A))
            {
                WorldLocation = WorldLocation - right * speed;
            }
        }
    }

    public class Flashlight : Entity
    {
        public float BatteryCharge = 100f;
        public float BatteryChargeSpeed = 5f;
        public float BatteryDischargeSpeed = 2f;
        private float m_BatteryDischargeSpeedCoef = 1f;

        private Texture2D[] m_BatteryChargeIcons = new Texture2D[4];

        public override void OnCreate()
        {
            string basePath = Project.GetContentPath() + "/Textures/Battery";
            m_BatteryChargeIcons[0] = new Texture2D(basePath + "/battery_low.png");
            m_BatteryChargeIcons[1] = new Texture2D(basePath + "/battery_low_mid.png");
            m_BatteryChargeIcons[2] = new Texture2D(basePath + "/battery_high_mid.png");
            m_BatteryChargeIcons[3] = new Texture2D(basePath + "/battery_high.png");
        }

        public override void OnEvent(Event e)
        {
            if (e.GetEventType() == EventType.KeyPressed)
            {
                KeyPressedEvent keyPressed = e as KeyPressedEvent;
                if (keyPressed.Key == KeyCode.F && keyPressed.RepeatCount == 0) // Toggle
                {
                    SpotLightComponent light = GetComponent<SpotLightComponent>();
                    light.bAffectsWorld = !light.bAffectsWorld;
                }
                else if (keyPressed.Key == KeyCode.R && keyPressed.RepeatCount == 0) // Stronger
                {
                    SpotLightComponent light = GetComponent<SpotLightComponent>();
                    light.OuterCutoffAngle /= 2f;
                    light.InnerCutoffAngle /= 2f;
                    light.Intensity *= 2f;
                    m_BatteryDischargeSpeedCoef = 2f;
                }
            }
            else if (e.GetEventType() == EventType.KeyReleased)
            {
                KeyReleasedEvent keyReleased = e as KeyReleasedEvent;
                if (keyReleased.Key == KeyCode.R) // !Stronger
                {
                    SpotLightComponent light = GetComponent<SpotLightComponent>();
                    light.OuterCutoffAngle *= 2f;
                    light.InnerCutoffAngle *= 2f;
                    light.Intensity /= 2f;
                    m_BatteryDischargeSpeedCoef = 1f;
                }
            }
        }

        public override void OnUpdate(float ts)
        {
            HandleButteryCharge(ts);
        }

        void HandleButteryCharge(float ts)
        {
            SpotLightComponent light = GetComponent<SpotLightComponent>();
            bool bEnabled = light.bAffectsWorld;

            if (bEnabled)
            {
                if (BatteryCharge <= 0f)
                    light.bAffectsWorld = false;

                // Double the discharge speed if strong light is active
                BatteryCharge = Math.Max(0f, BatteryCharge - ts * BatteryDischargeSpeed * m_BatteryDischargeSpeedCoef);
            }
            else
            {
                BatteryCharge = Math.Min(100f, BatteryCharge + ts * BatteryChargeSpeed);
            }

            GetComponent<Text2DComponent>().Text = $"{(uint)BatteryCharge}%";

            uint textureIndex = 0;
            if (BatteryCharge >= 75)
                textureIndex = 3;
            else if (BatteryCharge >= 50)
                textureIndex = 2;
            else if (BatteryCharge >= 25)
                textureIndex = 1;
            GetComponent<Image2DComponent>().Texture = m_BatteryChargeIcons[textureIndex];
        }
    }

    public class CameraMovement : Entity
    {
        private Vector2 m_MousePos;
        public float RotationSpeed = 1f;
        private float m_HeightBeforeCrouch = 0f;

        public override void OnCreate()
        {
            m_MousePos = Input.GetMousePosition();
        }

        public override void OnEvent(Event e)
        {
            if (e.GetEventType() == EventType.KeyPressed)
            {
                KeyPressedEvent keyPressed = e as KeyPressedEvent;
                if (keyPressed.Key == KeyCode.LeftControl && keyPressed.RepeatCount == 0) // Crouch
                {
                    Vector3 relLoc = RelativeLocation;
                    m_HeightBeforeCrouch = relLoc.Y;
                    relLoc.Y = 0.1f;
                    RelativeLocation = relLoc;
                }
            }
            else if (e.GetEventType() == EventType.KeyReleased)
            {
                KeyReleasedEvent keyReleased = e as KeyReleasedEvent;
                if (keyReleased.Key == KeyCode.LeftControl) // Crouch
                {
                    Vector3 relLoc = RelativeLocation;
                    relLoc.Y = m_HeightBeforeCrouch;
                    RelativeLocation = relLoc;
                }
            }
        }

        public override void OnUpdate(float ts)
        {
            HandleCameraMovement(ts);
        }
    
        void HandleCameraMovement(float ts)
        {
            Vector2 offset = m_MousePos - Input.GetMousePosition();

            Vector3 forward = GetForwardVector();
            Vector3 up = new Vector3(0f, 1f, 0f);

            // Limit cameras vertical rotation
            // Shouldn't prevent camera from moving if camera wants to move away from max rotation
            float cosTheta = Mathf.Dot(forward, up);
            bool bMoveInTheSameDir = Math.Sign(cosTheta) == Math.Sign(offset.Y);
            bool bStopXRotation = (Math.Abs(cosTheta) > 0.999f) && bMoveInTheSameDir ? true : false;

            float speed = RotationSpeed;
            float rotationX = bStopXRotation ? 0f : Mathf.Radians(offset.Y * speed);
            float rotationY = Mathf.Radians(offset.X * speed);

            Quat rotX = Mathf.AngleAxis(rotationX, new Vector3(1f, 0f, 0f));
            Quat rotY = Mathf.AngleAxis(rotationY, up);

            Quat rotation = WorldRotation.Rotation;
            Quat origRotation = rotation;
            rotation = rotation * rotX;
            rotation = rotY * rotation;

            // If we flipped over, reset back
            {
                forward = GetForwardVector();
                float cosThetaAfter = Mathf.Dot(forward, up);
                bool bLock = bMoveInTheSameDir && Math.Abs(cosThetaAfter) < Math.Abs(cosTheta);
                if (bLock)
                {
                    rotation = origRotation;
                    rotation = rotY * rotation;
                }
            }

            Rotator resultRotator = new Rotator();
            resultRotator.Rotation = rotation;
            WorldRotation = resultRotator;

            m_MousePos = Input.GetMousePosition();
        }
    }

    public class FogScript : Entity
    {
        FogSettings FogSettingsOnStart = new FogSettings();
        FogSettings Fog = new FogSettings();
        public Color3 Color = new Color3(0.6f, 0.4f, 0.3f);
        public float Speed = 1f;
        public bool bEnabled = true;

        private static float s_DefaultMinFog = 3f;
        private static float s_DefaultMaxFog = 7.5f;
        private float time = s_DefaultMaxFog;

        public override void OnCreate()
        {
            FogSettingsOnStart = Renderer.GetFogSettings();
            Fog = FogSettingsOnStart;
            Fog.MinDistance = s_DefaultMinFog;
        }

        public override void OnUpdate(float ts)
        {
            time += ts * Speed;
            Fog.MaxDistance = Math.Min(time, 41f);
            Fog.Color = Color;
            Fog.bEnabled = bEnabled;
            Renderer.SetFogSettings(Fog);
        }

        public override void OnDestroy()
        {
            Renderer.SetFogSettings(FogSettingsOnStart);
        }
    }
}
