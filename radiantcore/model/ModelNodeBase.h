#pragma once

#include <vector>
#include "scene/Node.h"
#include "RenderableModelSurface.h"

namespace model
{
/**
 * Common ModelNode implementation used by various model types,
 * e.g. StaticModelNode and MD5ModelNode
 */
class ModelNodeBase :
    public scene::Node
{
protected:
    // The renderable surfaces attached to the shaders
    std::vector<RenderableModelSurface::Ptr> _renderableSurfaces;

    bool _attachedToShaders;

protected:
    ModelNodeBase();

public:
    scene::INode::Type getNodeType() const override;

    void renderHighlights(IRenderableCollector& collector, const VolumeTest& volume) override;
    std::size_t getHighlightFlags() override;

    void onInsertIntoScene(scene::IMapRootNode& root) override;
    void onRemoveFromScene(scene::IMapRootNode& root) override;

protected:
    // To be implemented by subclasses, this should populate the _renderableSurfaces collection
    virtual void createRenderableSurfaces() = 0;

    // Detaches all surfaces from their shaders and clears the _renderableSurfaces collection
    virtual void destroyRenderableSurfaces();

    void attachToShaders();
    void detachFromShaders();
    void queueRenderableUpdate();
};

}
