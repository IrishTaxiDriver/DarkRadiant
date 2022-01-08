#include "BrushNode.h"

#include "ivolumetest.h"
#include "ifilter.h"
#include "iradiant.h"
#include "icounter.h"
#include "iclipper.h"
#include "imap.h"
#include "ientity.h"
#include "math/Frustum.h"
#include "math/Hash.h"
#include <functional>

// Constructor
BrushNode::BrushNode() :
	scene::SelectableNode(),
	m_brush(*this),
    _faceCentroidPointsNeedUpdate(true),
	_selectedPoints(GL_POINTS),
	_visibleFaceCentroidPoints(GL_POINTS),
	_renderableComponentsNeedUpdate(true),
    _untransformedOriginChanged(true),
    _renderableVertices(m_brush)
{
	m_brush.attach(*this); // BrushObserver

    // Try to anticipate a few face additions to avoid reallocations during map parsing
    reserve(6);
}

// Copy Constructor
BrushNode::BrushNode(const BrushNode& other) :
	scene::SelectableNode(other),
	scene::Cloneable(other),
	Snappable(other),
	IBrushNode(other),
	BrushObserver(other),
	SelectionTestable(other),
	ComponentSelectionTestable(other),
	ComponentEditable(other),
	ComponentSnappable(other),
	PlaneSelectable(other),
	Transformable(other),
	m_brush(*this, other.m_brush),
    _faceCentroidPointsNeedUpdate(true),
	_selectedPoints(GL_POINTS),
	_visibleFaceCentroidPoints(GL_POINTS),
	_renderableComponentsNeedUpdate(true),
    _untransformedOriginChanged(true),
    _renderableVertices(m_brush)
{
	m_brush.attach(*this); // BrushObserver
}

BrushNode::~BrushNode()
{
	m_brush.detach(*this); // BrushObserver
}

scene::INode::Type BrushNode::getNodeType() const
{
	return Type::Brush;
}

const AABB& BrushNode::localAABB() const {
	return m_brush.localAABB();
}

std::string BrushNode::getFingerprint()
{
    constexpr std::size_t SignificantDigits = scene::SignificantFingerprintDoubleDigits;

    if (m_brush.getNumFaces() == 0)
    {
        return std::string(); // empty brushes produce an empty fingerprint
    }

    math::Hash hash;

    hash.addSizet(static_cast<std::size_t>(m_brush.getDetailFlag() + 1));

    hash.addSizet(m_brush.getNumFaces());

    // Combine all face plane equations
    for (const auto& face : m_brush)
    {
        // Plane equation
        hash.addVector3(face->getPlane3().normal(), SignificantDigits);
        hash.addDouble(face->getPlane3().dist(), SignificantDigits);

        // Material Name
        hash.addString(face->getShader());

        // Texture Matrix
        auto texdef = face->getProjectionMatrix();
        hash.addDouble(texdef.xx(), SignificantDigits);
        hash.addDouble(texdef.yx(), SignificantDigits);
        hash.addDouble(texdef.zx(), SignificantDigits);
        hash.addDouble(texdef.xy(), SignificantDigits);
        hash.addDouble(texdef.yy(), SignificantDigits);
        hash.addDouble(texdef.zy(), SignificantDigits);
    }

    return hash;
}

// Snappable implementation
void BrushNode::snapto(float snap) {
	m_brush.snapto(snap);
}

void BrushNode::snapComponents(float snap) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->snapComponents(snap);
	}
}

void BrushNode::testSelect(Selector& selector, SelectionTest& test)
{
    // BeginMesh(true): Always treat brush faces twosided when in orthoview
	test.BeginMesh(localToWorld(), !test.getVolume().fill());

	SelectionIntersection best;
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i)
	{
		if (i->faceIsVisible())
		{
			i->testSelect(test, best);
		}
	}

	if (best.isValid()) {
		selector.addIntersection(best);
	}
}

bool BrushNode::isSelectedComponents() const {
	for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		if (i->selectedComponents()) {
			return true;
		}
	}
	return false;
}

void BrushNode::setSelectedComponents(bool select, selection::ComponentSelectionMode mode)
{
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->setSelected(mode, select);
	}
}

void BrushNode::invertSelectedComponents(selection::ComponentSelectionMode mode)
{
	// Component mode, invert the component selection
	switch (mode)
	{
	case selection::ComponentSelectionMode::Vertex:
		for (brush::VertexInstance& vertex : m_vertexInstances)
		{
			vertex.invertSelected();
		}
		break;
	case selection::ComponentSelectionMode::Edge:
		for (EdgeInstance& edge : m_edgeInstances)
		{
			edge.invertSelected();
		}
		break;
	case selection::ComponentSelectionMode::Face:
		for (FaceInstance& face : m_faceInstances)
		{
			face.invertSelected();
		}
		break;
	case selection::ComponentSelectionMode::Default:
		break;
	} // switch
}

void BrushNode::testSelectComponents(Selector& selector, SelectionTest& test, selection::ComponentSelectionMode mode)
{
	test.BeginMesh(localToWorld());

	switch (mode) {
		case selection::ComponentSelectionMode::Vertex: {
				for (VertexInstances::iterator i = m_vertexInstances.begin(); i != m_vertexInstances.end(); ++i) {
					i->testSelect(selector, test);
				}
			}
		break;
		case selection::ComponentSelectionMode::Edge: {
				for (EdgeInstances::iterator i = m_edgeInstances.begin(); i != m_edgeInstances.end(); ++i) {
					i->testSelect(selector, test);
				}
			}
			break;
		case selection::ComponentSelectionMode::Face: {
				if (test.getVolume().fill()) {
					for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
						i->testSelect(selector, test);
					}
				}
				else {
					for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
						i->testSelect_centroid(selector, test);
					}
				}
			}
			break;
		default:
			break;
	}
}

const AABB& BrushNode::getSelectedComponentsBounds() const {
	m_aabb_component = AABB();

	for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->iterate_selected(m_aabb_component);
	}

	return m_aabb_component;
}

void BrushNode::selectPlanes(Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback) {
	test.BeginMesh(localToWorld());

	PlanePointer brushPlanes[brush::c_brush_maxFaces];
	PlanesIterator j = brushPlanes;

	for (Brush::const_iterator i = m_brush.begin(); i != m_brush.end(); ++i) {
		*j++ = &(*i)->plane3();
	}

	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->selectPlane(selector, Line(test.getNear(), test.getFar()), brushPlanes, j, selectedPlaneCallback);
	}
}

void BrushNode::selectReversedPlanes(Selector& selector, const SelectedPlanes& selectedPlanes) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->selectReversedPlane(selector, selectedPlanes);
	}
}

void BrushNode::selectedChangedComponent(const ISelectable& selectable)
{
	_renderableComponentsNeedUpdate = true;

	GlobalSelectionSystem().onComponentSelection(SelectableNode::getSelf(), selectable);
}

// IBrushNode implementation
Brush& BrushNode::getBrush() {
	return m_brush;
}

IBrush& BrushNode::getIBrush() {
	return m_brush;
}

void BrushNode::translate(const Vector3& translation)
{
	m_brush.translate(translation);
}

scene::INodePtr BrushNode::clone() const {
	return std::make_shared<BrushNode>(*this);
}

void BrushNode::onInsertIntoScene(scene::IMapRootNode& root)
{
    m_brush.connectUndoSystem(root.getUndoSystem());
	GlobalCounters().getCounter(counterBrushes).increment();

    // Update the origin information needed for transformations
    _untransformedOriginChanged = true;
    _renderableVertices.queueUpdate();

	SelectableNode::onInsertIntoScene(root);
}

void BrushNode::onRemoveFromScene(scene::IMapRootNode& root)
{
	// De-select this node
	setSelected(false);

	// De-select all child components as well
	setSelectedComponents(false, selection::ComponentSelectionMode::Vertex);
	setSelectedComponents(false, selection::ComponentSelectionMode::Edge);
	setSelectedComponents(false, selection::ComponentSelectionMode::Face);

	GlobalCounters().getCounter(counterBrushes).decrement();
    m_brush.disconnectUndoSystem(root.getUndoSystem());
    _renderableVertices.clear();

	SelectableNode::onRemoveFromScene(root);
}

void BrushNode::clear() {
	m_faceInstances.clear();
}

void BrushNode::reserve(std::size_t size) {
	m_faceInstances.reserve(size);
}

void BrushNode::push_back(Face& face)
{
	m_faceInstances.emplace_back(face, std::bind(&BrushNode::selectedChangedComponent, this, std::placeholders::_1));
    _untransformedOriginChanged = true;
}

void BrushNode::pop_back() {
	ASSERT_MESSAGE(!m_faceInstances.empty(), "erasing invalid element");
	m_faceInstances.pop_back();
    _untransformedOriginChanged = true;
}

void BrushNode::erase(std::size_t index) {
	ASSERT_MESSAGE(index < m_faceInstances.size(), "erasing invalid element");
	m_faceInstances.erase(m_faceInstances.begin() + index);
}
void BrushNode::connectivityChanged() {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->connectivityChanged();
	}
}

void BrushNode::edge_clear() {
	m_edgeInstances.clear();
}
void BrushNode::edge_push_back(SelectableEdge& edge) {
	m_edgeInstances.push_back(EdgeInstance(m_faceInstances, edge));
}

void BrushNode::vertex_clear() {
	m_vertexInstances.clear();
}
void BrushNode::vertex_push_back(SelectableVertex& vertex) {
	m_vertexInstances.push_back(brush::VertexInstance(m_faceInstances, vertex));
}

void BrushNode::DEBUG_verify() {
	ASSERT_MESSAGE(m_faceInstances.size() == m_brush.DEBUG_size(), "FATAL: mismatch");
}

void BrushNode::onPreRender(const VolumeTest& volume)
{
    m_brush.evaluateBRep();

    assert(_renderEntity);

    // Every face is asked to run the rendering preparations
    // to link/unlink their geometry to/from the active shader
    for (auto& faceInstance : m_faceInstances)
    {
        auto& face = faceInstance.getFace();

        if (volume.fill())
        {
            face.getWindingSurfaceSolid().update(face.getFaceShader().getGLShader(), *_renderEntity);
        }
        else
        {
            face.getWindingSurfaceWireframe().update(_renderEntity->getWireShader(), *_renderEntity);
        }
    }

    if (isSelected() && GlobalSelectionSystem().Mode() == selection::SelectionSystem::eComponent)
    {
        _renderableVertices.setComponentMode(GlobalSelectionSystem().ComponentMode());
        _renderableVertices.update(m_brush.m_state_point);
    }
    else
    {
        _renderableVertices.clear();
        _renderableVertices.queueUpdate();
    }
}

void BrushNode::renderComponents(IRenderableCollector& collector, const VolumeTest& volume) const
{
	const Matrix4& l2w = localToWorld();

	if (volume.fill() && GlobalSelectionSystem().ComponentMode() == selection::ComponentSelectionMode::Face)
	{
        updateFaceCentroidPoints();
		collector.addRenderable(*m_brush.m_state_point, _visibleFaceCentroidPoints, l2w);
	}
#if 0
	else
	{
		m_brush.renderComponents(GlobalSelectionSystem().ComponentMode(), collector, volume, l2w);
	}
#endif
}

void BrushNode::renderSolid(IRenderableCollector& collector, const VolumeTest& volume) const
{
#if 0
	renderSolid(collector, volume, localToWorld());
#endif
}

void BrushNode::renderWireframe(IRenderableCollector& collector, const VolumeTest& volume) const
{
#if 0
	renderWireframe(collector, volume, localToWorld());
#endif
}

void BrushNode::renderHighlights(IRenderableCollector& collector, const VolumeTest& volume)
{
    // Check for the override status of this brush
    bool forceVisible = isForcedVisible();
    bool wholeBrushSelected = isSelected() || Node_isSelected(getParent());

    collector.setHighlightFlag(IRenderableCollector::Highlight::Primitives, wholeBrushSelected);

    // Submit the renderable geometry for each face
    for (auto& faceInstance : m_faceInstances)
    {
        // Skip invisible faces before traversing further
        if (!forceVisible && !faceInstance.faceIsVisible()) continue;

        Face& face = faceInstance.getFace();
        if (face.intersectVolume(volume))
        {
            bool highlight = wholeBrushSelected || faceInstance.selectedComponents();

            if (!highlight) continue;

            collector.setHighlightFlag(IRenderableCollector::Highlight::Faces, true);

            // Submit the RenderableWinding as reference, it will render the winding in polygon mode
            collector.addHighlightRenderable(face.getWindingSurfaceSolid(), Matrix4::getIdentity());

            collector.setHighlightFlag(IRenderableCollector::Highlight::Faces, false);
        }
    }

    if (wholeBrushSelected && GlobalClipper().clipMode())
    {
        collector.addHighlightRenderable(m_clipPlane, Matrix4::getIdentity());
    }

    collector.setHighlightFlag(IRenderableCollector::Highlight::Primitives, false);

    // Render any selected points (vertices, edges, faces)
    renderSelectedPoints(collector, volume, Matrix4::getIdentity());
}

void BrushNode::setRenderSystem(const RenderSystemPtr& renderSystem)
{
	SelectableNode::setRenderSystem(renderSystem);

	if (renderSystem)
	{
		m_state_selpoint = renderSystem->capture("$SELPOINT");
	}
	else
	{
		m_state_selpoint.reset();
	}

	m_brush.setRenderSystem(renderSystem);
	m_clipPlane.setRenderSystem(renderSystem);
}

std::size_t BrushNode::getHighlightFlags()
{
	if (!isSelected() && !isSelectedComponents()) return Highlight::NoHighlight;

	return isGroupMember() ? (Highlight::Selected | Highlight::GroupMember) : Highlight::Selected;
}

void BrushNode::onFaceVisibilityChanged()
{
    _faceCentroidPointsNeedUpdate = true;
}

void BrushNode::setForcedVisibility(bool forceVisible, bool includeChildren)
{
    Node::setForcedVisibility(forceVisible, includeChildren);
 
    _faceCentroidPointsNeedUpdate = true;
}

void BrushNode::updateFaceCentroidPoints() const
{
    if (!_faceCentroidPointsNeedUpdate) return;

    static const auto& colourSetting = GlobalBrushCreator().getSettings().getVertexColour();
    const Colour4b vertexColour(
        static_cast<int>(colourSetting[0] * 255), 
        static_cast<int>(colourSetting[1] * 255),
        static_cast<int>(colourSetting[2] * 255), 
        255);

    _faceCentroidPointsNeedUpdate = false;

    _visibleFaceCentroidPoints.clear();

    for (const auto& faceInstance:  m_faceInstances)
    {
        if (faceInstance.faceIsVisible())
        {
            _visibleFaceCentroidPoints.emplace_back(faceInstance.centroid(), vertexColour);
        }
    }
}

#if 0
void BrushNode::updateWireframeVisibility() const
{
    if (!_faceCentroidPointsNeedUpdate) return;

    _faceCentroidPointsNeedUpdate = false;

	// Array of booleans to indicate which faces are visible
	static bool faces_visible[brush::c_brush_maxFaces];

	// Will hold the indices of all visible faces (from the current viewpoint)
	static std::size_t visibleFaceIndices[brush::c_brush_maxFaces];

	std::size_t numVisibleFaces(0);
	bool* j = faces_visible;
	bool forceVisible = isForcedVisible();

    _visibleFaceCentroidPoints.clear();

	// Iterator to an index of a visible face
	std::size_t* visibleFaceIter = visibleFaceIndices;
	std::size_t curFaceIndex = 0;

	for (FaceInstances::const_iterator i = m_faceInstances.begin();
		 i != m_faceInstances.end();
		 ++i, ++j, ++curFaceIndex)
	{
		// Check if face is filtered before adding to visibility matrix
		// greebo: Removed localToWorld transformation here, brushes don't have a non-identity l2w
        // Don't cull backfacing planes to make those faces visible in orthoview (#5465)
		if (forceVisible || i->faceIsVisible())
		{
            _visibleFaceCentroidPoints.push_back(i->centroid());
#if 0
			*j = true;

			// Store the index of this visible face in the array
			*visibleFaceIter++ = curFaceIndex;
			numVisibleFaces++;
		}
		else
		{
			*j = false;
#endif
		}
	}
#if 0
	m_brush.update_wireframe(m_render_wireframe, faces_visible);
#endif
#if 0
	m_brush.update_faces_wireframe(_visibleFaceCentroidPoints, visibleFaceIndices, numVisibleFaces);
#endif
}
#endif

#if 0
void BrushNode::renderSolid(IRenderableCollector& collector,
                            const VolumeTest& volume,
                            const Matrix4& localToWorld) const
{
	assert(_renderEntity); // brushes rendered without parent entity - no way!

#ifdef RENDERABLE_GEOMETRY
    if (isSelected())
    {
        for (FaceInstance& faceInst : const_cast<BrushNode&>(*this).m_faceInstances)
        {
            // Send the winding geometry for rendering highlights
            auto& winding = faceInst.getFace().getWinding();

            if (!winding.empty())
            {
                collector.addGeometry(winding, IRenderableCollector::Highlight::Primitives|IRenderableCollector::Highlight::Flags::Faces);
            }
        }
    }
#endif

#if 0 // The faces already sent their geomtry in onPreRender()
	// Check for the override status of this brush
	bool forceVisible = isForcedVisible();

    // Submit the renderable geometry for each face
    for (const FaceInstance& faceInst : m_faceInstances)
    {
		// Skip invisible faces before traversing further
        if (!forceVisible && !faceInst.faceIsVisible()) continue;

        const Face& face = faceInst.getFace();
        if (face.intersectVolume(volume))
        {
            bool highlight = faceInst.selectedComponents();
            if (highlight)
                collector.setHighlightFlag(IRenderableCollector::Highlight::Faces, true);

#if 1
            // greebo: BrushNodes have always an identity l2w, don't do any transforms
            collector.addRenderable(
                *face.getFaceShader().getGLShader(), face.getWinding(),
                Matrix4::getIdentity(), this, _renderEntity
            );
#endif
            if (highlight)
                collector.setHighlightFlag(IRenderableCollector::Highlight::Faces, false);
        }
    }
#endif
#if 0
	renderSelectedPoints(collector, volume, localToWorld);
#endif
}

void BrushNode::renderWireframe(IRenderableCollector& collector, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	//renderCommon(collector, volume);

    //updateWireframeVisibility();
#if 0
	if (m_render_wireframe.m_size != 0)
	{
		collector.addRenderable(*_renderEntity->getWireShader(), m_render_wireframe, localToWorld);
	}
#endif
#if 0
	renderSelectedPoints(collector, volume, localToWorld);
#endif
}
#endif

void BrushNode::updateSelectedPointsArray() const
{
    if (!_renderableComponentsNeedUpdate) return;

    _renderableComponentsNeedUpdate = false;

    _selectedPoints.clear();

    for (const auto& faceInstance : m_faceInstances)
    {
        if (faceInstance.getFace().contributes())
        {
            faceInstance.iterate_selected(_selectedPoints);
        }
    }
}

void BrushNode::renderSelectedPoints(IRenderableCollector& collector,
                                     const VolumeTest& volume,
                                     const Matrix4& localToWorld) const
{
    updateSelectedPointsArray();

	if (!_selectedPoints.empty())
    {
		collector.setHighlightFlag(IRenderableCollector::Highlight::Primitives, false);
		collector.addRenderable(*m_state_selpoint, _selectedPoints, localToWorld);
	}
}

void BrushNode::evaluateTransform()
{
    if (getTransformationType() == NoTransform)
    {
        return;
    }

	if (getType() == TRANSFORM_PRIMITIVE)
    {
        // If this is a pure translation (no other bits set), call the specialised method
        if (getTransformationType() == Translation)
        {
            for (auto face : m_brush)
            {
                face->translate(getTranslation());
            }
        }
        else
        {
            m_brush.transform(calculateTransform());
        }
	}
	else
    {
		transformComponents(calculateTransform());
	}
}

bool BrushNode::getIntersection(const Ray& ray, Vector3& intersection)
{
	return m_brush.getIntersection(ray, intersection);
}

void BrushNode::updateFaceVisibility()
{
	// Trigger an update, the brush might not have any faces calculated so far
	m_brush.evaluateBRep();

	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i)
	{
		i->updateFaceVisibility();
	}
}

void BrushNode::transformComponents(const Matrix4& matrix) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->transformComponents(matrix);
	}
}

void BrushNode::setClipPlane(const Plane3& plane)
{
    if (_renderEntity)
    {
        m_clipPlane.setPlane(m_brush, plane, *_renderEntity);
    }
}

void BrushNode::forEachFaceInstance(const std::function<void(FaceInstance&)>& functor)
{
	std::for_each(m_faceInstances.begin(), m_faceInstances.end(), functor);
}

const Vector3& BrushNode::getUntransformedOrigin()
{
    if (_untransformedOriginChanged)
    {
        _untransformedOriginChanged = false;
        _untransformedOrigin = worldAABB().getOrigin();
    }

    return _untransformedOrigin;
}

bool BrushNode::facesAreForcedVisible()
{
    return isForcedVisible();
}

void BrushNode::onPostUndo()
{
    // The windings are usually lazy-evaluated when some code
    // is calling localAABB() during rendering.
    // To avoid the texture tool from rendering old texture coords
    // We evaluate the windings right after undo
    m_brush.evaluateBRep();
}

void BrushNode::onPostRedo()
{
    m_brush.evaluateBRep();
}

void BrushNode::_onTransformationChanged()
{
	m_brush.transformChanged();

    _renderableVertices.queueUpdate();
	_renderableComponentsNeedUpdate = true;
    _faceCentroidPointsNeedUpdate = true;
}

void BrushNode::_applyTransformation()
{
	m_brush.revertTransform();
	evaluateTransform();
	m_brush.freezeTransform();

    _untransformedOriginChanged = true;
}

void BrushNode::onVisibilityChanged(bool isVisibleNow)
{
    SelectableNode::onVisibilityChanged(isVisibleNow);

    // Let each face know about the change
    forEachFaceInstance([=](FaceInstance& face)
    {
        face.getFace().onBrushVisibilityChanged(isVisibleNow);
    });

    m_clipPlane.clear();
    _renderableVertices.clear();
}

void BrushNode::onSelectionStatusChange(bool changeGroupStatus)
{
    SelectableNode::onSelectionStatusChange(changeGroupStatus);

    // In clip mode we need to check if there's an active clip plane defined in the scene
    if (isSelected() && GlobalClipper().clipMode())
    {
        setClipPlane(GlobalClipper().getClipPlane());
    }
    else
    {
        m_clipPlane.clear();
    }
}
