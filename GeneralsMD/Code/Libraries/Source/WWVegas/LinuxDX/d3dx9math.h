/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

/////////////////////////////////////////////////////////////////////////
// Linux twin of the D3DX9 math library (d3dx9math.h).
//
// Provides D3DXVECTOR3, D3DXVECTOR4, D3DXMATRIX with row-major D3DX
// conventions and complete matrix / vector transformations.
/////////////////////////////////////////////////////////////////////////
#ifndef WWVEGAS_LINUXDX_D3DX9MATH_H
#define WWVEGAS_LINUXDX_D3DX9MATH_H

#ifndef D3DX9MATH_H
#define D3DX9MATH_H 1
#endif

#include <cmath>
#include <cstring>
#include <cstdint>

#ifndef WINAPI
#define WINAPI
#endif

#ifndef D3DX_PI
#define D3DX_PI 3.14159265358979323846f
#endif

#ifndef _D3DVECTOR_DEFINED
#define _D3DVECTOR_DEFINED
struct D3DVECTOR {
    float x;
    float y;
    float z;
};
#endif

#ifndef _D3DMATRIX_DEFINED
#define _D3DMATRIX_DEFINED
struct D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
};
#endif

#ifndef D3DXVECTOR3_DEFINED
#define D3DXVECTOR3_DEFINED
struct D3DXVECTOR3 : public D3DVECTOR
{
    D3DXVECTOR3() : D3DVECTOR{0.0f, 0.0f, 0.0f} {}
    D3DXVECTOR3(float x_, float y_, float z_) : D3DVECTOR{x_, y_, z_} {}
    operator float* () { return &x; }
    operator const float* () const { return &x; }
};
#endif

#ifndef D3DXVECTOR4_DEFINED
#define D3DXVECTOR4_DEFINED
struct D3DXVECTOR4
{
    float x, y, z, w;
    D3DXVECTOR4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    D3DXVECTOR4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    operator float* () { return &x; }
    operator const float* () const { return &x; }
};
#endif

struct D3DXMATRIX;

typedef long HRESULT;
typedef float FLOAT;
typedef unsigned int UINT;

typedef HRESULT (WINAPI * D3DXMatrixInverseFunction)(D3DXMATRIX * out, float * determinant,
	const D3DXMATRIX * matrix);
typedef D3DXMATRIX * (WINAPI * D3DXMatrixBinaryFunction)(D3DXMATRIX * out,
	const D3DXMATRIX * left, const D3DXMATRIX * right);
typedef D3DXMATRIX * (WINAPI * D3DXMatrixUnaryFunction)(D3DXMATRIX * out,
	const D3DXMATRIX * matrix);
typedef D3DXMATRIX * (WINAPI * D3DXMatrixTripleFunction)(D3DXMATRIX * out,
	float first, float second, float third);
typedef D3DXMATRIX * (WINAPI * D3DXMatrixAngleFunction)(D3DXMATRIX * out, float angle);
typedef D3DXVECTOR4 * (WINAPI * D3DXVec4TransformFunction)(D3DXVECTOR4 * out,
	const D3DXVECTOR4 * vector, const D3DXMATRIX * matrix);
typedef D3DXVECTOR4 * (WINAPI * D3DXVec3TransformFunction)(D3DXVECTOR4 * out,
	const D3DXVECTOR3 * vector, const D3DXMATRIX * matrix);

extern D3DXMatrixInverseFunction	D3DXMatrixInverse;
extern D3DXMatrixBinaryFunction		D3DXMatrixMultiply;
extern D3DXMatrixUnaryFunction		D3DXMatrixTranspose;
extern D3DXMatrixTripleFunction		D3DXMatrixScaling;
extern D3DXMatrixTripleFunction		D3DXMatrixTranslation;
extern D3DXMatrixAngleFunction		D3DXMatrixRotationZ;
extern D3DXVec4TransformFunction	D3DXVec4Transform;
extern D3DXVec3TransformFunction	D3DXVec3Transform;

#ifndef D3DXMATRIX_DEFINED
#define D3DXMATRIX_DEFINED
struct D3DXMATRIX : public D3DMATRIX
{
    D3DXMATRIX()
    {
        _11 = _12 = _13 = _14 = 0.0f;
        _21 = _22 = _23 = _24 = 0.0f;
        _31 = _32 = _33 = _34 = 0.0f;
        _41 = _42 = _43 = _44 = 0.0f;
    }
    D3DXMATRIX(float m11, float m12, float m13, float m14,
               float m21, float m22, float m23, float m24,
               float m31, float m32, float m33, float m34,
               float m41, float m42, float m43, float m44)
    {
        _11 = m11; _12 = m12; _13 = m13; _14 = m14;
        _21 = m21; _22 = m22; _23 = m23; _24 = m24;
        _31 = m31; _32 = m32; _33 = m33; _34 = m34;
        _41 = m41; _42 = m42; _43 = m43; _44 = m44;
    }

    operator float* () { return &_11; }
    operator const float* () const { return &_11; }

    float& operator()(unsigned int row, unsigned int col) { return m[row][col]; }
    float  operator()(unsigned int row, unsigned int col) const { return m[row][col]; }

    D3DXMATRIX operator*(const D3DXMATRIX& right) const
    {
        D3DXMATRIX result;
        if (D3DXMatrixMultiply) {
            D3DXMatrixMultiply(&result, this, &right);
        } else {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    result.m[i][j] = m[i][0] * right.m[0][j] +
                                     m[i][1] * right.m[1][j] +
                                     m[i][2] * right.m[2][j] +
                                     m[i][3] * right.m[3][j];
                }
            }
        }
        return result;
    }

    D3DXMATRIX& operator*=(const D3DXMATRIX& right)
    {
        if (D3DXMatrixMultiply) {
            D3DXMatrixMultiply(this, this, &right);
        } else {
            *this = *this * right;
        }
        return *this;
    }
};
#endif

inline D3DXMATRIX *D3DXMatrixIdentity(D3DXMATRIX *pOut)
{
    if (!pOut) return nullptr;
    pOut->m[0][1] = pOut->m[0][2] = pOut->m[0][3] =
    pOut->m[1][0] = pOut->m[1][2] = pOut->m[1][3] =
    pOut->m[2][0] = pOut->m[2][1] = pOut->m[2][3] =
    pOut->m[3][0] = pOut->m[3][1] = pOut->m[3][2] = 0.0f;
    pOut->m[0][0] = pOut->m[1][1] = pOut->m[2][2] = pOut->m[3][3] = 1.0f;
    return pOut;
}

inline float D3DXVec4Dot(const D3DXVECTOR4 *pV1, const D3DXVECTOR4 *pV2)
{
    if (!pV1 || !pV2) return 0.0f;
    return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z + pV1->w * pV2->w;
}

#endif // WWVEGAS_LINUXDX_D3DX9MATH_H
