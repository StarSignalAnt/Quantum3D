class Matrix

    float32 m11; float32 m12; float32 m13; float32 m14;
    float32 m21; float32 m22; float32 m23; float32 m24;
    float32 m31; float32 m32; float32 m33; float32 m34;
    float32 m41; float32 m42; float32 m43; float32 m44;

    // ==================== Constructors ====================

    // Default: Identity Matrix
    method void Matrix()
        ToIdentity();
    end

    // Full initialization
    method void Matrix(float32 v11, float32 v12, float32 v13, float32 v14,
                       float32 v21, float32 v22, float32 v23, float32 v24,
                       float32 v31, float32 v32, float32 v33, float32 v34,
                       float32 v41, float32 v42, float32 v43, float32 v44)
        m11 = v11; m12 = v12; m13 = v13; m14 = v14;
        m21 = v21; m22 = v22; m23 = v23; m24 = v24;
        m31 = v31; m32 = v32; m33 = v33; m34 = v34;
        m41 = v41; m42 = v42; m43 = v43; m44 = v44;
    end

    // ==================== Utility Methods ====================

    method void ToIdentity()
        m11 = 1.0; m12 = 0.0; m13 = 0.0; m14 = 0.0;
        m21 = 0.0; m22 = 1.0; m23 = 0.0; m24 = 0.0;
        m31 = 0.0; m32 = 0.0; m33 = 1.0; m34 = 0.0;
        m41 = 0.0; m42 = 0.0; m43 = 0.0; m44 = 1.0;
    end

    method void ToZero()
        m11 = 0.0; m12 = 0.0; m13 = 0.0; m14 = 0.0;
        m21 = 0.0; m22 = 0.0; m23 = 0.0; m24 = 0.0;
        m31 = 0.0; m32 = 0.0; m33 = 0.0; m34 = 0.0;
        m41 = 0.0; m42 = 0.0; m43 = 0.0; m44 = 0.0;
    end

    // ==================== Transformations ====================

    method void Translate(float32 x, float32 y, float32 z)
        m14 = m11 * x + m12 * y + m13 * z + m14;
        m24 = m21 * x + m22 * y + m23 * z + m24;
        m34 = m31 * x + m32 * y + m33 * z + m34;
        m44 = m41 * x + m42 * y + m43 * z + m44;
    end

    method void Scale(float32 x, float32 y, float32 z)
        m11 = m11 * x; m12 = m12 * y; m13 = m13 * z;
        m21 = m21 * x; m22 = m22 * y; m23 = m23 * z;
        m31 = m31 * x; m32 = m32 * y; m33 = m33 * z;
        m41 = m41 * x; m42 = m42 * y; m43 = m43 * z;
    end

    // ==================== Arithmetic ====================

    method Matrix Multiply(Matrix b)
        float32 v11 = m11 * b.m11 + m12 * b.m21 + m13 * b.m31 + m14 * b.m41;
        float32 v12 = m11 * b.m12 + m12 * b.m22 + m13 * b.m32 + m14 * b.m42;
        float32 v13 = m11 * b.m13 + m12 * b.m23 + m13 * b.m33 + m14 * b.m43;
        float32 v14 = m11 * b.m14 + m12 * b.m24 + m13 * b.m34 + m14 * b.m44;

        float32 v21 = m21 * b.m11 + m22 * b.m21 + m23 * b.m31 + m24 * b.m41;
        float32 v22 = m21 * b.m12 + m22 * b.m22 + m23 * b.m32 + m24 * b.m42;
        float32 v23 = m21 * b.m13 + m22 * b.m23 + m23 * b.m33 + m24 * b.m43;
        float32 v24 = m21 * b.m14 + m22 * b.m24 + m23 * b.m34 + m24 * b.m44;

        float32 v31 = m31 * b.m11 + m32 * b.m21 + m33 * b.m31 + m34 * b.m41;
        float32 v32 = m31 * b.m12 + m32 * b.m22 + m33 * b.m32 + m34 * b.m42;
        float32 v33 = m31 * b.m13 + m32 * b.m23 + m33 * b.m33 + m34 * b.m43;
        float32 v34 = m31 * b.m14 + m32 * b.m24 + m33 * b.m34 + m34 * b.m44;

        float32 v41 = m41 * b.m11 + m42 * b.m21 + m43 * b.m31 + m44 * b.m41;
        float32 v42 = m41 * b.m12 + m42 * b.m22 + m43 * b.m32 + m44 * b.m42;
        float32 v43 = m41 * b.m13 + m42 * b.m23 + m43 * b.m33 + m44 * b.m43;
        float32 v44 = m41 * b.m14 + m42 * b.m24 + m43 * b.m34 + m44 * b.m44;

        return new Matrix(v11, v12, v13, v14, v21, v22, v23, v24, v31, v32, v33, v34, v41, v42, v43, v44);
    end

    method Vec3 Multiply(Vec3 v)
        float32 rx = m11 * v.X + m12 * v.Y + m13 * v.Z + m14;
        float32 ry = m21 * v.X + m22 * v.Y + m23 * v.Z + m24;
        float32 rz = m31 * v.X + m32 * v.Y + m33 * v.Z + m34;
        // w component is ignored but assumed 1.0 for position
        return new Vec3(rx, ry, rz);
    end

    method Matrix Plus(Matrix b)
        return new Matrix(m11+b.m11, m12+b.m12, m13+b.m13, m14+b.m14,
                          m21+b.m21, m22+b.m22, m23+b.m23, m24+b.m24,
                          m31+b.m31, m32+b.m32, m33+b.m33, m34+b.m34,
                          m41+b.m41, m42+b.m42, m43+b.m43, m44+b.m44);
    end

    method Matrix Minus(Matrix b)
        return new Matrix(m11-b.m11, m12-b.m12, m13-b.m13, m14-b.m14,
                          m21-b.m21, m22-b.m22, m23-b.m23, m24-b.m24,
                          m31-b.m31, m32-b.m32, m33-b.m33, m34-b.m34,
                          m41-b.m41, m42-b.m42, m43-b.m43, m44-b.m44);
    end

    // ==================== Utilities ====================

    method void Transpose()
        float32 t;
        t = m12; m12 = m21; m21 = t;
        t = m13; m13 = m31; m31 = t;
        t = m14; m14 = m41; m41 = t;
        t = m23; m23 = m32; m32 = t;
        t = m24; m24 = m42; m42 = t;
        t = m34; m34 = m43; m43 = t;
    end

    method void Print()
        printf("[", m11, m12, m13, m14, "]");
        printf("[", m21, m22, m23, m24, "]");
        printf("[", m31, m32, m33, m34, "]");
        printf("[", m41, m42, m43, m44, "]");
    end

end
