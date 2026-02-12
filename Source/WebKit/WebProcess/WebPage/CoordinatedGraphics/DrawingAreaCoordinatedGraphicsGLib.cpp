/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DrawingAreaCoordinatedGraphicsGLib.h"

#include "DrawingAreaProxyMessages.h"
#include "LayerTreeHost.h"
#include "MessageSenderInlines.h"
#include "NonCompositedFrameRenderer.h"
#include "WebDisplayRefreshMonitor.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageInlines.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Region.h>
#include <WebCore/Settings.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/SetForScope.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaCoordinatedGraphics::DrawingAreaCoordinatedGraphics(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(parameters.drawingAreaIdentifier, webPage)
    , m_isPaintingSuspended(!(parameters.activityState & ActivityState::IsVisible))
    , m_displayTimer(RunLoop::mainSingleton(), "DrawingAreaCoordinatedGraphics::DisplayTimer"_s, this, &DrawingAreaCoordinatedGraphics::displayTimerFired)
{
#if !PLATFORM(WPE)
    m_displayTimer.setPriority(RunLoopSourcePriority::NonAcceleratedDrawingTimer);
#endif
}

DrawingAreaCoordinatedGraphics::~DrawingAreaCoordinatedGraphics() = default;

void DrawingAreaCoordinatedGraphics::setNeedsDisplay()
{
    if (m_layerTreeHost)
        return;

    setNeedsDisplayInRect(m_webPage->bounds());
}

void DrawingAreaCoordinatedGraphics::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_layerTreeHost)
        return;

    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage->bounds());
    if (dirtyRect.isEmpty())
        return;

    if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect(dirtyRect);
        scheduleDisplay();
    }
}

void DrawingAreaCoordinatedGraphics::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (m_layerTreeHost)
        return;

    if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect(m_webPage->bounds());
        scheduleDisplay();
    }
}

void DrawingAreaCoordinatedGraphics::updateRenderingWithForcedRepaint()
{
    if (m_layerTreeHost && !m_layerTreeStateIsFrozen)
        m_layerTreeHost->updateRenderingWithForcedRepaint();
    else if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->updateRenderingWithForcedRepaint();
}

void DrawingAreaCoordinatedGraphics::updateRenderingWithForcedRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    if (m_layerTreeHost) {
        if (m_layerTreeStateIsFrozen)
            return completionHandler();
        m_layerTreeHost->updateRenderingWithForcedRepaintAsync(WTF::move(completionHandler));
    } else if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->updateRenderingWithForcedRepaintAsync(WTF::move(completionHandler));
}

void DrawingAreaCoordinatedGraphics::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;

    if (m_layerTreeHost)
        m_layerTreeHost->setLayerTreeStateIsFrozen(isFrozen);
    else if (!isFrozen)
        scheduleDisplay();
}

void DrawingAreaCoordinatedGraphics::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage->corePage()->settings();
#if PLATFORM(GTK)
    if (settings.hardwareAccelerationEnabled())
        WebProcess::singleton().initializePlatformDisplayIfNeeded();
#endif
    settings.setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));
    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());

    m_supportsAsyncScrolling = settings.acceleratedCompositingEnabled() && store.getBoolValueForKey(WebPreferencesKey::threadedScrollingEnabledKey());
#if ENABLE(DEVELOPER_MODE)
    if (m_supportsAsyncScrolling) {
        auto disableAsyncScrolling = StringView::fromLatin1(getenv("WEBKIT_DISABLE_ASYNC_SCROLLING"));
        if (!disableAsyncScrolling.isEmpty() && disableAsyncScrolling == "0"_s)
            m_supportsAsyncScrolling = false;
    }
#endif

    // If async scrolling is disabled, we have to force-disable async frame and overflow scrolling
    // to keep the non-async scrolling on those elements working.
    if (!m_supportsAsyncScrolling) {
        settings.setAsyncFrameScrollingEnabled(false);
        settings.setAsyncOverflowScrollingEnabled(false);
    }
}

bool DrawingAreaCoordinatedGraphics::enterAcceleratedCompositingModeIfNeeded()
{
    if (m_webPage->corePage()->settings().acceleratedCompositingEnabled()) {
        m_layerTreeHost = makeUnique<LayerTreeHost>(m_webPage);
        if (m_isPaintingSuspended)
            m_layerTreeHost->pauseRendering();
    } else
        m_nonCompositedFrameRenderer = NonCompositedFrameRenderer::create(m_webPage);
    return true;
}

void DrawingAreaCoordinatedGraphics::backgroundColorDidChange()
{
    if (m_layerTreeHost)
        m_layerTreeHost->backgroundColorDidChange();
}

void DrawingAreaCoordinatedGraphics::setDeviceScaleFactor(float deviceScaleFactor, CompletionHandler<void()>&& completionHandler)
{
    Ref webPage = m_webPage.get();
    webPage->setDeviceScaleFactor(deviceScaleFactor);
    if (m_layerTreeHost && !webPage->size().isEmpty())
        m_layerTreeHost->sizeDidChange();
    completionHandler();
}

bool DrawingAreaCoordinatedGraphics::supportsAsyncScrolling() const
{
    return m_supportsAsyncScrolling;
}

void DrawingAreaCoordinatedGraphics::registerScrollingTree()
{
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    if (m_supportsAsyncScrolling)
        WebProcess::singleton().eventDispatcher().addScrollingTreeForPage(m_webPage);
#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
}

void DrawingAreaCoordinatedGraphics::unregisterScrollingTree()
{
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    if (m_supportsAsyncScrolling)
        WebProcess::singleton().eventDispatcher().removeScrollingTreeForPage(m_webPage);
#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
}

GraphicsLayerFactory* DrawingAreaCoordinatedGraphics::graphicsLayerFactory()
{
    RELEASE_ASSERT(m_layerTreeHost);
    return m_layerTreeHost->graphicsLayerFactory();
}

void DrawingAreaCoordinatedGraphics::setRootCompositingLayer(WebCore::Frame&, GraphicsLayer* graphicsLayer)
{
    if (m_layerTreeHost)
        m_layerTreeHost->setRootCompositingLayer(graphicsLayer);
}

void DrawingAreaCoordinatedGraphics::triggerRenderingUpdate()
{
    if (m_layerTreeStateIsFrozen)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->scheduleRenderingUpdate();
    else
        scheduleDisplay();
}

RefPtr<DisplayRefreshMonitor> DrawingAreaCoordinatedGraphics::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return WebDisplayRefreshMonitor::create(displayID);
}

void DrawingAreaCoordinatedGraphics::activityStateDidChange(OptionSet<ActivityState> changed, ActivityStateChangeID, CompletionHandler<void()>&& completionHandler)
{
    if (changed & ActivityState::IsVisible) {
        if (m_webPage->isVisible())
            resumePainting();
        else
            suspendPainting();
    }
    completionHandler();
}

void DrawingAreaCoordinatedGraphics::attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier, GraphicsLayer* viewOverlayRootLayer)
{
    if (m_layerTreeHost)
        m_layerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

void DrawingAreaCoordinatedGraphics::updateGeometry(const IntSize& size, CompletionHandler<void()>&& completionHandler)
{
    SetForScope inUpdateGeometry(m_inUpdateGeometry, true);
    Ref webPage = m_webPage.get();
    webPage->setSize(size);
    webPage->layoutIfNeeded();

    if (m_layerTreeHost)
        m_layerTreeHost->sizeDidChange();
    else if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect({ { }, size });
        m_nonCompositedFrameRenderer->display();
    }

    completionHandler();
}

void DrawingAreaCoordinatedGraphics::dispatchAfterEnsuringDrawing(IPC::AsyncReplyID callbackID)
{
    m_pendingAfterDrawCallbackIDs.append(callbackID);
    if (m_layerTreeHost) {
        if (!m_layerTreeStateIsFrozen) {
            m_layerTreeHost->ensureDrawing();
            return;
        }
    } else if (!m_isPaintingSuspended && m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect(m_webPage->bounds());
        scheduleDisplay();
        return;
    }

    // We can't ensure drawing, so process pending callbacks.
    dispatchPendingCallbacksAfterEnsuringDrawing();
}

void DrawingAreaCoordinatedGraphics::dispatchPendingCallbacksAfterEnsuringDrawing()
{
    if (m_pendingAfterDrawCallbackIDs.isEmpty())
        return;

    send(Messages::DrawingAreaProxy::DispatchPresentationCallbacksAfterFlushingLayers(m_pendingAfterDrawCallbackIDs));
    m_pendingAfterDrawCallbackIDs.clear();
}

#if PLATFORM(GTK)
void DrawingAreaCoordinatedGraphics::adjustTransientZoom(double scale, FloatPoint origin)
{
    if (!m_transientZoom) {
        RefPtr frameView = m_webPage->localMainFrameView();
        if (!frameView)
            return;

        FloatRect unobscuredContentRect = frameView->unobscuredContentRectIncludingScrollbars();

        m_transientZoom = true;
        m_transientZoomInitialOrigin = unobscuredContentRect.location();
    }

    if (m_layerTreeHost) {
        m_layerTreeHost->adjustTransientZoom(scale, origin);
        return;
    }

    // We can't do transient zoom for non-AC mode, so just zoom in place instead.

    FloatPoint unscrolledOrigin(origin);
    unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);

    Ref webPage = m_webPage.get();
    webPage->scalePage(scale / webPage->viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
}

void DrawingAreaCoordinatedGraphics::commitTransientZoom(double scale, FloatPoint origin, CompletionHandler<void()>&& completionHandler)
{
    if (m_layerTreeHost)
        m_layerTreeHost->commitTransientZoom(scale, origin);

    FloatPoint unscrolledOrigin(origin);
    unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);

    Ref webPage = m_webPage.get();
    webPage->scalePage(scale / webPage->viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));

    m_transientZoom = false;
    completionHandler();
}
#endif

void DrawingAreaCoordinatedGraphics::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    if (m_layerTreeHost)
        m_layerTreeHost->pauseRendering();
    else
        m_displayTimer.stop();

    m_isPaintingSuspended = true;

    m_webPage->corePage()->suspendScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }

    if (m_layerTreeHost)
        m_layerTreeHost->resumeRendering();

    m_isPaintingSuspended = false;

    // FIXME: We shouldn't always repaint everything here.
    setNeedsDisplay();

    m_webPage->corePage()->resumeScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::sendEnterAcceleratedCompositingModeIfNeeded()
{
    if (m_compositingAccordingToProxyMessages)
        return;

    RELEASE_ASSERT(m_layerTreeHost || m_nonCompositedFrameRenderer);
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerTreeHost ? m_layerTreeHost->layerTreeContext().contextID : m_nonCompositedFrameRenderer->surfaceID();

    send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
    m_compositingAccordingToProxyMessages = true;
}

void DrawingAreaCoordinatedGraphics::scheduleDisplay()
{
    ASSERT(!m_layerTreeHost);

    if (m_isPaintingSuspended)
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0_s);
}

void DrawingAreaCoordinatedGraphics::displayTimerFired()
{
    display();
}

void DrawingAreaCoordinatedGraphics::display()
{
    ASSERT(!m_layerTreeHost);
    ASSERT(!m_inUpdateGeometry);

    if (m_layerTreeStateIsFrozen)
        return;

    if (m_isPaintingSuspended)
        return;

    if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->display();
        return;
    }
}

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
void DrawingAreaCoordinatedGraphics::preferredBufferFormatsDidChange()
{
    if (m_layerTreeHost)
        m_layerTreeHost->preferredBufferFormatsDidChange();
}
#endif

#if ENABLE(DAMAGE_TRACKING)
void DrawingAreaCoordinatedGraphics::resetDamageHistoryForTesting()
{
    if (m_layerTreeHost)
        m_layerTreeHost->resetDamageHistoryForTesting();
    else if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->resetDamageHistoryForTesting();
}

void DrawingAreaCoordinatedGraphics::foreachRegionInDamageHistoryForTesting(Function<void(const Region&)>&& callback) const
{
    if (m_layerTreeHost)
        m_layerTreeHost->foreachRegionInDamageHistoryForTesting(WTF::move(callback));
    else if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->foreachRegionInDamageHistoryForTesting(WTF::move(callback));
}
#endif

void DrawingAreaCoordinatedGraphics::fillGLInformation(RenderProcessInfo&& info, CompletionHandler<void(RenderProcessInfo&&)>&& completionHandler)
{
    if (m_layerTreeHost)
        m_layerTreeHost->fillGLInformation(WTF::move(info), WTF::move(completionHandler));
    else
        completionHandler(WTF::move(info));
}

bool DrawingAreaCoordinatedGraphics::shouldUseTiledBackingForFrameView(const WebCore::LocalFrameView& frameView) const
{
    return frameView.frame().isMainFrame() || m_webPage->corePage()->settings().asyncFrameScrollingEnabled();
}

} // namespace WebKit
