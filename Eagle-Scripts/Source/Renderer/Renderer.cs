using System.Runtime.CompilerServices;

namespace Eagle
{
    public enum FogEquation
    {
        Linear = 0,
        Exponential = 1,
        Exponential2 = 2, // Square exponential
    }

    public static class Renderer
    {
        public static Vector3 FogColor
        {
            get { GetFogColor_Native(out Vector3 result); return result; }
            set { SetFogColor_Native(ref value); }
        }

        public static float FogMinDistance
        {
            get { return GetFogMinDistance_Native(); }
            set { SetFogMinDistance_Native(value); }
        }

        public static float FogMaxDistance
        {
            get { return GetFogMaxDistance_Native(); }
            set { SetFogMaxDistance_Native(value); }
        }

        public static float FogDensity
        {
            get { return GetFogDensity_Native(); }
            set { SetFogDensity_Native(value); }
        }

        public static FogEquation FogEquation
        {
            get { return GetFogEquation_Native(); }
            set { SetFogEquation_Native(value); }
        }

        public static bool bFogEnabled
        {
            get { return GetFogEnabled_Native(); }
            set { SetFogEnabled_Native(value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetFogColor_Native(out Vector3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogColor_Native(ref Vector3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetFogMinDistance_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogMinDistance_Native(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetFogMaxDistance_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogMaxDistance_Native(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetFogDensity_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogDensity_Native(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern FogEquation GetFogEquation_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogEquation_Native(FogEquation value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetFogEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogEnabled_Native(bool value);
    }
}
