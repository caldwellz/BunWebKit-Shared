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

#include <WebCore/ComputedStyle.h>

namespace WebCore {

class MutatorBase {
public:
    // MARK: - Non-property setters

    inline void setUsesViewportUnits();
    inline void setUsesContainerUnits();
    inline void setUsesTreeCountingFunctions();
    inline void setInsideLink(InsideLink);
    inline void setIsLink(bool);
    inline void setEmptyState(bool);
    inline void setFirstChildState();
    inline void setLastChildState();
    inline void setHasExplicitlyInheritedProperties();
    inline void setDisallowsFastPathInheritance();
    inline void setHasDisplayAffectedByAnimations();
    inline void setTransformStyleForcedToFlat(bool);
    inline void setUsesAnchorFunctions();
    inline void setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis>);
    inline void setIsPopoverInvoker();
    inline void setNativeAppearanceDisabled(bool);
    inline void setInsideDefaultButton(bool);
    inline void setInsideSubmitButton(bool);
    inline void setEventListenerRegionTypes(OptionSet<EventListenerRegionType>);
    inline void setIsForceHidden();
    inline void setAutoRevealsWhenFound();
    inline void setHasAttrContent();
    inline void setEffectiveInert(bool);
    inline void setIsEffectivelyTransparent(bool);
    inline void setEffectiveDisplay(DisplayType);
    inline void setUsedPositionOptionIndex(std::optional<size_t>);
    inline void setUsedAppearance(StyleAppearance);
    inline void setUsedContentVisibility(ContentVisibility);
    inline void setUsedTouchAction(TouchAction);
    inline void setUsedZIndex(ZIndex);
#if HAVE(CORE_MATERIAL)
    inline void setUsedAppleVisualEffectForSubtree(AppleVisualEffect);
#endif
#if ENABLE(TEXT_AUTOSIZING)
    void setAutosizeStatus(AutosizeStatus);
#endif

    // MARK: - Pseudo element/style

    void setPseudoElementIdentifier(std::optional<PseudoElementIdentifier>&&);
    inline void setHasPseudoStyles(EnumSet<PseudoElementType>);
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>);

    // MARK: - Custom properties

    void setCustomPropertyValue(Ref<const CustomProperty>&&, bool isInherited);
    void deduplicateCustomProperties(const ComputedStyleBase&);

    // MARK: - Custom paint

    void addCustomPaintWatchProperty(const AtomString&);

    // MARK: - Zoom

    inline void setEvaluationTimeZoomEnabled(bool);
    inline void setDeviceScaleFactor(float);
    inline void setUseSVGZoomRulesForLength(bool);
    inline bool setUsedZoom(float);

    // MARK: - Fonts

    WEBCORE_EXPORT FontCascade& mutableFontCascadeWithoutUpdate();
    void setFontCascade(FontCascade&&);

    WEBCORE_EXPORT FontCascadeDescription& mutableFontDescriptionWithoutUpdate();
    WEBCORE_EXPORT void setFontDescription(FontCascadeDescription&&);
    bool setFontDescriptionWithoutUpdate(FontCascadeDescription&&);

#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(LineHeight&&);
#endif

    void setLetterSpacingFromAnimation(LetterSpacing&&);
    void setWordSpacingFromAnimation(WordSpacing&&);

    void synchronizeLetterSpacingWithFontCascade();
    void synchronizeLetterSpacingWithFontCascadeWithoutUpdate();
    void synchronizeWordSpacingWithFontCascade();
    void synchronizeWordSpacingWithFontCascadeWithoutUpdate();

    // MARK: - Used Counter Directives

    void updateUsedCounterIncrementDirectives();
    void updateUsedCounterResetDirectives();
    void updateUsedCounterSetDirectives();

    // MARK: - Aggregates

    inline Animations& ensureAnimations();
    inline BackgroundLayers& ensureBackgroundLayers();
    inline MaskLayers& ensureMaskLayers();
    inline Transitions& ensureTransitions();

    inline void setBackgroundLayers(BackgroundLayers&&);
    inline void setBorderImage(BorderImage&&);
    inline void setBorderRadius(BorderRadius&&);
    inline void setBorderTop(BorderValue&&);
    inline void setBorderRight(BorderValue&&);
    inline void setBorderBottom(BorderValue&&);
    inline void setBorderLeft(BorderValue&&);
    inline void setInsetBox(InsetBox&&);
    inline void setMarginBox(MarginBox&&);
    inline void setMaskBorder(MaskBorder&&);
    inline void setMaskLayers(MaskLayers&&);
    inline void setPaddingBox(PaddingBox&&);
    inline void setPerspectiveOrigin(PerspectiveOrigin&&);
    inline void setTransformOrigin(TransformOrigin&&);

    // MARK: - Properties/descriptors that are not yet generated

    // `@page size`
    inline void setPageSize(PageSize&&);

    // MARK: - Resets

    inline void resetBorder();
    inline void resetBorderExceptRadius();
    inline void resetBorderTop();
    inline void resetBorderRight();
    inline void resetBorderBottom();
    inline void resetBorderLeft();
    inline void resetBorderRadius();
    inline void resetMargin();
    inline void resetPadding();

protected:
    MutatorBase(ComputedStyle& computedStyle)
        : m_computedStyle { computedStyle }
    {
    }

    CheckedRef<ComputedStyle> m_computedStyle;
};

} // namespace Style
} // namespace WebCore
