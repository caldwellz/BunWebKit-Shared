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

#include "StyleMutatorBase+Inlines.h"

namespace WebCore {
namespace Style {

// FIXME: - Below are property setters that are not yet generated

// FIXME: Support setters that need to return a `bool` value to indicate if the property changed.
inline bool MutatorProperties::setDirection(TextDirection bidiDirection)
{
    if (m_computedStyle->writingMode().computedTextDirection() == bidiDirection)
        return false;
    m_computedStyle->m_inheritedFlags.writingMode.setTextDirection(bidiDirection);
    return true;
}

inline bool MutatorProperties::setTextOrientation(TextOrientation textOrientation)
{
    if (m_computedStyle->writingMode().computedTextOrientation() == textOrientation)
        return false;
    m_inheritedFlags.writingMode.setTextOrientation(textOrientation);
    return true;
}

inline bool MutatorProperties::setWritingMode(StyleWritingMode mode)
{
    if (m_computedStyle->writingMode().computedWritingMode() == mode)
        return false;
    m_computedStyle->m_inheritedFlags.writingMode.setWritingMode(mode);
    return true;
}

inline bool MutatorProperties::setZoom(Zoom zoom)
{
    // Clamp the effective zoom value to avoid overflow in derived computations.
    // This matches other engines values for compatibility.
    constexpr float minEffectiveZoom = 1e-6f;
    constexpr float maxEffectiveZoom = 1e6f;

    m_computedStyle->setUsedZoom(clampTo<float>(m_computedStyle->usedZoom() * evaluate<float>(zoom), minEffectiveZoom, maxEffectiveZoom));

    if (m_computedStyle->m_nonInheritedData->rareData->zoom == zoom))
        return false;
    m_computedStyle->m_nonInheritedData.access().rareData.access().zoom = zoom;
    return true;
}

// FIXME: Support properties that set more than one value when set.

inline void MutatorProperties::setAppearance(StyleAppearance appearance)
{
    if (Ref readable = *m_computedStyle->m_nonInheritedData->miscData; readable->appearance != static_cast<unsigned>(appearance) || readable->usedAppearance != static_cast<unsigned>(appearance)) {
        Ref writable = m_computedStyle->m_nonInheritedData.access().miscData.access();
        writable->appearance = static_cast<unsigned>(appearance);
        writable->usedAppearance = static_cast<unsigned>(appearance)
    }
}

inline void MutatorProperties::setBlendMode(BlendMode mode)
{
    if (m_computedStyle->m_inheritedRareData->rareData->effectiveBlendMode != static_cast<unsigned>(mode))
        m_computedStyle->m_inheritedRareData.access().rareData.access().effectiveBlendMode = static_cast<unsigned>(mode);

    bool nonNormalBlendMode = (mode != BlendMode::Normal);
    if (m_computedStyle->m_inheritedRareData->isInSubtreeWithBlendMode != static_cast<unsigned>(nonNormalBlendMode))
        m_computedStyle->m_inheritedRareData.access().isInSubtreeWithBlendMode = static_cast<unsigned>(nonNormalBlendMode);
}

inline void MutatorProperties::setDisplay(DisplayType value)
{
    m_computedStyle->m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(value);
    m_computedStyle->m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(value);
}

// FIXME: Support generating properties that have their storage spread out

inline void MutatorProperties::setSpecifiedZIndex(ZIndex index)
{
    if (Ref readable = *m_computedStyle->m_nonInheritedData->boxData; readable->hasAutoSpecifiedZIndex != static_cast<uint8_t>(index.m_isAuto) || readable->specifiedZIndexValue != index.value) {
        Ref writable = m_computedStyle->m_nonInheritedData.access().boxData.access();
        writable->hasAutoSpecifiedZIndex = static_cast<uint8_t>(index.m_isAuto);
        writable->specifiedZIndexValue = index.value
    }
}

inline void MutatorProperties::setCursor(Cursor cursor)
{
    m_computedStyle->m_inheritedFlags.cursorType = static_cast<unsigned>(cursor.predefined);
    if (m_computedStyle->m_inheritedRareData->cursorImages != cursor.images)
        m_computedStyle->m_inheritedRareData.access().cursorImages = WTF::move(cursor.images);
}

// MARK: Fonts

inline void MutatorProperties::setTextSpacingTrim(TextSpacingTrim value)
{
    auto description = m_computedStyle->fontDescription();
    description.setTextSpacingTrim(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setTextAutospace(TextAutospace value)
{
    auto description = m_computedStyle->fontDescription();
    description.setTextAutospace(toPlatform(value));
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontSize(float size)
{
    // size must be specifiedSize if Text Autosizing is enabled, but computedSize if text
    // zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).

    ASSERT(std::isfinite(size));
    if (!std::isfinite(size) || size < 0)
        size = 0;
    else
        size = std::min(maximumAllowedFontSize, size);

    auto description = m_computedStyle->fontDescription();
    description.setSpecifiedSize(size);
    description.setComputedSize(size);
    setFontDescription(WTF::move(description));

    // Whenever the font size changes, letter-spacing and word-spacing, which are dependent on font-size, must be re-synchronized.
    synchronizeLetterSpacingWithFontCascade();
    synchronizeWordSpacingWithFontCascade();
}

inline void MutatorProperties::setFontSizeAdjust(FontSizeAdjust sizeAdjust)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontSizeAdjust(sizeAdjust.platform());
    setFontDescription(WTF::move(description));
}

#if ENABLE(VARIATION_FONTS)

inline void MutatorProperties::setFontOpticalSizing(FontOpticalSizing opticalSizing)
{
    auto description = m_computedStyle->fontDescription();
    description.setOpticalSizing(opticalSizing);
    setFontDescription(WTF::move(description));
}

#endif

inline void MutatorProperties::setFontFamily(FontFamilies families)
{
    auto description = m_computedStyle->fontDescription();
    description.setFamilies(families.takePlatform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontFeatureSettings(FontFeatureSettings settings)
{
    auto description = m_computedStyle->fontDescription();
    description.setFeatureSettings(settings.takePlatform());
    setFontDescription(WTF::move(description));
}

#if ENABLE(VARIATION_FONTS)

inline void MutatorProperties::setFontVariationSettings(FontVariationSettings settings)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariationSettings(settings.takePlatform());
    setFontDescription(WTF::move(description));
}

#endif

inline void MutatorProperties::setFontWeight(FontWeight value)
{
    auto description = m_computedStyle->fontDescription();
    description.setWeight(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontWidth(FontWidth value)
{
    auto description = m_computedStyle->fontDescription();
    description.setWidth(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontStyle(FontStyle style)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontStyleSlope(style.platformSlope());
    description.setFontStyleAxis(style.platformAxis());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontPalette(FontPalette value)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontPalette(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontKerning(Kerning value)
{
    auto description = m_computedStyle->fontDescription();
    description.setKerning(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontSmoothing(FontSmoothingMode value)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontSmoothing(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontSynthesisSmallCaps(FontSynthesisLonghandValue value)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontSynthesisSmallCaps(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontSynthesisStyle(FontSynthesisLonghandValue value)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontSynthesisStyle(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontSynthesisWeight(FontSynthesisLonghandValue value)
{
    auto description = m_computedStyle->fontDescription();
    description.setFontSynthesisWeight(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantAlternates(FontVariantAlternates value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantAlternates(value.takePlatform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantCaps(FontVariantCaps value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantCaps(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantEastAsian(FontVariantEastAsian value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantEastAsian(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantEmoji(FontVariantEmoji value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantEmoji(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantLigatures(FontVariantLigatures value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantLigatures(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantNumeric(FontVariantNumeric value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantNumeric(value.platform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setFontVariantPosition(FontVariantPosition value)
{
    auto description = m_computedStyle->fontDescription();
    description.setVariantPosition(value);
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setLocale(WebkitLocale value)
{
    auto description = m_computedStyle->fontDescription();
    description.setSpecifiedLocale(value.takePlatform());
    setFontDescription(WTF::move(description));
}

inline void MutatorProperties::setTextRendering(TextRenderingMode value)
{
    auto description = m_computedStyle->fontDescription();
    description.setTextRenderingMode(value);
    setFontDescription(WTF::move(description));
}

// MARK: Counter Directives

inline void MutatorProperties::didSetCounterIncrement()
{
    updateUsedCounterIncrementDirectives();
}

inline void MutatorProperties::didSetCounterReset()
{
    updateUsedCounterResetDirectives();
}

inline void MutatorProperties::didSetCounterSet()
{
    updateUsedCounterSetDirectives();
}

} // namespace Style
} // namespace WebCore
