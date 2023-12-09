C# Math
=======

Math API has some structs and functions that might help you with the development.

.. note::

	It's not complete at all and misses a bunch of functionality, but anyways it's here :)

`Vector2` struct
----------------
It is a utility struct representing 2D vector.

.. code-block:: csharp

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2 : IEquatable<Vector2>
    {
        public float X;
        public float Y;

        // X = Y = scalar
        public Vector2(float scalar);

        // X = x, Y = y
        public Vector2(float x, float y);

        public static Vector2 operator+ (Vector2 left, Vector2 right);
        public static Vector2 operator+ (Vector2 left, float scalar);
        public static Vector2 operator+ (float scalar, Vector2 right);

        public static Vector2 operator- (Vector2 left, Vector2 right);
        public static Vector2 operator- (Vector2 left, float scalar);
        public static Vector2 operator- (float scalar, Vector2 right);
        public static Vector2 operator- (Vector2 vector);

        public static Vector2 operator* (Vector2 left, Vector2 right);
        public static Vector2 operator* (Vector2 left, float scalar);
        public static Vector2 operator* (float scalar, Vector2 right);

        public static Vector2 operator/ (Vector2 left, Vector2 right);
        public static Vector2 operator/ (Vector2 left, float scalar);
        public static Vector2 operator/ (float scalar, Vector2 right);

        public override bool Equals(object obj) => obj is Vector2 other && this.Equals(other);
        public bool Equals(Vector2 right) => X == right.X && Y == right.Y;

        public static bool operator== (Vector2 left, Vector2 right) => left.Equals(right);
        public static bool operator!= (Vector2 left, Vector2 right) => !(left == right);

        public override string ToString()
        {
            return "Vector2[" + X + ", " + Y + "]";
        }

        public override int GetHashCode() => (X, Y).GetHashCode();
    }

`Vector3` struct
----------------
It is a utility struct representing 3D vector. It has the same functionality as ``Vector2D`` but additionally, it has a construct that takes ``Color3`` vector and an implicit cast operator.

``Vector3`` and ``Color3`` structs are identical. They can be used in your scripts so that the editor can distinguish them and display an appropriate way of editing it. For example, for a ``Color3`` value, there will be a color picker.

.. code-block:: csharp

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3 : IEquatable<Vector3>
    {
        public float X;
        public float Y;
        public float Z;

        public Vector3 (Color3 value)
        {
            X = value.R;
            Y = value.G;
            Z = value.B;
        }

        public static implicit operator Vector3(Color3 value);

        ...
    }

`Vector4` struct
----------------
It is a utility struct representing 4D vector. It has the same functionality as ``Vector3D`` but instead of ``Color3``, it works with ``Color4``.

``Vector4`` and ``Color4`` structs are identical. They can be used so that the editor can distinguish them and display an appropriate way of editing it. For example, for a ``Color4`` value, there will be a color-alpha picker.

`Quat` struct
-------------
It represents a quaternion that can be used for rotations.

.. code-block:: csharp

    [StructLayout(LayoutKind.Sequential)]
    public struct Quat : IEquatable<Quat>
    {
        public float W;
        public float X;
        public float Y;
        public float Z;

        // W = w, X = x, Y = y, Z = z
        public Quat(float w, float x, float y, float z);

        // W = w, X = v.X, Y = v.Y, Z = v.Z
        public Quat(float w, Vector3 v);

        public static Quat Conjugate(Quat val)
        {
            return new Quat(val.W, -val.X, -val.Y, -val.Z);
        }

        public static float Dot(Quat left, Quat right)
        {
            return left.X * right.X + left.Y * right.Y + left.Z * right.Z + left.W * right.W;
        }

        public Quat Inverse()
        {
            return Conjugate(this) / Dot(this, this);
        }

        public float Lenght()
        {
            return (float)Math.Sqrt(Quat.Dot(this, this));
        }

        // Turns it into a unit length quat
        public void Normalize();

        public static Quat Unit()
        {
            return new Quat(1f, 0f, 0f, 0f);
        }

        public static Quat operator* (Quat left, Quat right)
        {
            Quat p = left;
            Quat q = right;
            Quat result = new Quat();

            result.W = p.W * q.W - p.X * q.X - p.Y * q.Y - p.Z * q.Z;
            result.X = p.W * q.X + p.X * q.W + p.Y * q.Z - p.Z * q.Y;
            result.Y = p.W * q.Y + p.Y * q.W + p.Z * q.X - p.X * q.Z;
            result.Z = p.W * q.Z + p.Z * q.W + p.X * q.Y - p.Y * q.X;

            return result;
        }

        public static Quat operator/ (Quat left, float scalar);

        public override bool Equals(object obj) => obj is Quat other && this.Equals(other);

        public bool Equals(Quat right) => X == right.X && Y == right.Y && Z == right.Z && W == right.W;

        public static bool operator== (Quat left, Quat right) => left.Equals(right);
        public static bool operator!= (Quat left, Quat right) => !(left == right);

        public override string ToString()
        {
            return "Quat[" + X + ", " + Y + ", " + Z + ", " + W + "]";
        }

        public override int GetHashCode() => (X, Y, Z, W).GetHashCode();
    }

`Rotator` struct
----------------
Currently, it just contains a ``Quat`` member

.. code-block:: csharp

    [StructLayout(LayoutKind.Sequential)]
    public struct Rotator
    {
        public Quat Rotation;
    }

`Transform` struct
------------------
It is used to represent an objects transformation

.. code-block:: csharp

    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vector3 Location;
        public Rotator Rotation;
        public Vector3 Scale;
    }

`Math` class
------------
It is a static class that contains utility functions.

.. code-block:: csharp

    public static class Mathf
    {
        public static float Clamp(float value, float min, float max);

        public static Vector2 Clamp(in Vector2 value, float min, float max);
        public static Vector2 Clamp(in Vector2 value, in Vector2 min, in Vector2 max);

        public static Vector3 Clamp(in Vector3 value, float min, float max);
        public static Vector3 Clamp(in Vector3 value, in Vector3 min, in Vector3 max);

        public static Vector4 Clamp(in Vector4 value, float min, float max);
        public static Vector4 Clamp(in Vector4 value, in Vector4 min, in Vector4 max);
        
        // Converts angles to radians
        public static float Radians(float angle);

        // It can be used to get a quaternion that represent a rotation around an axis
        public static Quat AngleAxis(float angle, Vector3 v);

        public static Vector3 Reflect(Vector3 v, Vector3 normal);
        
        public static float Length(Vector2 v);
        public static float Length(Vector3 v);
        public static float Length(Vector4 v);
        
        public static Vector2 Normalize(Vector2 v);
        public static Vector3 Normalize(Vector3 v);
        public static Vector4 Normalize(Vector4 v);

        public static float Dot(Vector2 lhs, Vector2 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y);
        public static float Dot(Vector3 lhs, Vector3 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z);
        public static float Dot(Vector4 lhs, Vector4 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z + lhs.W * rhs.W);

        public static float Lerp(float x, float y, float alpha);
        public static Vector2 Lerp(Vector2 x, Vector2 y, float alpha);
        public static Vector3 Lerp(Vector3 x, Vector3 y, float alpha);
        public static Vector4 Lerp(Vector4 x, Vector4 y, float alpha);

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

        // All components are in the range [0; 1], including hue.
        public static Color3 RGB2HSV(Color3 c);

        // All components are in the range [0; 1], including hue.
        public static Color3 HSV2RGB(Color3 c);

        // Input values are RGB colors. They're converted into HSV, lerped, and converted back to RGB
        public static Color3 ColorLerp(Color3 x, Color3 y, float alpha);

        // Maps `value` from range [minA; maxA] to range [minB; maxB]
        public static float MapRange(float value, float minA, float maxA, float minB, float maxB);

        public static float Sign(float v) => Math.Sign(v);
        public static Vector2 Sign(Vector2 v) => new Vector2(Math.Sign(v.X), Math.Sign(v.Y));
        public static Vector3 Sign(Vector3 v) => new Vector3(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z));
        public static Vector4 Sign(Vector4 v) => new Vector4(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z), Math.Sign(v.W));
    }
