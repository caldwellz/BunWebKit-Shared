/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleMutatorBase.h"

namespace WebCore {
namespace Style {

// MARK: - Non-property setters

inline void MutatorBase::setUsesViewportUnits()
{
    m_computedStyle->m_nonInheritedFlags.usesViewportUnits = true;
}

inline void MutatorBase::setUsesContainerUnits()
{
    m_computedStyle->m_nonInheritedFlags.usesContainerUnits = true;
}

inline void MutatorBase::setUsesTreeCountingFunctions()
{
    m_computedStyle->m_nonInheritedFlags.useTreeCountingFunctions = true;
}

inline void MutatorBase::setInsideLink(InsideLink insideLink)
{
    m_computedStyle->m_inheritedFlags.insideLink = static_cast<unsigned>(insideLink);
}

inline void MutatorBase::setIsLink(bool isLink)
{
    m_computedStyle->m_nonInheritedFlags.isLink = isLink;
}

inline void MutatorBase::setEmptyState(bool emptyState)
{
    m_computedStyle->m_nonInheritedFlags.emptyState = emptyState;
}

inline void MutatorBase::setFirstChildState()
{
    m_computedStyle->m_nonInheritedFlags.firstChildState = true;
}

inline void MutatorBase::setLastChildState()
{
    m_computedStyle->m_nonInheritedFlags.lastChildState = true;
}

inline void MutatorBase::setHasExplicitlyInheritedProperties()
{
    m_computedStyle->m_nonInheritedFlags.hasExplicitlyInheritedProperties = true;
}

inline void MutatorBase::setDisallowsFastPathInheritance()
{
    m_computedStyle->m_nonInheritedFlags.disallowsFastPathInheritance = true;
}

inline void MutatorBase::setEffectiveDisplay(DisplayType effectiveDisplay)
{
    m_computedStyle->m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(effectiveDisplay);
}

inline void MutatorBase::setEffectiveInert(bool value)
{
    if (m_computedStyle->m_inheritedRareData->effectiveInert != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().effectiveInert = static_cast<unsigned>(value);
}

inline void MutatorBase::setIsEffectivelyTransparent(bool value)
{
    if (m_computedStyle->m_inheritedRareData->effectivelyTransparent != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().effectivelyTransparent = static_cast<unsigned>(value);
}

inline void MutatorBase::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes)
{
    if (m_computedStyle->m_inheritedRareData->eventListenerRegionTypes != eventListenerTypes)
        m_computedStyle->m_inheritedRareData.access().eventListenerRegionTypes = eventListenerTypes;
}

inline void MutatorBase::setHasAttrContent()
{
    if (m_computedStyle->m_nonInheritedData->miscData->hasAttrContent != static_cast<unsigned>(true))
        m_computedStyle->m_nonInheritedData.access().miscData.access().hasAttrContent = static_cast<unsigned>(true);
}

inline void MutatorBase::setHasDisplayAffectedByAnimations()
{
    if (m_computedStyle->m_nonInheritedData->miscData->hasDisplayAffectedByAnimations != static_cast<unsigned>(true))
        m_computedStyle->m_nonInheritedData.access().miscData.access().hasDisplayAffectedByAnimations = static_cast<unsigned>(true);
}

inline void MutatorBase::setTransformStyleForcedToFlat(bool value)
{
    if (m_computedStyle->m_nonInheritedData->rareData->transformStyleForcedToFlat != static_cast<unsigned>(value))
        m_computedStyle->m_nonInheritedData.access().rareData.access().transformStyleForcedToFlat = static_cast<unsigned>(value);
}

inline void MutatorBase::setUsesAnchorFunctions()
{
    if (m_computedStyle->m_nonInheritedData->rareData->usesAnchorFunctions != static_cast<unsigned>(true))
        m_computedStyle->m_nonInheritedData.access().rareData.access().usesAnchorFunctions = static_cast<unsigned>(true);
}

inline void MutatorBase::setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis> axes)
{
    if (m_computedStyle->m_nonInheritedData->rareData->anchorFunctionScrollCompensatedAxes != axes.toRaw())
        m_computedStyle->m_nonInheritedData.access().rareData.access().anchorFunctionScrollCompensatedAxes = axes.toRaw();
}

inline void MutatorBase::setIsPopoverInvoker()
{
    if (m_computedStyle->m_nonInheritedData->rareData->isPopoverInvoker != static_cast<unsigned>(true))
        m_computedStyle->m_nonInheritedData.access().rareData.access().isPopoverInvoker = static_cast<unsigned>(true);
}

inline void MutatorBase::setNativeAppearanceDisabled(bool value)
{
    if (m_computedStyle->m_nonInheritedData->rareData->nativeAppearanceDisabled != static_cast<unsigned>(value))
        m_computedStyle->m_nonInheritedData.access().rareData.access().nativeAppearanceDisabled = static_cast<unsigned>(value);
}

inline void MutatorBase::setIsForceHidden()
{
    if (m_computedStyle->m_inheritedRareData->isForceHidden != static_cast<unsigned>(true))
        m_computedStyle->m_inheritedRareData.access().isForceHidden = static_cast<unsigned>(true);
}

inline void MutatorBase::setAutoRevealsWhenFound()
{
    if (m_computedStyle->m_inheritedRareData->autoRevealsWhenFound != static_cast<unsigned>(true))
        m_computedStyle->m_inheritedRareData.access().autoRevealsWhenFound = static_cast<unsigned>(true);
}

inline void MutatorBase::setInsideDefaultButton(bool value)
{
    if (m_computedStyle->m_inheritedRareData->insideDefaultButton != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().insideDefaultButton = static_cast<unsigned>(value);
}

inline void MutatorBase::setInsideSubmitButton(bool value)
{
    if (m_computedStyle->m_inheritedRareData->insideSubmitButton != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().insideSubmitButton = static_cast<unsigned>(value);
}

inline void MutatorBase::setUsedPositionOptionIndex(std::optional<size_t> index)
{
    if (m_computedStyle->m_nonInheritedData->rareData->usedPositionOptionIndex != index)
        m_computedStyle->m_nonInheritedData.access().rareData.access().usedPositionOptionIndex = index;
}

inline void MutatorBase::setUsedAppearance(StyleAppearance value)
{
    if (m_computedStyle->m_nonInheritedData->miscData->usedAppearance != static_cast<unsigned>(value))
        m_computedStyle->m_nonInheritedData.access().miscData.access().usedAppearance = static_cast<unsigned>(value);
}

inline void MutatorBase::setUsedContentVisibility(ContentVisibility value)
{
    if (m_computedStyle->m_inheritedRareData->usedContentVisibility != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().usedContentVisibility = static_cast<unsigned>(value);
}

inline void MutatorBase::setUsedTouchAction(TouchAction value)
{
    if (m_computedStyle->m_inheritedRareData->usedTouchAction != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().usedTouchAction = static_cast<unsigned>(value);
}

inline void MutatorBase::setUsedZIndex(ZIndex index)
{
    if (Ref readable = *m_computedStyle->m_nonInheritedData->boxData; readable->hasAutoUsedZIndex != static_cast<uint8_t>(index.m_isAuto) || readable->usedZIndexValue != index.value) {
        Ref writable = m_computedStyle->m_nonInheritedData.access().boxData.access();
        writable->hasAutoUsedZIndex = static_cast<uint8_t>(index.m_isAuto);
        writable->usedZIndexValue = index.value
    }
}

#if HAVE(CORE_MATERIAL)

inline void MutatorBase::setUsedAppleVisualEffectForSubtree(AppleVisualEffect value)
{
    if (m_computedStyle->m_inheritedRareData->usedAppleVisualEffectForSubtree != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().usedAppleVisualEffectForSubtree = static_cast<unsigned>(value);
}

#endif

// MARK: - Pseudo element/style

inline void MutatorBase::setHasPseudoStyles(EnumSet<PseudoElementType> set)
{
    m_computedStyle->m_nonInheritedFlags.setHasPseudoStyles(set);
}

inline void MutatorBase::setPseudoElementIdentifier(std::optional<PseudoElementIdentifier>&& identifier)
{
    if (identifier) {
        m_computedStyle->m_nonInheritedFlags.pseudoElementType = enumToUnderlyingType(identifier->type) + 1;
        if (m_computedStyle->m_nonInheritedData->rareData->pseudoElementNameArgument != identifier->nameArgument)
            m_computedStyle->m_nonInheritedData.access().rareData.access().pseudoElementNameArgument = WTF::move(identifier->nameArgument);
    } else {
        m_computedStyle->m_nonInheritedFlags.pseudoElementType = 0;
        if (!m_computedStyle->m_nonInheritedData->rareData->pseudoElementNameArgument.isNull())
            m_computedStyle->m_nonInheritedData.access().rareData.access().pseudoElementNameArgument = nullAtom();
    }
}

// MARK: - Zoom

inline void MutatorBase::setEvaluationTimeZoomEnabled(bool value)
{
    if (m_computedStyle->m_inheritedRareData->evaluationTimeZoomEnabled != static_cast<unsigned>(value))
        m_computedStyle->m_inheritedRareData.access().evaluationTimeZoomEnabled = static_cast<unsigned>(value);
}

inline void MutatorBase::setDeviceScaleFactor(float value)
{
    if (m_computedStyle->m_inheritedRareData->deviceScaleFactor != value)
        m_computedStyle->m_inheritedRareData.access().deviceScaleFactor = value;
}

inline void MutatorBase::setUseSVGZoomRulesForLength(bool value)
{
    if (m_computedStyle->m_nonInheritedData->rareData->useSVGZoomRulesForLength != static_cast<unsigned>(value))
        m_computedStyle->m_nonInheritedData.access().rareData.access().useSVGZoomRulesForLength = static_cast<unsigned>(value);
}

inline bool MutatorBase::setUsedZoom(float zoomLevel)
{
    if (m_computedStyle->m_inheritedRareData->usedZoom != zoomLevel)
        return false;
    m_computedStyle->m_inheritedFlags.isZoomed = zoomLevel != 1.0f;
    m_computedStyle->m_inheritedRareData.access().usedZoom = zoomLevel;
    return true;
}

// MARK: - Aggregates

inline Animations& MutatorBase::ensureAnimations()
{
    return m_computedStyle->m_nonInheritedData.access().miscData.access().animations.access();
}

inline Transitions& MutatorBase::ensureTransitions()
{
    return m_computedStyle->m_nonInheritedData.access().miscData.access().transitions.access();
}

inline BackgroundLayers& MutatorBase::ensureBackgroundLayers()
{
    return m_computedStyle->m_nonInheritedData.access().backgroundData.access().background.access();
}

inline MaskLayers& MutatorBase::ensureMaskLayers()
{
    return m_computedStyle->m_nonInheritedData.access().miscData.access().mask.access();
}

inline void MutatorBase::setBackgroundLayers(BackgroundLayers&& layers)
{
    if (m_computedStyle->m_nonInheritedData->backgroundData->background != layers)
        m_computedStyle->m_nonInheritedData.access().backgroundData.access().background = WTF::move(layers);
}

inline void MutatorBase::setMaskLayers(MaskLayers&& layers)
{
    if (m_computedStyle->m_nonInheritedData->miscData->mask != layers)
        m_computedStyle->m_nonInheritedData.access().miscData.access().mask = WTF::move(layers);
}

inline void MutatorBase::setMaskBorder(MaskBorder&& image)
{
    if (m_computedStyle->m_nonInheritedData->rareData->maskBorder->maskBorder != image)
        m_computedStyle->m_nonInheritedData.access().rareData.access().maskBorder.access().maskBorder = WTF::move(image);
}

inline void MutatorBase::setBorderImage(BorderImage&& image)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->border.borderImage->borderImage != image)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().border.borderImage.access().borderImage = WTF::move(image);
}

inline void MutatorBase::setPerspectiveOrigin(PerspectiveOrigin&& origin)
{
    if (m_computedStyle->m_nonInheritedData->rareData->perspectiveOrigin != origin)
        m_computedStyle->m_nonInheritedData.access().rareData.access().perspectiveOrigin = WTF::move(origin);
}

inline void MutatorBase::setTransformOrigin(TransformOrigin&& origin)
{
    if (m_computedStyle->m_nonInheritedData->miscData->transform->origin != origin)
        m_computedStyle->m_nonInheritedData.access().miscData.access().transform.access().origin = WTF::move(origin);
}

inline void MutatorBase::setInsetBox(InsetBox&& box)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->inset != box)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().inset = WTF::move(box);
}

inline void MutatorBase::setMarginBox(MarginBox&& box)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->margin != box)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().margin = WTF::move(box);
}

inline void MutatorBase::setPaddingBox(PaddingBox&& box)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->padding != box)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().padding = WTF::move(box);
}

inline void MutatorBase::setBorderRadius(BorderRadius&& radii)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->border.radii != radii)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().border.radii = WTF::move(radii);
}

void MutatorBase::setBorderTop(BorderValue&& value)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->border.edges.top() != value)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().border.edges.top() = WTF::move(value);
}

void MutatorBase::setBorderRight(BorderValue&& value)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->border.edges.right() != value)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().border.edges.right() = WTF::move(value);
}

void MutatorBase::setBorderBottom(BorderValue&& value)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->border.edges.bottom() != value)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().border.edges.bottom() = WTF::move(value);
}

void MutatorBase::setBorderLeft(BorderValue&& value)
{
    if (m_computedStyle->m_nonInheritedData->surroundData->border.edges.left() != value)
        m_computedStyle->m_nonInheritedData.access().surroundData.access().border.edges.left() = WTF::move(value);
}

// MARK: - Properties/descriptors that are not yet generated

// FIXME: Support generating descriptor setters

inline void MutatorBase::setPageSize(PageSize&& pageSize)
{
    if (m_computedStyle->m_nonInheritedData->rareData->pageSize != pageSize)
        m_computedStyle->m_nonInheritedData.access().rareData.access().pageSize = WTF::move(pageSize);
}

// MARK: - Resets

inline void MutatorBase::resetBorderBottom()
{
    setBorderBottom(BorderValue { });
}

inline void MutatorBase::resetBorderLeft()
{
    setBorderLeft(BorderValue { });
}

inline void MutatorBase::resetBorderRight()
{
    setBorderRight(BorderValue { });
}

inline void MutatorBase::resetBorderTop()
{
    setBorderTop(BorderValue { });
}

inline void MutatorBase::resetMargin()
{
    setMarginBox(0_css_px);
}

inline void MutatorBase::resetPadding()
{
    setPaddingBox(0_css_px);
}

inline void MutatorBase::resetBorder()
{
    resetBorderExceptRadius();
    resetBorderRadius();
}

inline void MutatorBase::resetBorderExceptRadius()
{
    setBorderImage(BorderImage { });
    resetBorderTop();
    resetBorderRight();
    resetBorderBottom();
    resetBorderLeft();
}

inline void MutatorBase::resetBorderRadius()
{
    setBorderRadius(Style::BorderRadius {
        ComputedStyle::initialBorderTopLeftRadius(),
        ComputedStyle::initialBorderTopRightRadius(),
        ComputedStyle::initialBorderBottomLeftRadius(),
        ComputedStyle::initialBorderBottomRightRadius(),
    });
}

} // namespace Style
} // namespace WebCore
