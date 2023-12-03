using Eagle;
using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.Threading;

namespace Sandbox
{
    public class Rolling : Entity
    {
        private RigidBodyComponent m_RigidBody;
        public Vector3 Force = new Vector3(0f, 0f, 2f);
        public float Speed = 2f;

        Sound3D m_BarrelHitSound;

        private Action<Entity, Entity, CollisionInfo> BeginCollisionCallback = (thisEnt, ent, collisionInfo) =>
        {
            Rolling thisObj = thisEnt.GetComponent<ScriptComponent>().GetInstance() as Rolling;
            thisObj.m_BarrelHitSound.SetWorldPosition(collisionInfo.Position);
            thisObj.m_BarrelHitSound.Play();
        };

        public override void OnCreate()
        {
            SoundSettings soundSettings = new SoundSettings();
            soundSettings.Volume = 0.05f;
            m_BarrelHitSound = new Sound3D(Project.GetContentPath() + "/Sounds/BarrelHit.wav", new Vector3(0f), RollOffModel.Default, soundSettings);

            AddCollisionBeginCallback(BeginCollisionCallback);
            m_RigidBody = GetComponent<RigidBodyComponent>();
        }

        public override void OnUpdate(float ts)
        {
            m_RigidBody.AddForce(Force * Speed * ts, ForceMode.Force);
        }
    }

    public class Projectile : Entity
    {
        private static StaticMesh s_ProjectileMesh = new StaticMesh(Project.GetContentPath() + "/Meshes/sphere.fbx");

        private Sound3D m_ShootSound = null;
        public Sound3D HitSound = null;
        private Color3 m_ProjectileColor;
        private float m_ProjectileColorIntensity = 0.5f;
        private float m_ProjectileSpeed = 10f;

        // To prevent multiple explosions from a single projectile
        // Also small projectiles don't generate explosions
        private bool m_bExploaded = false;

        private Action<Entity, Entity, CollisionInfo> BeginCollisionCallback = (thisEnt, ent, collisionInfo) =>
        {
            Projectile thisObj = thisEnt.GetComponent<ScriptComponent>().GetInstance() as Projectile;
            if (thisObj.m_bExploaded)
                return;

            // Ignore character & self collisions
            if (ent.HasComponent<ScriptComponent>())
            {
                Type scriptType = ent.GetComponent<ScriptComponent>().GetScriptType();
                if (scriptType == typeof(Character) || scriptType == typeof(Projectile))
                    return;
            }

            thisObj.m_bExploaded = true;

            if (thisObj.HitSound != null)
            {
                thisObj.HitSound.SetWorldPosition(collisionInfo.Position);
                thisObj.HitSound.Play();
            }

            for (uint i = 0; i < 10; ++i)
            {
                // Create an entity
                Entity entity = Entity.SpawnEntity("Projectile");
                entity.WorldLocation = collisionInfo.Position + collisionInfo.Normal * 0.025f;
                entity.WorldRotation = thisObj.WorldRotation;
                entity.WorldScale = new Vector3(0.01f);

                // Attach `Projectile` script to it
                ScriptComponent sc = entity.AddComponent<ScriptComponent>();
                sc.SetScript(typeof(Projectile));

                Random randomGenerator = new Random();
                float randomR = (float)randomGenerator.NextDouble() * 10f;
                float randomG = (float)randomGenerator.NextDouble() * 10f;
                float randomB = (float)randomGenerator.NextDouble() * 10f;
                Color3 projectileColor = new Color3(randomR, randomG, randomB); // Random color

                float randomX = (float)(randomGenerator.NextDouble()) * 2f - 1f; // [-1; 1] range
                float randomY = (float)(randomGenerator.NextDouble()) * 2f - 1f; // [-1; 1] range
                float randomZ = (float)(randomGenerator.NextDouble()) * 2f - 1f; // [-1; 1] range
                Vector3 normalShift = new Vector3(randomX, randomY, randomZ);
                Vector3 reflection = Mathf.Reflect(thisObj.GetForwardVector(), collisionInfo.Normal);
                reflection = Mathf.Normalize(collisionInfo.Normal + normalShift);

                // Get instance of the script and shoot
                Projectile projectile = sc.GetInstance() as Projectile;
                projectile.SetSettings(null, thisObj.m_ProjectileColorIntensity * 0.01f, thisObj.m_ProjectileSpeed * 0.25f, true);
                projectile.Shoot(projectileColor, reflection);
            }
        };

        public override void OnCreate()
        {
            base.OnCreate();
            AddCollisionBeginCallback(BeginCollisionCallback);
        }

        public void Shoot(Vector3 projectileColor, Vector3 direction)
        {
            if (m_ShootSound != null)
            {
                m_ShootSound.SetWorldPosition(WorldLocation);
                m_ShootSound.Play();
            }

            m_ProjectileColor = projectileColor;

            StaticMeshComponent sm = AddComponent<StaticMeshComponent>();
            sm.Mesh = s_ProjectileMesh;
            sm.bCastsShadows = false;

            Material smMaterial = sm.GetMaterial();
            smMaterial.EmissiveTexture = Texture2D.White;
            smMaterial.EmissiveIntensity = m_ProjectileColor;
            sm.SetMaterial(smMaterial);

            // Must be created first in order to set body type. Because it's checked once and for all when a collider is added
            RigidBodyComponent body = AddComponent<RigidBodyComponent>();
            body.SetBodyType(PhysicsBodyType.Dynamic);
            body.SetEnableGravity(true);

            SphereColliderComponent collider = AddComponent<SphereColliderComponent>();
            collider.SetRadius(1f);
            collider.SetDynamicFriction(0.01f);
            collider.SetStaticFriction(0.01f);

            body.AddForce(direction * m_ProjectileSpeed, ForceMode.Impulse);

            PointLightComponent light = AddComponent<PointLightComponent>();
            light.bCastsShadows = false;
            light.Radius = m_bExploaded ? 10f : 20f;
            light.LightColor = m_ProjectileColor * m_ProjectileColorIntensity;
        }

        public void SetSettings(Sound3D shootSound, float colorIntensity, float speed, bool bExploaded = false)
        {
            m_bExploaded = bExploaded;
            m_ShootSound = shootSound;
            m_ProjectileColorIntensity = colorIntensity;
            m_ProjectileSpeed = speed;
        }
    }

    public class Character : Entity
    {
        private Entity m_Camera;
        private Vector3 m_CameraForward;

        public float WalkSpeed = 1f;
        public float RunSpeed = 2.25f;
        public float JumpStrength = 50f;
        private float m_HeightBeforeCrouch = 1f;

        public float ProjectileColorIntensity = 0.5f;
        public float ProjectileSpeed = 10f;

        private Entity m_FloorLevel; // It's used to get location of the bottom of this entity

        Sound2D m_AmbientMusic;
        Sound3D m_ShootSound = new Sound3D(Project.GetContentPath() + "/Sounds/ball_shoot.wav", new Vector3(0f), RollOffModel.Inverse, new SoundSettings(1f));
        Sound3D m_HitSound = new Sound3D(Project.GetContentPath() + "/Sounds/debris.wav", new Vector3(0f), RollOffModel.Inverse, new SoundSettings(1f));
        Random m_RandomGenerator = new Random();

        public float AmbientMusicVolume = 1f;

        public override void OnCreate()
        {
            Input.SetCursorMode(CursorMode.Hidden);

            m_FloorLevel = GetChildrenByName("Character_FloorLevel");

            SoundSettings soundSettings = new SoundSettings(AmbientMusicVolume);
            soundSettings.bLooping = true;
            soundSettings.LoopCount = -1;

            m_AmbientMusic = new Sound2D(Project.GetContentPath() + "/Sounds/ambient.wav", soundSettings);
            m_AmbientMusic.Play();
            m_AmbientMusic.SetPosition(3000);

            //* Example of how to check entity script and get instance
            // Type type = GetComponent<ScriptComponent>().GetScriptType();
            // if (type == typeof(Character))
            // {
            //     Character thisCharacter = GetComponent<ScriptComponent>().GetInstance() as Character;
            // }
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
                if (keyPressed.Key == KeyCode.Space && keyPressed.RepeatCount == 0) // Jumping
                {
                    if (IsOnGround())
                    {
                        RigidBodyComponent rigidBody = GetComponent<RigidBodyComponent>();
                        rigidBody.AddForce(new Vector3(0, 1, 0) * JumpStrength, ForceMode.Impulse);
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
        }

        private void SpawnProjectile()
        {
            // Create an entity
            Entity entity = Entity.SpawnEntity("Projectile");
            entity.WorldLocation = m_Camera.WorldLocation + m_CameraForward * 0.1f;
            entity.WorldRotation = m_Camera.WorldRotation;
            entity.WorldScale = new Vector3(0.05f);

            // Attach `Projectile` script to it
            ScriptComponent sc = entity.AddComponent<ScriptComponent>();
            sc.SetScript(typeof(Projectile));

            // Get instance of the script and shoot
            Projectile projectile = sc.GetInstance() as Projectile;
            projectile.SetSettings(m_ShootSound, ProjectileColorIntensity, ProjectileSpeed);
            projectile.HitSound = m_HitSound;

            float randomR = (float)m_RandomGenerator.NextDouble() * 10f;
            float randomG = (float)m_RandomGenerator.NextDouble() * 10f;
            float randomB = (float)m_RandomGenerator.NextDouble() * 10f;
            Color3 projectileColor = new Color3(randomR, randomG, randomB); // Random color
            projectile.Shoot(projectileColor, m_CameraForward);
        }

        private void HandleMovement(float ts)
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

        private bool IsOnGround()
        {
            return Scene.Raycast(m_FloorLevel.WorldLocation, -GetUpVector(), 0.01f, out RaycastHit hit);
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

    public class RendererSettingsChanger : Entity
    {
        public enum SettingParamsToChange
        {
            None = 0,
            SoftShadows,
            Bloom,
            Fog,
            AO,
            VolumetricLight,
            VolumetricFog,
            Tonemapping,
            AA,
            TranslucentShadows,
            IBL
        }

        public SettingParamsToChange Setting;

        private static Vector3 s_PressedCoordOffset = new Vector3(0.015f, 0f, 0f);
        private static Color3 s_EnableColor = new Vector3(1f, 5f, 1f);
        private static Color3 s_DisableColor = new Vector3(5f, 1f, 1f);

        private TextureCube m_Cubemap;
        private Entity[] m_Buttons;
        public float DisabledColorDivider = 7.5f;
        public int NumberOfButtons = 2;

        public override void OnCreate()
        {
            m_Buttons = new Entity[NumberOfButtons];
            for (int i = 0; i < m_Buttons.Length; ++i)
            {
                m_Buttons[i] = GetChildrenByName($"Button{i}");
            }

            int enabledOption = IsSettingEnabled();
            Entity enableButton = m_Buttons[enabledOption];
            enableButton.RelativeLocation = enableButton.RelativeLocation - s_PressedCoordOffset;
            enableButton.GetComponent<TextComponent>().Color = s_EnableColor;

            for (int i = 0; i < m_Buttons.Length; i++)
            {
                if (i == enabledOption)
                    continue;

                Entity disableButton = m_Buttons[i];
                disableButton.GetComponent<TextComponent>().Color = s_DisableColor / DisabledColorDivider;
            }

            m_Cubemap = Renderer.GetCubemap();
        }

        public override void OnEvent(Event e)
        {
            if (e.GetEventType() == EventType.KeyPressed)
            {
                KeyPressedEvent mouseEvent = e as KeyPressedEvent;
                if (mouseEvent.Key == KeyCode.E)
                {
                    int enabledOption = IsSettingEnabled();
                    Vector2 screenCenter = Renderer.GetViewportSize() / 2f;

                    for (int i = 0; i < m_Buttons.Length; ++i)
                    {
                        // Dont check already enabled button
                        if (i == enabledOption)
                            continue;

                        Entity enableButton = m_Buttons[i];
                        if (enableButton.IsMouseHovered(ref screenCenter))
                        {
                            enableButton.RelativeLocation = enableButton.RelativeLocation - s_PressedCoordOffset;
                            enableButton.GetComponent<TextComponent>().Color = s_EnableColor;

                            Entity disableButton = m_Buttons[enabledOption];
                            disableButton.RelativeLocation = disableButton.RelativeLocation + s_PressedCoordOffset;
                            disableButton.GetComponent<TextComponent>().Color = s_DisableColor / DisabledColorDivider;

                            SetSettingEnabled(i);
                            break;
                        }
                    }
                }
            }
        }
    
        private void SetSettingEnabled(int option)
        {
            switch (Setting)
            {
                case SettingParamsToChange.SoftShadows:
                {
                    Renderer.bEnableSoftShadows = Convert.ToBoolean(option);
                    break;
                }
                case SettingParamsToChange.Bloom:
                {
                    BloomSettings settings = Renderer.GetBloomSettings();
                    settings.bEnabled = Convert.ToBoolean(option);
                    Renderer.SetBloomSettings(settings);
                    break;
                }
                case SettingParamsToChange.Fog:
                {
                    FogSettings settings = Renderer.GetFogSettings();
                    settings.bEnabled = Convert.ToBoolean(option);
                    Renderer.SetFogSettings(settings);
                    break;
                }
                case SettingParamsToChange.AO:
                {
                    Renderer.AO = (AmbientOcclusion)option;
                    break;
                }
                case SettingParamsToChange.VolumetricLight:
                {
                    VolumetricLightsSettings settings = Renderer.GetVolumetricLightsSettings();
                    settings.bEnabled = Convert.ToBoolean(option);
                    Renderer.SetVolumetricLightsSettings(settings);
                    break;
                }
                case SettingParamsToChange.VolumetricFog:
                {
                    VolumetricLightsSettings settings = Renderer.GetVolumetricLightsSettings();
                    settings.bFogEnabled = Convert.ToBoolean(option);
                    Renderer.SetVolumetricLightsSettings(settings);
                    break;
                }
                case SettingParamsToChange.Tonemapping:
                {
                    Renderer.Tonemapping = (TonemappingMethod)option;
                    break;
                }
                case SettingParamsToChange.AA:
                {
                    Renderer.AA = (AAMethod)option;
                    break;
                }
                case SettingParamsToChange.TranslucentShadows:
                {
                    Renderer.bTranslucentShadows = Convert.ToBoolean(option);
                    break;
                }
                case SettingParamsToChange.IBL:
                {
                    bool bEnable = Convert.ToBoolean(option);
                    if (bEnable)
                        Renderer.SetCubemap(m_Cubemap);
                    else
                        Renderer.SetCubemap(null);
                    break;
                }
            }
        }

        private int IsSettingEnabled()
        {
            switch(Setting)
            {
                case SettingParamsToChange.SoftShadows: return Convert.ToInt32(Renderer.bEnableSoftShadows);
                case SettingParamsToChange.Bloom: return Convert.ToInt32(Renderer.GetBloomSettings().bEnabled);
                case SettingParamsToChange.Fog: return Convert.ToInt32(Renderer.GetFogSettings().bEnabled);
                case SettingParamsToChange.AO: return (int)Renderer.AO;
                case SettingParamsToChange.VolumetricLight: return Convert.ToInt32(Renderer.GetVolumetricLightsSettings().bEnabled);
                case SettingParamsToChange.VolumetricFog: return Convert.ToInt32(Renderer.GetVolumetricLightsSettings().bFogEnabled);
                case SettingParamsToChange.Tonemapping: return (int)Renderer.Tonemapping;
                case SettingParamsToChange.AA: return (int)Renderer.AA;
                case SettingParamsToChange.TranslucentShadows: return Convert.ToInt32(Renderer.bTranslucentShadows);
                case SettingParamsToChange.IBL: return Renderer.GetCubemap().IsValid() ? 1 : 0;
            }

            return -1;
        }
    }

    public class TeleportToScene : Entity
    {
        public String SceneToTeleport;

        public override void OnCreate()
        {
            base.OnCreate();
        }

        public override void OnUpdate(float ts)
        {
            base.OnUpdate(ts);
        }

        public override void OnEvent(Event e)
        {
            if (e.GetEventType() == EventType.KeyPressed)
            {
                KeyPressedEvent mouseEvent = e as KeyPressedEvent;
                if (mouseEvent.Key == KeyCode.E)
                {
                    Vector2 screenCenter = Renderer.GetViewportSize() / 2f;
                    if (IsMouseHovered(ref screenCenter))
                    {
                        Scene.OpenScene(Project.GetContentPath() + "/" + SceneToTeleport);
                    }
                }
            }
        }
    }
}
