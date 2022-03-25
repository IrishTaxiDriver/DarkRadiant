#include "LightingModeRenderer.h"

#include "GLProgramFactory.h"
#include "LightingModeRenderResult.h"
#include "LightInteractions.h"
#include "OpenGLShaderPass.h"
#include "OpenGLShader.h"
#include "ObjectRenderer.h"
#include "OpenGLState.h"
#include "glprogram/GLSLDepthFillAlphaProgram.h"
#include "glprogram/GLSLBumpProgram.h"

namespace render
{

void LightingModeRenderer::ensureShadowMapSetup()
{
    if (!_shadowMapFbo)
    {
        _shadowMapFbo = FrameBuffer::CreateShadowMapBuffer();

        // Define the shadow atlas regions (supporting 6 lights)
        _shadowMapAtlas.resize(6);

        for (int i = 0; i < 6; ++i)
        {
            _shadowMapAtlas[i].x = 0;
            _shadowMapAtlas[i].y = static_cast<int>((_shadowMapFbo->getHeight() / 6) * i);
            _shadowMapAtlas[i].width = static_cast<int>(_shadowMapFbo->getWidth() / 6);
            _shadowMapAtlas[i].height = static_cast<int>(_shadowMapFbo->getHeight() / 6);
        }
    }

    if (!_shadowMapProgram)
    {
        _shadowMapProgram = dynamic_cast<ShadowMapProgram*>(_programFactory.getBuiltInProgram(ShaderProgram::ShadowMap));
        assert(_shadowMapProgram);
    }
}

IRenderResult::Ptr LightingModeRenderer::render(RenderStateFlags globalFlagsMask, 
    const IRenderView& view, std::size_t time)
{
    _result = std::make_shared<LightingModeRenderResult>();

    ensureShadowMapSetup();

    determineLightInteractions(view);

    // Construct default OpenGL state
    OpenGLState current;
    setupState(current);

    // Past this point, everything in the geometry store is up to date
    _geometryStore.syncToBufferObjects();

    auto [vertexBuffer, indexBuffer] = _geometryStore.getBufferObjects();

    vertexBuffer->bind();
    indexBuffer->bind();

    // Set the vertex attribute pointers
    ObjectRenderer::InitAttributePointers();

    // Render depth information to the shadow maps
    drawShadowMaps(current, time);

    // Load the model view & projection matrix for the main scene
    setupViewMatrices(view);

    // Run the depth fill pass
    drawDepthFillPass(current, globalFlagsMask, view, time);

    // Draw the surfaces per light and material
    drawLightInteractions(current, globalFlagsMask, view, time);

    // Draw any surfaces without any light interactions
    drawNonInteractionPasses(current, globalFlagsMask, view, time);

    vertexBuffer->unbind();
    indexBuffer->unbind();

    cleanupState();

    // Cleanup the data accumulated in this render pass
    _interactingLights.clear();

    return std::move(_result); // move-return our result reference
}

void LightingModeRenderer::determineLightInteractions(const IRenderView& view)
{
    _interactingLights.reserve(_lights.size());

    // Gather all visible lights and render the surfaces touched by them
    for (const auto& light : _lights)
    {
        LightInteractions interaction(*light, _geometryStore);

        if (!interaction.isInView(view))
        {
            _result->skippedLights++;
            continue;
        }

        _result->visibleLights++;

        // Check all the surfaces that are touching this light
        interaction.collectSurfaces(view, _entities);

        _result->objects += interaction.getObjectCount();
        _result->entities += interaction.getEntityCount();

        _interactingLights.emplace_back(std::move(interaction));
    }
}

void LightingModeRenderer::drawLightInteractions(OpenGLState& current, RenderStateFlags globalFlagsMask,
    const IRenderView& view, std::size_t renderTime)
{
    // Draw the surfaces per light and material
    auto interactionState = InteractionPass::GenerateInteractionState(_programFactory);

    // Prepare the current state for drawing
    interactionState.applyTo(current, globalFlagsMask);

    auto interactionProgram = dynamic_cast<GLSLBumpProgram*>(current.glProgram);
    assert(interactionProgram);

    interactionProgram->setModelViewProjection(view.GetViewProjection());

    // Bind the texture containing the shadow maps
    OpenGLState::SetTextureState(current.texture5, _shadowMapFbo->getTextureNumber(), GL_TEXTURE5, GL_TEXTURE_2D);

    for (auto& interactionList : _interactingLights)
    {
        if (interactionList.castsShadows())
        {
            // Define which part of the shadow map atlas should be sampled
            interactionProgram->enableShadowMapping(true);
            interactionProgram->setShadowMapRectangle(_shadowMapAtlas[3]);
        }
        else
        {
            interactionProgram->enableShadowMapping(false);
        }

        interactionList.drawInteractions(current, *interactionProgram, view, renderTime);
        _result->interactionDrawCalls += interactionList.getInteractionDrawCalls();
    }

    // Unbind the shadow map texture
    OpenGLState::SetTextureState(current.texture5, 0, GL_TEXTURE5, GL_TEXTURE_2D);
}

void LightingModeRenderer::drawShadowMaps(OpenGLState& current,std::size_t renderTime)
{
    // Draw the shadow maps of each light
    // Save the viewport set up in the camera code
    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    _shadowMapProgram->enable();
    _shadowMapFbo->bind();

    // Enable GL state and save to state
    glDepthMask(GL_TRUE);
    current.setRenderFlag(RENDER_DEPTHWRITE);

    glDepthFunc(GL_LEQUAL);
    current.setDepthFunc(GL_LEQUAL);

    glEnable(GL_DEPTH_TEST);
    current.setRenderFlag(RENDER_DEPTHTEST);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    current.setRenderFlag(RENDER_FILL);

    glPolygonOffset(0, 0);
    glEnable(GL_POLYGON_OFFSET_FILL);

    // Enable the 4 clip planes, they are used in the vertex shader
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_CLIP_DISTANCE1);
    glEnable(GL_CLIP_DISTANCE2);
    glEnable(GL_CLIP_DISTANCE3);

    // Render a single light to the shadow map buffer
    for (auto& interactionList : _interactingLights)
    {
        if (!interactionList.castsShadows()) continue;

        interactionList.drawShadowMap(current, _shadowMapAtlas[3], *_shadowMapProgram, renderTime);
        _result->shadowDrawCalls += interactionList.getShadowMapDrawCalls();
        break;
    }

    _shadowMapFbo->unbind();
    _shadowMapProgram->disable();

    glDisable(GL_CLIP_DISTANCE3);
    glDisable(GL_CLIP_DISTANCE2);
    glDisable(GL_CLIP_DISTANCE1);
    glDisable(GL_CLIP_DISTANCE0);

    glDisable(GL_POLYGON_OFFSET_FILL);

    // Restore view port
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    glDisable(GL_DEPTH_TEST);
    current.clearRenderFlag(RENDER_DEPTHTEST);
}


void LightingModeRenderer::drawDepthFillPass(OpenGLState& current, RenderStateFlags globalFlagsMask,
    const IRenderView& view, std::size_t renderTime)
{
    // Run the depth fill pass
    auto depthFillState = DepthFillPass::GenerateDepthFillState(_programFactory);

    // Prepare the current state for depth filling
    depthFillState.applyTo(current, globalFlagsMask);

    auto depthFillProgram = dynamic_cast<GLSLDepthFillAlphaProgram*>(current.glProgram);
    assert(depthFillProgram);

    // Set the modelview and projection matrix
    depthFillProgram->setModelViewProjection(view.GetViewProjection());

    for (auto& interactionList : _interactingLights)
    {
        interactionList.fillDepthBuffer(current, *depthFillProgram, renderTime, _untransformedObjectsWithoutAlphaTest);
        _result->depthDrawCalls += interactionList.getDepthDrawCalls();
    }

    // Unbind the diffuse texture
    OpenGLState::SetTextureState(current.texture0, 0, GL_TEXTURE0, GL_TEXTURE_2D);

    // All objects without alpha test or transformation matrix go into one final drawcall
    if (!_untransformedObjectsWithoutAlphaTest.empty())
    {
        depthFillProgram->setObjectTransform(Matrix4::getIdentity());
        depthFillProgram->setAlphaTest(-1);

        ObjectRenderer::SubmitGeometry(_untransformedObjectsWithoutAlphaTest, GL_TRIANGLES, _geometryStore);
        _result->depthDrawCalls++;

        _untransformedObjectsWithoutAlphaTest.clear();
    }
}

void LightingModeRenderer::drawNonInteractionPasses(OpenGLState& current, RenderStateFlags globalFlagsMask, 
    const IRenderView& view, std::size_t time)
{
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    // Draw non-interaction passes (like skyboxes or blend stages)
    for (const auto& entity : _entities)
    {
        entity->foreachRenderable([&](const render::IRenderableObject::Ptr& object, Shader* shader)
        {
            // Skip empty objects
            if (!object->isVisible()) return;

            // Don't collect invisible shaders
            if (!shader->isVisible()) return;

            auto glShader = static_cast<OpenGLShader*>(shader);

            // We only consider materials designated for camera rendering
            if (!glShader->isApplicableTo(RenderViewType::Camera))
            {
                return;
            }

            // For each pass except for the depth fill and interaction passes, draw the geometry
            glShader->foreachNonInteractionPass([&](OpenGLShaderPass& pass)
            {
                if (!pass.stateIsActive())
                {
                    return;
                }

                // Apply our state to the current state object
                pass.evaluateStagesAndApplyState(current, globalFlagsMask, time, entity.get());

                if (current.glProgram)
                {
                    OpenGLShaderPass::SetUpNonInteractionProgram(current, view.getViewer(), object->getObjectTransform());
                }

                ObjectRenderer::SubmitObject(*object, _geometryStore);
                _result->nonInteractionDrawCalls++;
            });
        });
    }
}

}