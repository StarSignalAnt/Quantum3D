class Vec3

    float32 X;
    float32 Y;
    float32 Z;

    // ==================== Constructors ====================

    method void Vec3()
        X = 0;
        Y = 0;
        Z = 0;
    end 

    method void Vec3(float32 x, float32 y, float32 z)
        X = x;
        Y = y;
        Z = z;
    end 

    method void Vec3(float32 value)
        X = value;
        Y = value;
        Z = value;
    end

    // ==================== Operator Overloads ====================

    // Addition: Vec3 + Vec3
    method Vec3 Plus(Vec3 right)
        return new Vec3(X + right.X, Y + right.Y, Z + right.Z);
    end 

    // Addition: Vec3 + scalar
    method Vec3 Plus(float32 scalar)
        return new Vec3(X + scalar, Y + scalar, Z + scalar);
    end

    // Subtraction: Vec3 - Vec3
    method Vec3 Minus(Vec3 right)
        return new Vec3(X - right.X, Y - right.Y, Z - right.Z);
    end

    // Subtraction: Vec3 - scalar
    method Vec3 Minus(float32 scalar)
        return new Vec3(X - scalar, Y - scalar, Z - scalar);
    end

    // Multiplication: Vec3 * Vec3 (component-wise)
    method Vec3 Multiply(Vec3 right)
        return new Vec3(X * right.X, Y * right.Y, Z * right.Z);
    end

    // Multiplication: Vec3 * scalar
    method Vec3 Multiply(float32 scalar)
        return new Vec3(X * scalar, Y * scalar, Z * scalar);
    end

    // Division: Vec3 / Vec3 (component-wise)
    method Vec3 Divide(Vec3 right)
        return new Vec3(X / right.X, Y / right.Y, Z / right.Z);
    end

    // Division: Vec3 / scalar
    method Vec3 Divide(float32 scalar)
        return new Vec3(X / scalar, Y / scalar, Z / scalar);
    end

    // Multiplication: Vec3 * Matrix
    method Vec3 Multiply(Matrix m)
        float32 rx = X * m.m11 + Y * m.m21 + Z * m.m31 + m.m41;
        float32 ry = X * m.m12 + Y * m.m22 + Z * m.m32 + m.m42;
        float32 rz = X * m.m13 + Y * m.m23 + Z * m.m33 + m.m43;
        return new Vec3(rx, ry, rz);
    end

    // ==================== Utility Methods ====================

    // Dot product
    method float32 Dot(Vec3 other)
        return X * other.X + Y * other.Y + Z * other.Z;
    end

    // Cross product
    method Vec3 Cross(Vec3 other)
        float32 cx = Y * other.Z - Z * other.Y;
        float32 cy = Z * other.X - X * other.Z;
        float32 cz = X * other.Y - Y * other.X;
        return new Vec3(cx, cy, cz);
    end

    // Squared length (faster than Length)
    method float32 LengthSquared()
        return X * X + Y * Y + Z * Z;
    end

    // Negate
    method Vec3 Negate()
        return new Vec3(-X, -Y, -Z);
    end

    // Lerp (linear interpolation)
    method Vec3 Lerp(Vec3 target, float32 t)
        float32 lx = X + (target.X - X) * t;
        float32 ly = Y + (target.Y - Y) * t;
        float32 lz = Z + (target.Z - Z) * t;
        return new Vec3(lx, ly, lz);
    end

    // ==================== Static-like Factory Methods ====================

    method Vec3 Zero()
        return new Vec3(0, 0, 0);
    end

    method Vec3 One()
        return new Vec3(1, 1, 1);
    end

    method Vec3 Up()
        return new Vec3(0, 1, 0);
    end

    method Vec3 Down()
        return new Vec3(0, -1, 0);
    end

    method Vec3 Left()
        return new Vec3(-1, 0, 0);
    end

    method Vec3 Right()
        return new Vec3(1, 0, 0);
    end

    method Vec3 Forward()
        return new Vec3(0, 0, 1);
    end

    method Vec3 Back()
        return new Vec3(0, 0, -1);
    end

    // ==================== Debug ====================

    method void Print()
        printf("Vec3(", X, ",", Y, ",", Z, ")");
    end

end 