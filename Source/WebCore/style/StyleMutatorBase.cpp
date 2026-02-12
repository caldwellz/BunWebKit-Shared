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

#include "config.h"
#include "StyleMutatorBase.h"

#include "FontCascade.h"

namespace WebCore {
namespace Style {

#if ENABLE(TEXT_AUTOSIZING)

// MARK: - Text Autosizing

void MutatorBase::setAutosizeStatus(AutosizeStatus autosizeStatus)
{
    m_computedStyle->m_inheritedFlags.autosizeStatus = autosizeStatus.fields().toRaw();
}

#endif // ENABLE(TEXT_AUTOSIZING)

// MARK: - Pseudo element/style

RenderStyle* MutatorBase::addCachedPseudoStyle(std::unique_ptr<RenderStyle> pseudo)
{
    if (!pseudo)
        return nullptr;

    ASSERT(pseudo->pseudoElementType());

    auto* result = pseudo.get();
    m_computedStyle->m_cachedPseudoStyles.add(*result->pseudoElementIdentifier(), WTF::move(pseudo));
    return result;
}

// MARK: - Custom properties

void MutatorBase::setCustomPropertyValue(Ref<const CustomProperty>&& value, bool isInherited)
{
    auto& name = value->name();
    if (isInherited) {
        if (RefPtr existingValue = m_computedStyle->m_inheritedRareData->customProperties->get(name); !existingValue || *existingValue != value.get())
            m_computedStyle->m_inheritedRareData.access().customProperties.access().set(name, WTF::move(value));
    } else {
        if (RefPtr existingValue = m_computedStyle->m_nonInheritedData->rareData->customProperties->get(name); !existingValue || *existingValue != value.get())
            m_computedStyle->m_nonInheritedData.access().rareData.access().customProperties.access().set(name, WTF::move(value));
    }
}

void MutatorBase::deduplicateCustomProperties(const MutatorBase& other)
{
    auto deduplicate = [&] <typename T> (const DataRef<T>& data, const DataRef<T>& otherData) {
        auto& properties = const_cast<DataRef<CustomPropertyData>&>(data->customProperties);
        auto& otherProperties = otherData->customProperties;
        if (properties.ptr() == otherProperties.ptr() || *properties != *otherProperties)
            return;
        properties = otherProperties;
    };

    deduplicate(m_inheritedRareData, other.m_inheritedRareData);
    deduplicate(m_nonInheritedData->rareData, other.m_nonInheritedData->rareData);
}

// MARK: - Custom paint

void MutatorBase::addCustomPaintWatchProperty(const AtomString& name)
{
    Ref data = m_computedStyle->m_nonInheritedData.access().rareData.access();
    data->customPaintWatchedProperties.add(name);
}

// MARK: - FontCascade support.

FontCascade& MutatorBase::mutableFontCascadeWithoutUpdate()
{
    return m_computedStyle->m_inheritedData.access().fontData.access().fontCascade;
}

void MutatorBase::setFontCascade(FontCascade&& fontCascade)
{
    if (fontCascade == this->fontCascade())
        return;

    m_computedStyle->m_inheritedData.access().fontData.access().fontCascade = fontCascade;
}

// MARK: - FontCascadeDescription support.

FontCascadeDescription& MutatorBase::mutableFontDescriptionWithoutUpdate()
{
    auto& cascade = m_computedStyle->m_inheritedData.access().fontData.access().fontCascade;
    return cascade.mutableFontDescription();
}

void MutatorBase::setFontDescription(FontCascadeDescription&& description)
{
    if (fontDescription() == description)
        return;

    auto existingFontCascade = this->fontCascade();
    RefPtr fontSelector = existingFontCascade.fontSelector();

    auto newCascade = FontCascade { WTF::move(description), existingFontCascade };
    newCascade.update(WTF::move(fontSelector));
    setFontCascade(WTF::move(newCascade));
}

bool MutatorBase::setFontDescriptionWithoutUpdate(FontCascadeDescription&& description)
{
    if (fontDescription() == description)
        return false;

    auto& cascade = m_computedStyle->m_inheritedData.access().fontData.access().fontCascade;
    cascade = { WTF::move(description), cascade };
    return true;
}

#if ENABLE(TEXT_AUTOSIZING)

void MutatorBase::setSpecifiedLineHeight(LineHeight&& value)
{
    if (value != m_computedStyle->m_inheritedData->specifiedLineHeight)
        m_computedStyle->m_inheritedData.access().specifiedLineHeight = WTF::move(value);
}

#endif

void MutatorBase::setLetterSpacingFromAnimation(LetterSpacing&& value)
{
    if (value != m_computedStyle->m_inheritedData->fontData->letterSpacing) {
        m_computedStyle->m_inheritedData.access().fontData.access().letterSpacing = value;

        synchronizeLetterSpacingWithFontCascade();
    }
}

void MutatorBase::setWordSpacingFromAnimation(WordSpacing&& value)
{
    if (value != m_computedStyle->m_inheritedData->fontData->wordSpacing) {
        m_computedStyle->m_inheritedData.access().fontData.access().wordSpacing = value;

        synchronizeWordSpacingWithFontCascade();
    }
}

void MutatorBase::synchronizeLetterSpacingWithFontCascade()
{
    auto& fontCascade = mutableFontCascadeWithoutUpdate();
    auto fontSize = fontCascade.size();

    auto newLetterSpacing = evaluate<float>(m_inheritedData->fontData->letterSpacing, fontSize, usedZoomForLength());

    if (newLetterSpacing != fontCascade.letterSpacing()) {
        fontCascade.setLetterSpacing(newLetterSpacing);

        auto oldFontDescription = fontDescription();

        bool oldShouldDisableLigatures = oldFontDescription.shouldDisableLigaturesForSpacing();
        bool newShouldDisableLigatures = newLetterSpacing != 0;

        // Switching letter-spacing between zero and non-zero requires updating to enable/disable ligatures.
        if (oldShouldDisableLigatures != newShouldDisableLigatures) {
            auto newFontDescription = oldFontDescription;
            newFontDescription.setShouldDisableLigaturesForSpacing(newShouldDisableLigatures);
            setFontDescription(WTF::move(newFontDescription));
        }
    }
}

void MutatorBase::synchronizeLetterSpacingWithFontCascadeWithoutUpdate()
{
    auto& fontCascade = mutableFontCascadeWithoutUpdate();
    auto fontSize = fontCascade.size();

    auto newLetterSpacing = evaluate<float>(m_inheritedData->fontData->letterSpacing, fontSize, usedZoomForLength());

    if (newLetterSpacing != fontCascade.letterSpacing()) {
        fontCascade.setLetterSpacing(newLetterSpacing);

        auto oldFontDescription = fontDescription();

        bool oldShouldDisableLigatures = oldFontDescription.shouldDisableLigaturesForSpacing();
        bool newShouldDisableLigatures = newLetterSpacing != 0;

        // Switching letter-spacing between zero and non-zero requires updating to enable/disable ligatures.
        if (oldShouldDisableLigatures != newShouldDisableLigatures) {
            auto newFontDescription = oldFontDescription;
            newFontDescription.setShouldDisableLigaturesForSpacing(newShouldDisableLigatures);
            setFontDescriptionWithoutUpdate(WTF::move(newFontDescription));
        }
    }
}

void MutatorBase::synchronizeWordSpacingWithFontCascade()
{
    auto& fontCascade = mutableFontCascadeWithoutUpdate();
    auto fontSize = fontCascade.size();

    auto newWordSpacing = evaluate<float>(m_inheritedData->fontData->wordSpacing, fontSize, usedZoomForLength());

    if (newWordSpacing != fontCascade.wordSpacing())
        fontCascade.setWordSpacing(newWordSpacing);
}

void MutatorBase::synchronizeWordSpacingWithFontCascadeWithoutUpdate()
{
    synchronizeWordSpacingWithFontCascade();
}

// MARK: - Used Counter Directives

void MutatorBase::updateUsedCounterIncrementDirectives()
{
    auto& map = m_computedStyle->m_nonInheritedData.access().rareData.access().usedCounterDirectives.map;

    for (auto& keyValue : map)
        keyValue.value.incrementValue = std::nullopt;

    for (auto& counterIncrementValue : m_computedStyle->m_nonInheritedData->rareData->counterIncrement) {
        auto& directives = map.add(counterIncrementValue.name.value, CounterDirectives { }).iterator->value;
        directives.incrementValue = saturatedSum(directives.incrementValue.value_or(0), counterIncrementValue.value.value);
    }
}

void MutatorBase::updateUsedCounterResetDirectives()
{
    auto& map = m_computedStyle->m_nonInheritedData.access().rareData.access().usedCounterDirectives.map;

    for (auto& keyValue : map)
        keyValue.value.resetValue = std::nullopt;

    for (auto& counterResetValue : m_computedStyle->m_nonInheritedData->rareData->counterReset) {
        auto& directives = map.add(counterResetValue.name.value, CounterDirectives { }).iterator->value;
        directives.resetValue = counterResetValue.value.value;
    }
}

void MutatorBase::updateUsedCounterSetDirectives()
{
    auto& map = m_computedStyle->m_nonInheritedData.access().rareData.access().usedCounterDirectives.map;

    for (auto& keyValue : map)
        keyValue.value.setValue = std::nullopt;

    for (auto& counterSetValue : m_computedStyle->m_nonInheritedData->rareData->counterSet) {
        auto& directives = map.add(counterSetValue.name.value, CounterDirectives { }).iterator->value;
        directives.setValue = counterSetValue.value.value;
    }
}

} // namespace Style
} // namespace WebCore
