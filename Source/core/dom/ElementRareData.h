/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ElementRareData_h
#define ElementRareData_h

#include "core/animation/ActiveAnimations.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/ClassList.h"
#include "core/html/ime/InputMethodContext.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class HTMLElement;

class ElementRareData : public NodeRareData {
public:
    static PassOwnPtr<ElementRareData> create(RenderObject* renderer) { return adoptPtr(new ElementRareData(renderer)); }

    ~ElementRareData();

    void setPseudoElement(PseudoId, PassRefPtr<PseudoElement>);
    PseudoElement* pseudoElement(PseudoId) const;

    void resetStyleState();
    void resetDynamicRestyleObservations();

    short tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(short index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }

    bool styleAffectedByEmpty() const { return m_styleAffectedByEmpty; }
    void setStyleAffectedByEmpty(bool value) { m_styleAffectedByEmpty = value; }

    bool isInCanvasSubtree() const { return m_isInCanvasSubtree; }
    void setIsInCanvasSubtree(bool value) { m_isInCanvasSubtree = value; }

    bool containsFullScreenElement() { return m_containsFullScreenElement; }
    void setContainsFullScreenElement(bool value) { m_containsFullScreenElement = value; }

    bool isInTopLayer() const { return m_isInTopLayer; }
    void setIsInTopLayer(bool value) { m_isInTopLayer = value; }

    bool childrenAffectedByFocus() const { return m_childrenAffectedByFocus; }
    void setChildrenAffectedByFocus(bool value) { m_childrenAffectedByFocus = value; }
    bool childrenAffectedByHover() const { return m_childrenAffectedByHover; }
    void setChildrenAffectedByHover(bool value) { m_childrenAffectedByHover = value; }
    bool childrenAffectedByActive() const { return m_childrenAffectedByActive; }
    void setChildrenAffectedByActive(bool value) { m_childrenAffectedByActive = value; }
    bool childrenAffectedByDrag() const { return m_childrenAffectedByDrag; }
    void setChildrenAffectedByDrag(bool value) { m_childrenAffectedByDrag = value; }

    bool childrenAffectedByFirstChildRules() const { return m_childrenAffectedByFirstChildRules; }
    void setChildrenAffectedByFirstChildRules(bool value) { m_childrenAffectedByFirstChildRules = value; }
    bool childrenAffectedByLastChildRules() const { return m_childrenAffectedByLastChildRules; }
    void setChildrenAffectedByLastChildRules(bool value) { m_childrenAffectedByLastChildRules = value; }
    bool childrenAffectedByDirectAdjacentRules() const { return m_childrenAffectedByDirectAdjacentRules; }
    void setChildrenAffectedByDirectAdjacentRules(bool value) { m_childrenAffectedByDirectAdjacentRules = value; }
    bool childrenAffectedByForwardPositionalRules() const { return m_childrenAffectedByForwardPositionalRules; }
    void setChildrenAffectedByForwardPositionalRules(bool value) { m_childrenAffectedByForwardPositionalRules = value; }
    bool childrenAffectedByBackwardPositionalRules() const { return m_childrenAffectedByBackwardPositionalRules; }
    void setChildrenAffectedByBackwardPositionalRules(bool value) { m_childrenAffectedByBackwardPositionalRules = value; }
    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }

    CSSStyleDeclaration& ensureInlineCSSStyleDeclaration(Element* ownerElement);

    void clearShadow() { m_shadow = nullptr; }
    ElementShadow* shadow() const { return m_shadow.get(); }
    ElementShadow& ensureShadow()
    {
        if (!m_shadow)
            m_shadow = ElementShadow::create();
        return *m_shadow;
    }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(PassOwnPtr<NamedNodeMap> attributeMap) { m_attributeMap = attributeMap; }

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(PassRefPtr<RenderStyle> computedStyle) { m_computedStyle = computedStyle; }
    void clearComputedStyle() { m_computedStyle = nullptr; }

    ClassList* classList() const { return m_classList.get(); }
    void setClassList(PassOwnPtr<ClassList> classList) { m_classList = classList; }
    void clearClassListValueForQuirksMode()
    {
        if (!m_classList)
            return;
        m_classList->clearValueForQuirksMode();
    }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(PassOwnPtr<DatasetDOMStringMap> dataset) { m_dataset = dataset; }

    LayoutSize minimumSizeForResizing() const { return m_minimumSizeForResizing; }
    void setMinimumSizeForResizing(LayoutSize size) { m_minimumSizeForResizing = size; }

    IntSize savedLayerScrollOffset() const { return m_savedLayerScrollOffset; }
    void setSavedLayerScrollOffset(IntSize size) { m_savedLayerScrollOffset = size; }

    ActiveAnimations* activeAnimations() { return m_activeAnimations.get(); }
    void setActiveAnimations(PassOwnPtr<ActiveAnimations> activeAnimations)
    {
        m_activeAnimations = activeAnimations;
    }

    bool hasPendingResources() const { return m_hasPendingResources; }
    void setHasPendingResources(bool has) { m_hasPendingResources = has; }

    bool hasInputMethodContext() const { return m_inputMethodContext; }
    InputMethodContext& ensureInputMethodContext(HTMLElement* element)
    {
        if (!m_inputMethodContext)
            m_inputMethodContext = InputMethodContext::create(element);
        return *m_inputMethodContext;
    }

    bool hasPseudoElements() const;
    void clearPseudoElements();

private:
    short m_tabIndex;
    unsigned short m_childIndex;
    unsigned m_tabIndexWasSetExplicitly : 1;
    unsigned m_needsFocusAppearanceUpdateSoonAfterAttach : 1;
    unsigned m_styleAffectedByEmpty : 1;
    unsigned m_isInCanvasSubtree : 1;
    unsigned m_containsFullScreenElement : 1;
    unsigned m_isInTopLayer : 1;
    unsigned m_hasPendingResources : 1;
    unsigned m_childrenAffectedByFocus : 1;
    unsigned m_childrenAffectedByHover : 1;
    unsigned m_childrenAffectedByActive : 1;
    unsigned m_childrenAffectedByDrag : 1;
    // Bits for dynamic child matching.
    // We optimize for :first-child and :last-child. The other positional child selectors like nth-child or
    // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
    unsigned m_childrenAffectedByFirstChildRules : 1;
    unsigned m_childrenAffectedByLastChildRules : 1;
    unsigned m_childrenAffectedByDirectAdjacentRules : 1;
    unsigned m_childrenAffectedByForwardPositionalRules : 1;
    unsigned m_childrenAffectedByBackwardPositionalRules : 1;

    LayoutSize m_minimumSizeForResizing;
    IntSize m_savedLayerScrollOffset;
    RefPtr<RenderStyle> m_computedStyle;

    OwnPtr<DatasetDOMStringMap> m_dataset;
    OwnPtr<ClassList> m_classList;
    OwnPtr<ElementShadow> m_shadow;
    OwnPtr<NamedNodeMap> m_attributeMap;
    OwnPtr<InputMethodContext> m_inputMethodContext;
    OwnPtr<ActiveAnimations> m_activeAnimations;
    OwnPtr<InlineCSSStyleDeclaration> m_cssomWrapper;

    RefPtr<PseudoElement> m_generatedBefore;
    RefPtr<PseudoElement> m_generatedAfter;
    RefPtr<PseudoElement> m_backdrop;

    ElementRareData(RenderObject*);
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(LayoutUnit::max(), LayoutUnit::max());
}

inline ElementRareData::ElementRareData(RenderObject* renderer)
    : NodeRareData(renderer)
    , m_tabIndex(0)
    , m_childIndex(0)
    , m_tabIndexWasSetExplicitly(false)
    , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
    , m_styleAffectedByEmpty(false)
    , m_isInCanvasSubtree(false)
    , m_containsFullScreenElement(false)
    , m_isInTopLayer(false)
    , m_hasPendingResources(false)
    , m_childrenAffectedByFocus(false)
    , m_childrenAffectedByHover(false)
    , m_childrenAffectedByActive(false)
    , m_childrenAffectedByDrag(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_minimumSizeForResizing(defaultMinimumSizeForResizing())
{
}

inline ElementRareData::~ElementRareData()
{
    ASSERT(!m_shadow);
    ASSERT(!m_generatedBefore);
    ASSERT(!m_generatedAfter);
    ASSERT(!m_backdrop);
}

inline bool ElementRareData::hasPseudoElements() const
{
    return m_generatedBefore || m_generatedAfter || m_backdrop;
}

inline void ElementRareData::clearPseudoElements()
{
    setPseudoElement(BEFORE, nullptr);
    setPseudoElement(AFTER, nullptr);
    setPseudoElement(BACKDROP, nullptr);
}

inline void ElementRareData::setPseudoElement(PseudoId pseudoId, PassRefPtr<PseudoElement> element)
{
    switch (pseudoId) {
    case BEFORE:
        if (m_generatedBefore)
            m_generatedBefore->dispose();
        m_generatedBefore = element;
        break;
    case AFTER:
        if (m_generatedAfter)
            m_generatedAfter->dispose();
        m_generatedAfter = element;
        break;
    case BACKDROP:
        if (m_backdrop)
            m_backdrop->dispose();
        m_backdrop = element;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

inline PseudoElement* ElementRareData::pseudoElement(PseudoId pseudoId) const
{
    switch (pseudoId) {
    case BEFORE:
        return m_generatedBefore.get();
    case AFTER:
        return m_generatedAfter.get();
    case BACKDROP:
        return m_backdrop.get();
    default:
        return 0;
    }
}

inline void ElementRareData::resetStyleState()
{
    setStyleAffectedByEmpty(false);
    setChildIndex(0);
}

inline void ElementRareData::resetDynamicRestyleObservations()
{
    setChildrenAffectedByFocus(false);
    setChildrenAffectedByHover(false);
    setChildrenAffectedByActive(false);
    setChildrenAffectedByDrag(false);
    setChildrenAffectedByFirstChildRules(false);
    setChildrenAffectedByLastChildRules(false);
    setChildrenAffectedByDirectAdjacentRules(false);
    setChildrenAffectedByForwardPositionalRules(false);
    setChildrenAffectedByBackwardPositionalRules(false);
}

} // namespace

#endif // ElementRareData_h
