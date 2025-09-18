
using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public static class Mathf
    {
        public static Vector3 GetForwardVector(Rotator rotator)
        {
            return GetForwardVector_Native(ref rotator);
        }

        public static Vector3 GetUpVector(Rotator rotator)
        {
            return GetUpVector_Native(ref rotator);
        }

        public static Vector3 GetRightVector(Rotator rotator)
        {
            return GetRightVector_Native(ref rotator);
        }

        // Returns degree of the angle between Velocity and rotation's forward vector
        // The range of return will be from[-180, 180].
        public static float CalculateDirection(Vector3 velocity, Rotator rotator)
        {
            return CalculateDirection_Native(ref velocity, ref rotator);
        }

        public static float Clamp(float value, float min, float max)
        {
            if (value < min)
                return min;
            else if (value > max)
                return max;
            else
                return value;
        }

        public static Vector2 Clamp(in Vector2 value, float min, float max)
        {
            Vector2 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            return result;
        }

        public static Vector2 Clamp(in Vector2 value, in Vector2 min, in Vector2 max)
        {
            Vector2 result = value;
            result.X = Clamp(result.X, min.X, max.X);
            result.Y = Clamp(result.Y, min.Y, max.Y);
            return result;
        }

        public static Vector3 Clamp(in Vector3 value, float min, float max)
        {
            Vector3 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            result.Z = Clamp(result.Z, min, max);
            return result;
        }

        public static Vector3 Clamp(in Vector3 value, in Vector3 min, in Vector3 max)
        {
            Vector3 result = value;
            result.X = Clamp(result.X, min.X, max.X);
            result.Y = Clamp(result.Y, min.Y, max.Y);
            result.Z = Clamp(result.Z, min.Z, max.Z);
            return result;
        }

        public static Vector4 Clamp(in Vector4 value, float min, float max)
        {
            Vector4 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            result.Z = Clamp(result.Z, min, max);
            result.W = Clamp(result.W, min, max);
            return result;
        }

        public static Vector4 Clamp(in Vector4 value, in Vector4 min, in Vector4 max)
        {
            Vector4 result = value;
            result.X = Clamp(result.X, min.X, max.X);
            result.Y = Clamp(result.Y, min.Y, max.Y);
            result.Z = Clamp(result.Z, min.Z, max.Z);
            result.W = Clamp(result.W, min.W, max.W);
            return result;
        }

        public static float Radians(float angle)
        {
            return ((float)Math.PI / 180f) * angle;
        }

        public static float Degrees(float radians)
        {
            return (180f / (float)Math.PI) * radians;
        }

        // Projects vector `v` on a plane with normal `n`
        public static Vector3 Project(Vector3 v, Vector3 n)
        {
            return v - Dot(v, n) * n;
        }

	    public static Quat AngleAxis(float angle, Vector3 v)
	    {
		    float s = (float)Math.Sin(angle * 0.5f);

		    return new Quat((float)Math.Cos(angle * 0.5f), v * s);
	    }

        public static Vector3 Reflect(Vector3 v, Vector3 normal)
        {
            return v - 2f * Dot(v, normal) * normal;
        }

        public static float Length(Vector2 v)
        {
            return (float)Math.Sqrt(Dot(v, v));
        }

        public static float Length(Vector3 v)
        {
            return (float)Math.Sqrt(Dot(v, v));
        }

        public static float Length(Vector4 v)
        {
            return (float)Math.Sqrt(Dot(v, v));
        }

        public static float Length2(Vector2 v)
        {
            return (float)Dot(v, v);
        }

        public static float Length2(Vector3 v)
        {
            return (float)Dot(v, v);
        }

        public static float Length2(Vector4 v)
        {
            return (float)Dot(v, v);
        }

        public static Vector2 Normalize(Vector2 v)
        {
            float len = Length(v);
            return v / len;
        }

        public static Vector3 Normalize(Vector3 v)
        {
            float len = Length(v);
            return v / len;
        }

        public static Vector4 Normalize(Vector4 v)
        {
            float len = Length(v);
            return v / len;
        }

        public static float Dot(Vector2 lhs, Vector2 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y);
        public static float Dot(Vector3 lhs, Vector3 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z);
        public static float Dot(Vector4 lhs, Vector4 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z + lhs.W * rhs.W);

        public static float Lerp(float x, float y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Vector2 Lerp(Vector2 x, Vector2 y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Vector3 Lerp(Vector3 x, Vector3 y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Vector4 Lerp(Vector4 x, Vector4 y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Quat Slerp(Quat x, Quat y, float alpha)
        {
            return SlerpQuat_Native(ref x, ref y, alpha);
        }

        public static Rotator Slerp(Rotator x, Rotator y, float alpha)
        {
            return SlerpQuat_Native(ref x.Rotation, ref y.Rotation, alpha);
        }

        // @dir. Desired forward direction.Needs to be normalized.
        public static Rotator LookAt(Vector3 dir)
        {
            return LookAt_Native(ref dir);
        }

        // Calculates LookAt rotator around Y axis
        // @dir. Desired forward direction.Needs to be normalized.
        public static Rotator LookAtY(Vector3 dir)
        {
            return LookAtY_Native(ref dir);
        }

        public static float Step(float edge, float x) => x < edge ? 0.0f : 1.0f;

        public static Vector2 Step(Vector2 edge, Vector2 x) => new Vector2(Step(edge.X, x.X), Step(edge.Y, x.Y));

        public static Vector3 Step(Vector3 edge, Vector3 x) => new Vector3(Step(edge.X, x.X), Step(edge.Y, x.Y), Step(edge.Z, x.Z));

        public static Vector4 Step(Vector4 edge, Vector4 x) => new Vector4(Step(edge.X, x.X), Step(edge.Y, x.Y), Step(edge.Z, x.Z), Step(edge.W, x.W));

        public static float Floor(float x) => (float)Math.Floor(x);

        public static Vector2 Floor(Vector2 x) => new Vector2((float)Math.Floor(x.X), (float)Math.Floor(x.Y));

        public static Vector3 Floor(Vector3 x) => new Vector3((float)Math.Floor(x.X), (float)Math.Floor(x.Y), (float)Math.Floor(x.Z));

        public static Vector4 Floor(Vector4 x) => new Vector4((float)Math.Floor(x.X), (float)Math.Floor(x.Y), (float)Math.Floor(x.Z), (float)Math.Floor(x.W));

        public static float Abs(float x) => (float)Math.Abs(x);

        public static Vector2 Abs(Vector2 x) => new Vector2((float)Math.Abs(x.X), (float)Math.Abs(x.Y));

        public static Vector3 Abs(Vector3 x) => new Vector3((float)Math.Abs(x.X), (float)Math.Abs(x.Y), (float)Math.Abs(x.Z));

        public static Vector4 Abs(Vector4 x) => new Vector4((float)Math.Abs(x.X), (float)Math.Abs(x.Y), (float)Math.Abs(x.Z), (float)Math.Abs(x.W));

        public static float Fract(float x) => x - Floor(x);

        public static Vector2 Fract(Vector2 x) => x - Floor(x);

        public static Vector3 Fract(Vector3 x) => x - Floor(x);

        public static Vector4 Fract(Vector4 x) => x - Floor(x);

        public static float Min(float a, float b) => Math.Min(a, b);

        public static Vector2 Min(Vector2 a, Vector2 b) => new Vector2(Math.Min(a.X, b.X), Math.Min(a.Y, b.Y));

        public static Vector3 Min(Vector3 a, Vector3 b) => new Vector3(Math.Min(a.X, b.X), Math.Min(a.Y, b.Y), Math.Min(a.Z, b.Z));

        public static Vector4 Min(Vector4 a, Vector4 b) => new Vector4(Math.Min(a.X, b.X), Math.Min(a.Y, b.Y), Math.Min(a.Z, b.Z), Math.Min(a.W, b.W));

        public static float Max(float a, float b) => Math.Max(a, b);

        public static Vector2 Max(Vector2 a, Vector2 b) => new Vector2(Math.Max(a.X, b.X), Math.Max(a.Y, b.Y));

        public static Vector3 Max(Vector3 a, Vector3 b) => new Vector3(Math.Max(a.X, b.X), Math.Max(a.Y, b.Y), Math.Max(a.Z, b.Z));

        public static Vector4 Max(Vector4 a, Vector4 b) => new Vector4(Math.Max(a.X, b.X), Math.Max(a.Y, b.Y), Math.Max(a.Z, b.Z), Math.Max(a.W, b.W));

        // All components are in the range [0…1], including hue.
        public static Color3 RGB2HSV(Color3 c)
        {
            Vector4 K = new Vector4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
            Vector4 p = Lerp(new Vector4(c.B, c.G, K.W, K.Z), new Vector4(c.G, c.B, K.X, K.Y), Step(c.B, c.G));
            Vector4 q = Lerp(new Vector4(p.X, p.Y, p.W, c.R), new Vector4(c.R, p.Y, p.Z, p.X), Step(p.X, c.R));

            float d = q.X - Math.Min(q.W, q.Y);
            float e = 1.0e-10f;
            return new Vector3(Math.Abs(q.Z + (q.W - q.Y) / (6.0f * d + e)), d / (q.X + e), q.X);
        }

        // All components are in the range [0…1], including hue.
        public static Color3 HSV2RGB(Color3 c)
        {
            Vector3 K = new Vector3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f);
            float Kw = 3f;
            Vector3 Kx = new Vector3(K.X);

            Vector3 fract = Fract(new Vector3(c.R) + K);
            Vector3 p = Mathf.Abs((fract * 6.0f - new Vector3(Kw)));
            return c.B * Lerp(Kx, Clamp(p - Kx, 0.0f, 1.0f), c.G);
        }

        // Input values are RGB colors. They're converted into HSV, lerped, and converted back to RGB
        public static Color3 ColorLerp(Color3 x, Color3 y, float alpha)
        {
            x = RGB2HSV(x);
            y = RGB2HSV(y);
            Vector3 result = (y - x) * alpha + x;
            return HSV2RGB(result);
        }

        // Maps `value` from range [minA; maxA] to range [minB; maxB]
        public static float MapRange(float value, float minA, float maxA, float minB, float maxB)
        {
            float alpha = Mathf.Clamp((value - minA) / (maxA - minA), 0f, 1f);
            float result = alpha * (maxB - minB) + minB;
            return result;
        }

        public static float Sign(float v) => Math.Sign(v);
        public static Vector2 Sign(Vector2 v) => new Vector2(Math.Sign(v.X), Math.Sign(v.Y));
        public static Vector3 Sign(Vector3 v) => new Vector3(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z));
        public static Vector4 Sign(Vector4 v) => new Vector4(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z), Math.Sign(v.W));


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3 GetForwardVector_Native(ref Rotator rotator);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3 GetUpVector_Native(ref Rotator rotator);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3 GetRightVector_Native(ref Rotator rotator);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float CalculateDirection_Native(ref Vector3 velocity, ref Rotator rotator);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Quat SlerpQuat_Native(ref Quat x, ref Quat y, float alpha);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Quat LookAt_Native(ref Vector3 dir);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Quat LookAtY_Native(ref Vector3 dir);
    }
}
