#pragma once

#include "texturelib.h"
#include "Winding.h"
#include "math/AABB.h"
#include "iregistry.h"
#include "BrushPrimitTexDef.h"
#include "selection/algorithm/Shader.h"

/* greebo: A texture projection contains the texture definition
 * as well as the brush primitive texture definition.
 */
class TextureProjection
{
public:
    TexDef m_texdef;
    BrushPrimitTexDef m_brushprimit_texdef;

    /**
     * \brief
     * Construct a default TextureProjection.
     *
     * The projection is initialised with the default texture scale from the
     * registry.
     */
    TextureProjection();

    TextureProjection(
        const TexDef& texdef,
        const BrushPrimitTexDef& brushprimit_texdef
    ) :
        m_texdef(texdef),
        m_brushprimit_texdef(brushprimit_texdef)
    {}

    // Copy Constructor
    TextureProjection(const TextureProjection& other) :
        m_texdef(other.m_texdef),
        m_brushprimit_texdef(other.m_brushprimit_texdef)
    {}

    void assign(const TextureProjection& other);

    void setTransform(float width, float height, const Matrix4& transform);
    Matrix4 getTransform() const;

	// s and t are texture coordinates, not pixels
    void shift(float s, float t);
    void scale(float s, float t, std::size_t shaderWidth, std::size_t shaderHeight);
    void rotate(float angle);

    // Normalise projection for a given texture width and height.
    void normalise(float width, float height);

    Matrix4 getBasisForNormal(const Vector3& normal) const;

    void transformLocked(std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& identity2transformed);

    // Fits a texture to a brush face
    void fitTexture(std::size_t width, std::size_t height, const Vector3& normal, const Winding& w, float s_repeat, float t_repeat);

    /** greebo: Mirrors the texture around the given axis.
     *
     * @flipAxis: 0 = flip x, 1 = flip y
     */
    void flipTexture(unsigned int flipAxis);

    // Aligns this texture to the given edge of the winding
    void alignTexture(EAlignType align, const Winding& winding);

    // greebo: Looks like this method saves the texture definitions into the brush winding points
    void emitTextureCoordinates(Winding& w, const Vector3& normal, const Matrix4& localToWorld) const;

    // greebo: This returns a matrix that transforms world vertex coordinates into this texture space
    Matrix4 getWorldToTexture(const Vector3& normal, const Matrix4& localToWorld) const;

}; // class TextureProjection
