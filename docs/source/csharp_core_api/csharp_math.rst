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

        public static Vector2 Sign(Vector2 v) => new Vector2(Math.Sign(v.X), Math.Sign(v.Y));
        public static Vector3 Sign(Vector3 v) => new Vector3(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z));
        public static Vector4 Sign(Vector4 v) => new Vector4(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z), Math.Sign(v.W));
    }
