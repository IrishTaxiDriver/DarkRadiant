#include "Matrix4.h"

#include "Quaternion.h"

namespace
{
	/// \brief Returns \p euler angles converted from degrees to radians.
	inline Vector3 euler_degrees_to_radians(const Vector3& euler)
	{
		return Vector3(
			degrees_to_radians(euler.x()),
			degrees_to_radians(euler.y()),
			degrees_to_radians(euler.z())
		);
	}

	inline bool quaternion_component_is_90(float component)
	{
		return (fabs(component) - c_half_sqrt2f) < 0.001f;
	}
}

// Main explicit constructor (private)
Matrix4::Matrix4(float xx_, float xy_, float xz_, float xw_,
						float yx_, float yy_, float yz_, float yw_,
						float zx_, float zy_, float zz_, float zw_,
						float tx_, float ty_, float tz_, float tw_)
{
    xx() = xx_;
    xy() = xy_;
    xz() = xz_;
    xw() = xw_;
    yx() = yx_;
    yy() = yy_;
    yz() = yz_;
    yw() = yw_;
    zx() = zx_;
    zy() = zy_;
    zz() = zz_;
    zw() = zw_;
    tx() = tx_;
    ty() = ty_;
    tz() = tz_;
    tw() = tw_;
}

// Named constructors

// Identity matrix
const Matrix4& Matrix4::getIdentity()
{
    static const Matrix4 _identity(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );
    return _identity;
}

Matrix4 Matrix4::getRotation(const std::string& rotationString)
{
	float rotation[9];

	std::stringstream strm(rotationString);
    strm << std::skipws;

	for (int i = 0; i < 9; ++i)
	{
		strm >> rotation[i];
	}

	if (!strm)
	{
		// Parsing failed, fall back to the identity matrix
		return Matrix4::getIdentity();
	}

	return Matrix4::byColumns(
		rotation[0],
		rotation[1],
		rotation[2],
		0,
		rotation[3],
		rotation[4],
		rotation[5],
		0,
		rotation[6],
		rotation[7],
		rotation[8],
		0,
		0,
		0,
		0,
		1
	);
}

// Get a translation matrix for the given vector
Matrix4 Matrix4::getTranslation(const Vector3& translation)
{
    return Matrix4::byColumns(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        translation.x(), translation.y(), translation.z(), 1
    );
}

// Get a rotation from 2 vectors (named constructor)
Matrix4 Matrix4::getRotation(const Vector3& a, const Vector3& b)
{
	float angle = a.angle(b);
	Vector3 axis = b.crossProduct(a).getNormalised();

	return getRotation(axis, angle);
}

Matrix4 Matrix4::getRotation(const Vector3& axis, const float angle)
{
	// Pre-calculate the terms
	float cosPhi = cos(angle);
	float sinPhi = sin(angle);
	float oneMinusCosPhi = static_cast<float>(1) - cos(angle);
	float x = axis.x();
	float y = axis.y();
	float z = axis.z();
	return Matrix4::byColumns(
		cosPhi + oneMinusCosPhi*x*x, oneMinusCosPhi*x*y - sinPhi*z, oneMinusCosPhi*x*z + sinPhi*y, 0,
		oneMinusCosPhi*y*x + sinPhi*z, cosPhi + oneMinusCosPhi*y*y, oneMinusCosPhi*y*z - sinPhi*x, 0,
		oneMinusCosPhi*z*x - sinPhi*y, oneMinusCosPhi*z*y + sinPhi*x, cosPhi + oneMinusCosPhi*z*z, 0,
		0, 0, 0, 1
	);
}

Matrix4 Matrix4::getRotation(const Quaternion& quaternion)
{
	const float x2 = quaternion[0] + quaternion[0];
	const float y2 = quaternion[1] + quaternion[1];
	const float z2 = quaternion[2] + quaternion[2];
	const float xx = quaternion[0] * x2;
	const float xy = quaternion[0] * y2;
	const float xz = quaternion[0] * z2;
	const float yy = quaternion[1] * y2;
	const float yz = quaternion[1] * z2;
	const float zz = quaternion[2] * z2;
	const float wx = quaternion[3] * x2;
	const float wy = quaternion[3] * y2;
	const float wz = quaternion[3] * z2;

	return Matrix4::byColumns(
		1.0f - (yy + zz),
		xy + wz,
		xz - wy,
		0,
		xy - wz,
		1.0f - (xx + zz),
		yz + wx,
		0,
		xz + wy,
		yz - wx,
		1.0f - (xx + yy),
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationQuantised(const Quaternion& quaternion)
{
	if (quaternion.y() == 0 && quaternion.z() == 0 && quaternion_component_is_90(quaternion.x()) && quaternion_component_is_90(quaternion.w()))
	{
		return Matrix4::getRotationAboutXForSinCos((quaternion.x() > 0) ? 1.0f : -1.0f, 0);
	}

	if (quaternion.x() == 0 && quaternion.z() == 0 && quaternion_component_is_90(quaternion.y()) && quaternion_component_is_90(quaternion.w()))
	{
		return Matrix4::getRotationAboutYForSinCos((quaternion.y() > 0) ? 1.0f : -1.0f, 0);
	}

	if (quaternion.x() == 0	&& quaternion.y() == 0 && quaternion_component_is_90(quaternion.z()) && quaternion_component_is_90(quaternion.w()))
	{
		return Matrix4::getRotationAboutZForSinCos((quaternion.z() > 0) ? 1.0f : -1.0f, 0);
	}

	return getRotation(quaternion);
}

Matrix4 Matrix4::getRotationAboutXForSinCos(float s, float c)
{
	return Matrix4::byColumns(
		1, 0, 0, 0,
		0, c, s, 0,
		0,-s, c, 0,
		0, 0, 0, 1
	);
}

Matrix4 Matrix4::getRotationAboutYForSinCos(float s, float c)
{
	return Matrix4::byColumns(
		c, 0,-s, 0,
		0, 1, 0, 0,
		s, 0, c, 0,
		0, 0, 0, 1
	);
}

Matrix4 Matrix4::getRotationAboutZForSinCos(float s, float c)
{
	return Matrix4::byColumns(
		c, s, 0, 0,
		-s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
}

/*! \verbatim
clockwise rotation around X, Y, Z, facing along axis
 1  0   0    cy 0 -sy   cz  sz 0
 0  cx  sx   0  1  0   -sz  cz 0
 0 -sx  cx   sy 0  cy   0   0  1

rows of Z by cols of Y
 cy*cz -sy*cz+sz -sy*sz+cz
-sz*cy -sz*sy+cz

  .. or something like that..

final rotation is Z * Y * X
 cy*cz -sx*-sy*cz+cx*sz  cx*-sy*sz+sx*cz
-cy*sz  sx*sy*sz+cx*cz  -cx*-sy*sz+sx*cz
 sy    -sx*cy            cx*cy

transposed
cy.cz + 0.sz + sy.0            cy.-sz + 0 .cz +  sy.0          cy.0  + 0 .0  +   sy.1       |
sx.sy.cz + cx.sz + -sx.cy.0    sx.sy.-sz + cx.cz + -sx.cy.0    sx.sy.0  + cx.0  + -sx.cy.1  |
-cx.sy.cz + sx.sz +  cx.cy.0   -cx.sy.-sz + sx.cz +  cx.cy.0   -cx.sy.0  + 0 .0  +  cx.cy.1  |
\endverbatim */
Matrix4 Matrix4::getRotationForEulerXYZ(const Vector3& euler)
{
	float cx = cos(euler[0]);
	float sx = sin(euler[0]);
	float cy = cos(euler[1]);
	float sy = sin(euler[1]);
	float cz = cos(euler[2]);
	float sz = sin(euler[2]);

	return Matrix4::byColumns(
		cy*cz,
		cy*sz,
		-sy,
		0,
		sx*sy*cz + cx*-sz,
		sx*sy*sz + cx*cz,
		sx*cy,
		0,
		cx*sy*cz + sx*sz,
		cx*sy*sz + -sx*cz,
		cx*cy,
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationForEulerXYZDegrees(const Vector3& euler)
{
	return getRotationForEulerXYZ(euler_degrees_to_radians(euler));
}

Matrix4 Matrix4::getRotationForEulerYZX(const Vector3& euler)
{
	float cx = cos(euler[0]);
	float sx = sin(euler[0]);
	float cy = cos(euler[1]);
	float sy = sin(euler[1]);
	float cz = cos(euler[2]);
	float sz = sin(euler[2]);

	return Matrix4::byColumns(
		cy*cz,
		cx*cy*sz + sx*sy,
		sx*cy*sz - cx*sy,
		0,
		-sz,
		cx*cz,
		sx*cz,
		0,
		sy*cz,
		cx*sy*sz - sx*cy,
		sx*sy*sz + cx*cy,
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationForEulerYZXDegrees(const Vector3& euler)
{
	return getRotationForEulerYZX(euler_degrees_to_radians(euler));
}

Matrix4 Matrix4::getRotationForEulerXZY(const Vector3& euler)
{
	float cx = cos(euler[0]);
	float sx = sin(euler[0]);
	float cy = cos(euler[1]);
	float sy = sin(euler[1]);
	float cz = cos(euler[2]);
	float sz = sin(euler[2]);

	return Matrix4::byColumns(
		cy*cz,
		sz,
		-sy*cz,
		0,
		sx*sy - cx*cy*sz,
		cx*cz,
		cx*sy*sz + sx*cy,
		0,
		sx*cy*sz + cx*sy,
		-sx*cz,
		cx*cy - sx*sy*sz,
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationForEulerXZYDegrees(const Vector3& euler)
{
	return getRotationForEulerXZY(euler_degrees_to_radians(euler));
}

Matrix4 Matrix4::getRotationForEulerYXZ(const Vector3& euler)
{
	float cx = cos(euler[0]);
	float sx = sin(euler[0]);
	float cy = cos(euler[1]);
	float sy = sin(euler[1]);
	float cz = cos(euler[2]);
	float sz = sin(euler[2]);

	return Matrix4::byColumns(
		cy*cz - sx*sy*sz,
		cy*sz + sx*sy*cz,
		-cx*sy,
		0,
		-cx*sz,
		cx*cz,
		sx,
		0,
		sy*cz + sx*cy*sz,
		sy*sz - sx*cy*cz,
		cx*cy,
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationForEulerYXZDegrees(const Vector3& euler)
{
	return getRotationForEulerYXZ(euler_degrees_to_radians(euler));
}

Matrix4 Matrix4::getRotationForEulerZXY(const Vector3& euler)
{
	float cx = cos(euler[0]);
	float sx = sin(euler[0]);
	float cy = cos(euler[1]);
	float sy = sin(euler[1]);
	float cz = cos(euler[2]);
	float sz = sin(euler[2]);

	return Matrix4::byColumns(
		cy*cz + sx*sy*sz,
		cx*sz,
		sx*cy*sz - sy*cz,
		0,
		sx*sy*cz - cy*sz,
		cx*cz,
		sy*sz + sx*cy*cz,
		0,
		cx*sy,
		-sx,
		cx*cy,
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationForEulerZXYDegrees(const Vector3& euler)
{
	return getRotationForEulerZXY(euler_degrees_to_radians(euler));
}

Matrix4 Matrix4::getRotationForEulerZYX(const Vector3& euler)
{
	float cx = cos(euler[0]);
	float sx = sin(euler[0]);
	float cy = cos(euler[1]);
	float sy = sin(euler[1]);
	float cz = cos(euler[2]);
	float sz = sin(euler[2]);

	return Matrix4::byColumns(
		cy*cz,
		cx*sz + sx*sy*cz,
		sx*sz - cx*sy*cz,
		0,
		-cy*sz,
		cx*cz - sx*sy*sz,
		sx*cz + cx*sy*sz,
		0,
		sy,
		-sx*cy,
		cx*cy,
		0,
		0,
		0,
		0,
		1
	);
}

Matrix4 Matrix4::getRotationForEulerZYXDegrees(const Vector3& euler)
{
	return getRotationForEulerZYX(euler_degrees_to_radians(euler));
}

// Get a scale matrix
Matrix4 Matrix4::getScale(const Vector3& scale)
{
    return Matrix4::byColumns(
        scale[0], 0, 0, 0,
        0, scale[1], 0, 0,
        0, 0, scale[2], 0,
        0, 0, 0,        1
    );
}

// Transpose the matrix in-place
void Matrix4::transpose()
{
    std::swap(_m[1], _m[4]); // xy <=> yx
    std::swap(_m[2], _m[8]); // xz <=> zx
    std::swap(_m[3], _m[12]); // xw <=> tx
    std::swap(_m[6], _m[9]); // yz <=> zy
    std::swap(_m[7], _m[13]); // yw <=> ty
    std::swap(_m[11], _m[14]); // zw <=> tz
}

// Return transposed copy
Matrix4 Matrix4::getTransposed() const
{
    return Matrix4(
        xx(), yx(), zx(), tx(),
        xy(), yy(), zy(), ty(),
        xz(), yz(), zz(), tz(),
        xw(), yw(), zw(), tw()
    );
}

// Return affine inverse
Matrix4 Matrix4::getInverse() const
{
  Matrix4 result;

  // determinant of rotation submatrix
  float det
    = _m[0] * ( _m[5]*_m[10] - _m[9]*_m[6] )
    - _m[1] * ( _m[4]*_m[10] - _m[8]*_m[6] )
    + _m[2] * ( _m[4]*_m[9] - _m[8]*_m[5] );

  // throw exception here if (det*det < 1e-25)

  // invert rotation submatrix
  det = 1.0f / det;

  result[0] = (  (_m[5]*_m[10]- _m[6]*_m[9] )*det);
  result[1] = (- (_m[1]*_m[10]- _m[2]*_m[9] )*det);
  result[2] = (  (_m[1]*_m[6] - _m[2]*_m[5] )*det);
  result[3] = 0;
  result[4] = (- (_m[4]*_m[10]- _m[6]*_m[8] )*det);
  result[5] = (  (_m[0]*_m[10]- _m[2]*_m[8] )*det);
  result[6] = (- (_m[0]*_m[6] - _m[2]*_m[4] )*det);
  result[7] = 0;
  result[8] = (  (_m[4]*_m[9] - _m[5]*_m[8] )*det);
  result[9] = (- (_m[0]*_m[9] - _m[1]*_m[8] )*det);
  result[10]= (  (_m[0]*_m[5] - _m[1]*_m[4] )*det);
  result[11] = 0;

  // multiply translation part by rotation
  result[12] = - (_m[12] * result[0] +
    _m[13] * result[4] +
    _m[14] * result[8]);
  result[13] = - (_m[12] * result[1] +
    _m[13] * result[5] +
    _m[14] * result[9]);
  result[14] = - (_m[12] * result[2] +
    _m[13] * result[6] +
    _m[14] * result[10]);
  result[15] = 1;

  return result;
}

Matrix4 Matrix4::getFullInverse() const
{
	// The inverse is generated through the adjugate matrix

	// 2x2 minors (re-usable for the determinant)
	float minor01 = zz() * tw() - zw() * tz();
	float minor02 = zy() * tw() - zw() * ty();
	float minor03 = zx() * tw() - zw() * tx();
	float minor04 = zy() * tz() - zz() * ty();
	float minor05 = zx() * tz() - zz() * tx();
	float minor06 = zx() * ty() - zy() * tx();

	// 2x2 minors (not usable for the determinant)
	float minor07 = yz() * tw() - yw() * tz();
	float minor08 = yy() * tw() - yw() * ty();
	float minor09 = yy() * tz() - yz() * ty();
	float minor10 = yx() * tw() - yw() * tx();
	float minor11 = yx() * tz() - yz() * tx();
	float minor12 = yx() * ty() - yy() * tx();
	float minor13 = yz() * zw() - yw() * zz();
	float minor14 = yy() * zw() - yw() * zy();
	float minor15 = yy() * zz() - yz() * zy();
	float minor16 = yx() * zw() - yw() * zx();
	float minor17 = yx() * zz() - yz() * zx();
	float minor18 = yx() * zy() - yy() * zx();

	// 3x3 minors (re-usable for the determinant)
	float minor3x3_11 = yy() * minor01 - yz() * minor02 + yw() * minor04;
	float minor3x3_21 = yx() * minor01 - yz() * minor03 + yw() * minor05;
	float minor3x3_31 = yx() * minor02 - yy() * minor03 + yw() * minor06;
	float minor3x3_41 = yx() * minor04 - yy() * minor05 + yz() * minor06;
	
	// 3x3 minors (not usable for the determinant)
	float minor3x3_12 = xy() * minor01 - xz() * minor02 + xw() * minor04;
	float minor3x3_22 = xx() * minor01 - xz() * minor03 + xw() * minor05;
	float minor3x3_32 = xx() * minor02 - xy() * minor03 + xw() * minor06;
	float minor3x3_42 = xx() * minor04 - xy() * minor05 + xz() * minor06;
	
	float minor3x3_13 = xy() * minor07 - xz() * minor08 + xw() * minor09;
	float minor3x3_23 = xx() * minor07 - xz() * minor10 + xw() * minor11;
	float minor3x3_33 = xx() * minor08 - xy() * minor10 + xw() * minor12;
	float minor3x3_43 = xx() * minor09 - xy() * minor11 + xz() * minor12;

	float minor3x3_14 = xy() * minor13 - xz() * minor14 + xw() * minor15;
	float minor3x3_24 = xx() * minor13 - xz() * minor16 + xw() * minor17;
	float minor3x3_34 = xx() * minor14 - xy() * minor16 + xw() * minor18;
	float minor3x3_44 = xx() * minor15 - xy() * minor17 + xz() * minor18;

	float determinant = xx() * minor3x3_11 - xy() * minor3x3_21 + xz() * minor3x3_31 - xw() * minor3x3_41;
	float invDet = 1.0f / determinant;

	return Matrix4::byColumns(
		+minor3x3_11 * invDet, -minor3x3_12 * invDet, +minor3x3_13 * invDet, -minor3x3_14 * invDet,
		-minor3x3_21 * invDet, +minor3x3_22 * invDet, -minor3x3_23 * invDet, +minor3x3_24 * invDet,
		+minor3x3_31 * invDet, -minor3x3_32 * invDet, +minor3x3_33 * invDet, -minor3x3_34 * invDet,
		-minor3x3_41 * invDet, +minor3x3_42 * invDet, -minor3x3_43 * invDet, +minor3x3_44 * invDet
	);
}

// Transform a plane
Plane3 Matrix4::transform(const Plane3& plane) const
{
    Plane3 transformed;
    transformed.normal().x() = _m[0] * plane.normal().x() + _m[4] * plane.normal().y() + _m[8] * plane.normal().z();
    transformed.normal().y() = _m[1] * plane.normal().x() + _m[5] * plane.normal().y() + _m[9] * plane.normal().z();
    transformed.normal().z() = _m[2] * plane.normal().x() + _m[6] * plane.normal().y() + _m[10] * plane.normal().z();
    transformed.dist() = -(	(-plane.dist() * transformed.normal().x() + _m[12]) * transformed.normal().x() +
                        (-plane.dist() * transformed.normal().y() + _m[13]) * transformed.normal().y() +
                        (-plane.dist() * transformed.normal().z() + _m[14]) * transformed.normal().z());
    return transformed;
}

// Inverse transform a plane
Plane3 Matrix4::inverseTransform(const Plane3& plane) const
{
    return Plane3(
        _m[ 0] * plane.normal().x() + _m[ 1] * plane.normal().y() + _m[ 2] * plane.normal().z() + _m[ 3] * plane.dist(),
        _m[ 4] * plane.normal().x() + _m[ 5] * plane.normal().y() + _m[ 6] * plane.normal().z() + _m[ 7] * plane.dist(),
        _m[ 8] * plane.normal().x() + _m[ 9] * plane.normal().y() + _m[10] * plane.normal().z() + _m[11] * plane.dist(),
        _m[12] * plane.normal().x() + _m[13] * plane.normal().y() + _m[14] * plane.normal().z() + _m[15] * plane.dist()
    );
}

// Multiply by another matrix, in-place
void Matrix4::multiplyBy(const Matrix4& other)
{
    *this = getMultipliedBy(other);
}

// Add a translation component
void Matrix4::translateBy(const Vector3& translation)
{
    multiplyBy(getTranslation(translation));
}

// Add a scale component
void Matrix4::scaleBy(const Vector3& scale)
{
    multiplyBy(getScale(scale));
}

namespace
{

class Vector4ClipLT
{
public:
  static bool compare(const Vector4& self, std::size_t index)
  {
    return self[index] < self[3];
  }

  static float scale(const Vector4& self, const Vector4& other, std::size_t index)
  {
    return (self[index] - self[3]) / (other[3] - other[index]);
  }
};

class Vector4ClipGT
{
public:
  static bool compare(const Vector4& self, std::size_t index)
  {
    return self[index] > -self[3];
  }

  static float scale(const Vector4& self, const Vector4& other, std::size_t index)
  {
    return (self[index] + self[3]) / (-other[3] - other[index]);
  }

};

template<typename ClipPlane>
class Vector4ClipPolygon
{
public:
  typedef Vector4* iterator;
  typedef const Vector4* const_iterator;

  static std::size_t apply(const_iterator first, const_iterator last, iterator out, std::size_t index)
  {
    const_iterator next = first, i = last - 1;
    iterator tmp(out);
    bool b0 = ClipPlane::compare(*i, index);
    while(next != last)
    {
      bool b1 = ClipPlane::compare(*next, index);
      if(b0 ^ b1)
      {
        *out = *next - *i;

        float scale = ClipPlane::scale(*i, *out, index);

        (*out)[0] = (*i)[0] + scale*((*out)[0]);
        (*out)[1] = (*i)[1] + scale*((*out)[1]);
        (*out)[2] = (*i)[2] + scale*((*out)[2]);
        (*out)[3] = (*i)[3] + scale*((*out)[3]);

        ++out;
      }

      if(b1)
      {
        *out = *next;
        ++out;
      }

      i = next;
      ++next;
      b0 = b1;
    }

    return out - tmp;
  }
};

#define CLIP_X_LT_W(p) (Vector4ClipLT::compare(p, 0))
#define CLIP_X_GT_W(p) (Vector4ClipGT::compare(p, 0))
#define CLIP_Y_LT_W(p) (Vector4ClipLT::compare(p, 1))
#define CLIP_Y_GT_W(p) (Vector4ClipGT::compare(p, 1))
#define CLIP_Z_LT_W(p) (Vector4ClipLT::compare(p, 2))
#define CLIP_Z_GT_W(p) (Vector4ClipGT::compare(p, 2))

	inline ClipResult homogenous_clip_point(const Vector4& clipped)
	{
		ClipResult result = c_CLIP_FAIL;

		if (CLIP_X_LT_W(clipped)) result &= ~c_CLIP_LT_X; // X < W
		if (CLIP_X_GT_W(clipped)) result &= ~c_CLIP_GT_X; // X > -W
		if (CLIP_Y_LT_W(clipped)) result &= ~c_CLIP_LT_Y; // Y < W
		if (CLIP_Y_GT_W(clipped)) result &= ~c_CLIP_GT_Y; // Y > -W
		if (CLIP_Z_LT_W(clipped)) result &= ~c_CLIP_LT_Z; // Z < W
		if (CLIP_Z_GT_W(clipped)) result &= ~c_CLIP_GT_Z; // Z > -W

		return result;
	}

	inline std::size_t homogenous_clip_line(Vector4 clipped[2])
	{
		const Vector4& p0 = clipped[0];
		const Vector4& p1 = clipped[1];

		// early out
		{
			ClipResult mask0 = homogenous_clip_point(clipped[0]);
			ClipResult mask1 = homogenous_clip_point(clipped[1]);

			if ((mask0 | mask1) == c_CLIP_PASS) // both points passed all planes
			{
				return 2;
			}

			if (mask0 & mask1) // both points failed any one plane
			{
				return 0;
			}
		}

		{
			const bool index = CLIP_X_LT_W(p0);

			if (index ^ CLIP_X_LT_W(p1))
			{
				Vector4 clip(p1 - p0);

				float scale = (p0[0] - p0[3]) / (clip[3] - clip[0]);

				clip[0] = p0[0] + scale * clip[0];
				clip[1] = p0[1] + scale * clip[1];
				clip[2] = p0[2] + scale * clip[2];
				clip[3] = p0[3] + scale * clip[3];

				clipped[index] = clip;
			}
			else if(index == 0)
			{
				return 0;
			}
		}

		{
			const bool index = CLIP_X_GT_W(p0);

			if (index ^ CLIP_X_GT_W(p1))
			{
				Vector4 clip(p1 - p0);

				float scale = (p0[0] + p0[3]) / (-clip[3] - clip[0]);

				clip[0] = p0[0] + scale * clip[0];
				clip[1] = p0[1] + scale * clip[1];
				clip[2] = p0[2] + scale * clip[2];
				clip[3] = p0[3] + scale * clip[3];

				clipped[index] = clip;
			}
			else if(index == 0)
			{
				return 0;
			}
		}

		{
			const bool index = CLIP_Y_LT_W(p0);

			if (index ^ CLIP_Y_LT_W(p1))
			{
				Vector4 clip(p1 - p0);

				float scale = (p0[1] - p0[3]) / (clip[3] - clip[1]);

				clip[0] = p0[0] + scale * clip[0];
				clip[1] = p0[1] + scale * clip[1];
				clip[2] = p0[2] + scale * clip[2];
				clip[3] = p0[3] + scale * clip[3];

				clipped[index] = clip;
			}
			else if (index == 0)
			{
				return 0;
			}
		}

		{
			const bool index = CLIP_Y_GT_W(p0);

			if (index ^ CLIP_Y_GT_W(p1))
			{
				Vector4 clip(p1 - p0);

				float scale = (p0[1] + p0[3]) / (-clip[3] - clip[1]);

				clip[0] = p0[0] + scale * clip[0];
				clip[1] = p0[1] + scale * clip[1];
				clip[2] = p0[2] + scale * clip[2];
				clip[3] = p0[3] + scale * clip[3];

				clipped[index] = clip;
			}
			else if (index == 0)
			{
				return 0;
			}
		}

		{
			const bool index = CLIP_Z_LT_W(p0);

			if (index ^ CLIP_Z_LT_W(p1))
			{
				Vector4 clip(p1 - p0);

				float scale = (p0[2] - p0[3]) / (clip[3] - clip[2]);

				clip[0] = p0[0] + scale * clip[0];
				clip[1] = p0[1] + scale * clip[1];
				clip[2] = p0[2] + scale * clip[2];
				clip[3] = p0[3] + scale * clip[3];

				clipped[index] = clip;
			}
			else if (index == 0)
			{
				return 0;
			}
		}

		{
			const bool index = CLIP_Z_GT_W(p0);

			if (index ^ CLIP_Z_GT_W(p1))
			{
				Vector4 clip(p1 - p0);

				float scale = (p0[2] + p0[3]) / (-clip[3] - clip[2]);

				clip[0] = p0[0] + scale * clip[0];
				clip[1] = p0[1] + scale * clip[1];
				clip[2] = p0[2] + scale * clip[2];
				clip[3] = p0[3] + scale * clip[3];

				clipped[index] = clip;
			}
			else if (index == 0)
			{
				return 0;
			}
		}

		return 2;
	}

	inline std::size_t homogenous_clip_triangle(Vector4 clipped[9])
	{
		Vector4 buffer[9];
		std::size_t count = 3;

		count = Vector4ClipPolygon< Vector4ClipLT >::apply(clipped, clipped + count, buffer, 0);
		count = Vector4ClipPolygon< Vector4ClipGT >::apply(buffer, buffer + count, clipped, 0);
		count = Vector4ClipPolygon< Vector4ClipLT >::apply(clipped, clipped + count, buffer, 1);
		count = Vector4ClipPolygon< Vector4ClipGT >::apply(buffer, buffer + count, clipped, 1);
		count = Vector4ClipPolygon< Vector4ClipLT >::apply(clipped, clipped + count, buffer, 2);

		return Vector4ClipPolygon< Vector4ClipGT >::apply(buffer, buffer + count, clipped, 2);
	}

} // namespace

ClipResult Matrix4::clipPoint(const Vector3& point, Vector4& clipped) const
{
	clipped[0] = point[0];
	clipped[1] = point[1];
	clipped[2] = point[2];
	clipped[3] = 1;

	clipped = transform(clipped);

	return homogenous_clip_point(clipped);
}

std::size_t Matrix4::clipTriangle(const Vector3& p0, const Vector3& p1, const Vector3& p2, Vector4 clipped[9]) const
{
	clipped[0][0] = p0[0];
	clipped[0][1] = p0[1];
	clipped[0][2] = p0[2];
	clipped[0][3] = 1;
	clipped[1][0] = p1[0];
	clipped[1][1] = p1[1];
	clipped[1][2] = p1[2];
	clipped[1][3] = 1;
	clipped[2][0] = p2[0];
	clipped[2][1] = p2[1];
	clipped[2][2] = p2[2];
	clipped[2][3] = 1;

	clipped[0] = transform(clipped[0]);
	clipped[1] = transform(clipped[1]);
	clipped[2] = transform(clipped[2]);

	return homogenous_clip_triangle(clipped);
}

std::size_t Matrix4::clipLine(const Vector3& p0, const Vector3& p1, Vector4 clipped[2]) const
{
	clipped[0][0] = p0[0];
	clipped[0][1] = p0[1];
	clipped[0][2] = p0[2];
	clipped[0][3] = 1;
	clipped[1][0] = p1[0];
	clipped[1][1] = p1[1];
	clipped[1][2] = p1[2];
	clipped[1][3] = 1;

	clipped[0] = transform(clipped[0]);
	clipped[1] = transform(clipped[1]);

	return homogenous_clip_line(clipped);
}
