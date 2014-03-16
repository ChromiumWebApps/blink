/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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
 */

#ifndef CSSSelector_h
#define CSSSelector_h

#include "core/dom/QualifiedName.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {
    class CSSSelectorList;

    // This class represents a selector for a StyleRule.

    // CSS selector representation is somewhat complicated and subtle. A representative list of selectors is
    // in CSSSelectorTest; run it in a debug build to see useful debugging output.
    //
    // ** tagHistory() and relation():
    //
    // Selectors are represented as a linked list of simple selectors (defined more or less according to
    // http://www.w3.org/TR/css3-selectors/#simple-selectors-dfn). The tagHistory() method returns the next
    // simple selector in the list. The relation() method returns the relationship of the current simple selector to
    // the one in tagHistory(). For example, the CSS selector .a.b #c is represented as:
    //
    // selectorText(): .a.b .c
    // --> (relation == Descendant)
    //   selectorText(): .a.b
    //   --> (relation == SubSelector)
    //     selectorText(): .b
    //
    // Note that currently a bare selector such as ".a" has a relation() of Descendant. This is a bug - instead the relation should be
    // "None".
    //
    // The order of tagHistory() varies depending on the situation.
    // * Relations using combinators (http://www.w3.org/TR/css3-selectors/#combinators), such as descendant, sibling, etc., are parsed
    //   right-to-left (in the example above, this is why .c is earlier in the tagHistory() chain than .a.b).
    // * SubSelector relations are parsed left-to-right in most cases (such as the .a.b example above); a counter-example is the
    //   ::content pseudo-element. Most (all?) other pseudo elements and pseudo classes are parsed left-to-right.
    // * ShadowPseudo relations are parsed right-to-left. Example: summary::-webkit-details-marker is parsed as:
    //   selectorText(): summary::-webkit-details-marker
    //    --> (relation == ShadowPseudo)
    //     selectorText(): summary
    //
    // ** match():
    //
    // The match of the current simple selector tells us the type of selector, such as class, id, tagname, or pseudo-class.
    // Inline comments in the Match enum give examples of when each type would occur.
    //
    // ** value(), attribute():
    //
    // value() tells you the value of the simple selector. For example, for class selectors, value() will tell you the class string,
    // and for id selectors it will tell you the id(). See below for the special case of attribute selectors.
    //
    // ** Attribute selectors.
    //
    // Attribute selectors return the attribute name in the attribute() method. The value() method returns the value matched against
    // in case of selectors like [attr="value"].
    //
    // ** isCustomPseudoElement():
    //
    // It appears this is used only for pseudo elements that appear in user-agent shadow DOM. They are not exposed to author-created
    // shadow DOM.

    class CSSSelector {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CSSSelector();
        CSSSelector(const CSSSelector&);
        explicit CSSSelector(const QualifiedName&, bool tagIsForNamespaceRule = false);

        ~CSSSelector();

        /**
         * Re-create selector text from selector's data
         */
        String selectorText(const String& = "") const;

        // checks if the 2 selectors (including sub selectors) agree.
        bool operator==(const CSSSelector&) const;

        // tag == -1 means apply to all elements (Selector = *)

        // http://www.w3.org/TR/css3-selectors/#specificity
        // We use 256 as the base of the specificity number system.
        unsigned specificity() const;

        /* how the attribute value has to match.... Default is Exact */
        enum Match {
            Unknown = 0,
            Tag, // Example: div
            Id, // Example: #id
            Class, // example: .class
            Exact, // Example: E[foo="bar"]
            Set, // Example: E[foo]
            List, // Example: E[foo~="bar"]
            Hyphen, // Example: E[foo|="bar"]
            PseudoClass, // Example:  :nth-child(2)
            PseudoElement, // Example: ::first-line
            Contain, // css3: E[foo*="bar"]
            Begin, // css3: E[foo^="bar"]
            End, // css3: E[foo$="bar"]
            PagePseudoClass // ??
        };

        enum Relation {
            Descendant = 0, // "Space" combinator
            Child, // > combinator
            DirectAdjacent, // + combinator
            IndirectAdjacent, // ~ combinator
            SubSelector, // "No space" combinator
            ShadowPseudo, // Special case of shadow DOM pseudo elements
            // FIXME: implement named combinator (i.e. named relation) and
            // replace the following /shadow/ and /shadow-deep/ with the implementation.
            Shadow, // /shadow/ combinator
            ShadowDeep, // /shadow-deep/ combinator
            ShadowContent // /content/ for shadow styling
        };

        enum PseudoType {
            PseudoNotParsed = 0,
            PseudoUnknown,
            PseudoEmpty,
            PseudoFirstChild,
            PseudoFirstOfType,
            PseudoLastChild,
            PseudoLastOfType,
            PseudoOnlyChild,
            PseudoOnlyOfType,
            PseudoFirstLine,
            PseudoFirstLetter,
            PseudoNthChild,
            PseudoNthOfType,
            PseudoNthLastChild,
            PseudoNthLastOfType,
            PseudoLink,
            PseudoVisited,
            PseudoAny,
            PseudoAnyLink,
            PseudoAutofill,
            PseudoHover,
            PseudoDrag,
            PseudoFocus,
            PseudoActive,
            PseudoChecked,
            PseudoEnabled,
            PseudoFullPageMedia,
            PseudoDefault,
            PseudoDisabled,
            PseudoOptional,
            PseudoRequired,
            PseudoReadOnly,
            PseudoReadWrite,
            PseudoValid,
            PseudoInvalid,
            PseudoIndeterminate,
            PseudoTarget,
            PseudoBefore,
            PseudoAfter,
            PseudoBackdrop,
            PseudoLang,
            PseudoNot,
            PseudoResizer,
            PseudoRoot,
            PseudoScope,
            PseudoScrollbar,
            PseudoScrollbarBack,
            PseudoScrollbarButton,
            PseudoScrollbarCorner,
            PseudoScrollbarForward,
            PseudoScrollbarThumb,
            PseudoScrollbarTrack,
            PseudoScrollbarTrackPiece,
            PseudoWindowInactive,
            PseudoCornerPresent,
            PseudoDecrement,
            PseudoIncrement,
            PseudoHorizontal,
            PseudoVertical,
            PseudoStart,
            PseudoEnd,
            PseudoDoubleButton,
            PseudoSingleButton,
            PseudoNoButton,
            PseudoSelection,
            PseudoLeftPage,
            PseudoRightPage,
            PseudoFirstPage,
            PseudoFullScreen,
            PseudoFullScreenDocument,
            PseudoFullScreenAncestor,
            PseudoInRange,
            PseudoOutOfRange,
            PseudoUserAgentCustomElement,
            PseudoWebKitCustomElement,
            PseudoCue,
            PseudoFutureCue,
            PseudoPastCue,
            PseudoDistributed,
            PseudoUnresolved,
            PseudoHost,
            PseudoAncestor
        };

        enum OptionalPseudoTypeRequirements {
            // 0 is used to mean "no requirements".
            RequiresShadowDOM = 1
        };

        enum MarginBoxType {
            TopLeftCornerMarginBox,
            TopLeftMarginBox,
            TopCenterMarginBox,
            TopRightMarginBox,
            TopRightCornerMarginBox,
            BottomLeftCornerMarginBox,
            BottomLeftMarginBox,
            BottomCenterMarginBox,
            BottomRightMarginBox,
            BottomRightCornerMarginBox,
            LeftTopMarginBox,
            LeftMiddleMarginBox,
            LeftBottomMarginBox,
            RightTopMarginBox,
            RightMiddleMarginBox,
            RightBottomMarginBox,
        };

        PseudoType pseudoType() const
        {
            if (m_pseudoType == PseudoNotParsed)
                extractPseudoType();
            return static_cast<PseudoType>(m_pseudoType);
        }

        static PseudoType parsePseudoType(const AtomicString&);
        static PseudoId pseudoId(PseudoType);

        // Selectors are kept in an array by CSSSelectorList. The next component of the selector is
        // the next item in the array.
        const CSSSelector* tagHistory() const { return m_isLastInTagHistory ? 0 : const_cast<CSSSelector*>(this + 1); }

        const QualifiedName& tagQName() const;
        const AtomicString& value() const;

        // WARNING: Use of QualifiedName by attribute() is a lie.
        // attribute() will return a QualifiedName with prefix and namespaceURI
        // set to starAtom to mean "matches any namespace". Be very careful
        // how you use the returned QualifiedName.
        // http://www.w3.org/TR/css3-selectors/#attrnmsp
        const QualifiedName& attribute() const;
        // Returns the argument of a parameterized selector. For example, nth-child(2) would have an argument of 2.
        const AtomicString& argument() const { return m_hasRareData ? m_data.m_rareData->m_argument : nullAtom; }
        const CSSSelectorList* selectorList() const { return m_hasRareData ? m_data.m_rareData->m_selectorList.get() : 0; }

#ifndef NDEBUG
        void show() const;
        void show(int indent) const;
#endif

        void setValue(const AtomicString&);
        void setAttribute(const QualifiedName&);
        void setArgument(const AtomicString&);
        void setSelectorList(PassOwnPtr<CSSSelectorList>);
        void setMatchUserAgentOnly();

        bool parseNth() const;
        bool matchNth(int count) const;

        bool matchesPseudoElement() const;
        bool isUnknownPseudoElement() const;
        bool isCustomPseudoElement() const;
        bool isDirectAdjacentSelector() const { return m_relation == DirectAdjacent; }
        bool isSiblingSelector() const;
        bool isAttributeSelector() const;
        bool isDistributedPseudoElement() const;
        bool isHostPseudoClass() const;

        // FIXME: selectors with no tagHistory() get a relation() of Descendant. It should instead be
        // None.
        Relation relation() const { return static_cast<Relation>(m_relation); }

        bool isLastInSelectorList() const { return m_isLastInSelectorList; }
        void setLastInSelectorList() { m_isLastInSelectorList = true; }
        bool isLastInTagHistory() const { return m_isLastInTagHistory; }
        void setNotLastInTagHistory() { m_isLastInTagHistory = false; }

        // http://dev.w3.org/csswg/selectors4/#compound
        bool isCompound() const;

        bool isForPage() const { return m_isForPage; }
        void setForPage() { m_isForPage = true; }

        bool relationIsAffectedByPseudoContent() const { return m_relationIsAffectedByPseudoContent; }
        void setRelationIsAffectedByPseudoContent() { m_relationIsAffectedByPseudoContent = true; }

        unsigned m_relation           : 4; // enum Relation
        mutable unsigned m_match      : 4; // enum Match
        mutable unsigned m_pseudoType : 8; // PseudoType

    private:
        mutable unsigned m_parsedNth      : 1; // Used for :nth-*
        unsigned m_isLastInSelectorList   : 1;
        unsigned m_isLastInTagHistory     : 1;
        unsigned m_hasRareData            : 1;
        unsigned m_isForPage              : 1;
        unsigned m_tagIsForNamespaceRule  : 1;
        unsigned m_relationIsAffectedByPseudoContent  : 1;

        unsigned specificityForOneSelector() const;
        unsigned specificityForPage() const;
        void extractPseudoType() const;

        // Hide.
        CSSSelector& operator=(const CSSSelector&);

        struct RareData : public RefCounted<RareData> {
            static PassRefPtr<RareData> create(const AtomicString& value) { return adoptRef(new RareData(value)); }
            ~RareData();

            bool parseNth();
            bool matchNth(int count);

            AtomicString m_value;
            int m_a; // Used for :nth-*
            int m_b; // Used for :nth-*
            QualifiedName m_attribute; // used for attribute selector
            AtomicString m_argument; // Used for :contains, :lang, :nth-*
            OwnPtr<CSSSelectorList> m_selectorList; // Used for :-webkit-any and :not

        private:
            RareData(const AtomicString& value);
        };
        void createRareData();

        union DataUnion {
            DataUnion() : m_value(0) { }
            StringImpl* m_value;
            QualifiedName::QualifiedNameImpl* m_tagQName;
            RareData* m_rareData;
        } m_data;
    };

inline const QualifiedName& CSSSelector::attribute() const
{
    ASSERT(isAttributeSelector());
    ASSERT(m_hasRareData);
    return m_data.m_rareData->m_attribute;
}

inline bool CSSSelector::matchesPseudoElement() const
{
    if (m_pseudoType == PseudoUnknown)
        extractPseudoType();
    return m_match == PseudoElement;
}

inline bool CSSSelector::isUnknownPseudoElement() const
{
    return m_match == PseudoElement && m_pseudoType == PseudoUnknown;
}

inline bool CSSSelector::isCustomPseudoElement() const
{
    return m_match == PseudoElement && (m_pseudoType == PseudoUserAgentCustomElement || m_pseudoType == PseudoWebKitCustomElement);
}

inline bool CSSSelector::isHostPseudoClass() const
{
    return m_match == PseudoClass && m_pseudoType == PseudoHost;
}

inline bool CSSSelector::isSiblingSelector() const
{
    PseudoType type = pseudoType();
    return m_relation == DirectAdjacent
        || m_relation == IndirectAdjacent
        || type == PseudoEmpty
        || type == PseudoFirstChild
        || type == PseudoFirstOfType
        || type == PseudoLastChild
        || type == PseudoLastOfType
        || type == PseudoOnlyChild
        || type == PseudoOnlyOfType
        || type == PseudoNthChild
        || type == PseudoNthOfType
        || type == PseudoNthLastChild
        || type == PseudoNthLastOfType;
}

inline bool CSSSelector::isAttributeSelector() const
{
    return m_match == CSSSelector::Exact
        || m_match ==  CSSSelector::Set
        || m_match == CSSSelector::List
        || m_match == CSSSelector::Hyphen
        || m_match == CSSSelector::Contain
        || m_match == CSSSelector::Begin
        || m_match == CSSSelector::End;
}

inline bool CSSSelector::isDistributedPseudoElement() const
{
    return m_match == PseudoElement && pseudoType() == PseudoDistributed;
}

inline void CSSSelector::setValue(const AtomicString& value)
{
    ASSERT(m_match != Tag);
    ASSERT(m_pseudoType == PseudoNotParsed);
    // Need to do ref counting manually for the union.
    if (m_hasRareData) {
        m_data.m_rareData->m_value = value;
        return;
    }
    if (m_data.m_value)
        m_data.m_value->deref();
    m_data.m_value = value.impl();
    m_data.m_value->ref();
}

inline CSSSelector::CSSSelector()
    : m_relation(Descendant)
    , m_match(Unknown)
    , m_pseudoType(PseudoNotParsed)
    , m_parsedNth(false)
    , m_isLastInSelectorList(false)
    , m_isLastInTagHistory(true)
    , m_hasRareData(false)
    , m_isForPage(false)
    , m_tagIsForNamespaceRule(false)
    , m_relationIsAffectedByPseudoContent(false)
{
}

inline CSSSelector::CSSSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
    : m_relation(Descendant)
    , m_match(Tag)
    , m_pseudoType(PseudoNotParsed)
    , m_parsedNth(false)
    , m_isLastInSelectorList(false)
    , m_isLastInTagHistory(true)
    , m_hasRareData(false)
    , m_isForPage(false)
    , m_tagIsForNamespaceRule(tagIsForNamespaceRule)
    , m_relationIsAffectedByPseudoContent(false)
{
    m_data.m_tagQName = tagQName.impl();
    m_data.m_tagQName->ref();
}

inline CSSSelector::CSSSelector(const CSSSelector& o)
    : m_relation(o.m_relation)
    , m_match(o.m_match)
    , m_pseudoType(o.m_pseudoType)
    , m_parsedNth(o.m_parsedNth)
    , m_isLastInSelectorList(o.m_isLastInSelectorList)
    , m_isLastInTagHistory(o.m_isLastInTagHistory)
    , m_hasRareData(o.m_hasRareData)
    , m_isForPage(o.m_isForPage)
    , m_tagIsForNamespaceRule(o.m_tagIsForNamespaceRule)
    , m_relationIsAffectedByPseudoContent(o.m_relationIsAffectedByPseudoContent)
{
    if (o.m_match == Tag) {
        m_data.m_tagQName = o.m_data.m_tagQName;
        m_data.m_tagQName->ref();
    } else if (o.m_hasRareData) {
        m_data.m_rareData = o.m_data.m_rareData;
        m_data.m_rareData->ref();
    } else if (o.m_data.m_value) {
        m_data.m_value = o.m_data.m_value;
        m_data.m_value->ref();
    }
}

inline CSSSelector::~CSSSelector()
{
    if (m_match == Tag)
        m_data.m_tagQName->deref();
    else if (m_hasRareData)
        m_data.m_rareData->deref();
    else if (m_data.m_value)
        m_data.m_value->deref();
}

inline const QualifiedName& CSSSelector::tagQName() const
{
    ASSERT(m_match == Tag);
    return *reinterpret_cast<const QualifiedName*>(&m_data.m_tagQName);
}

inline const AtomicString& CSSSelector::value() const
{
    ASSERT(m_match != Tag);
    if (m_hasRareData)
        return m_data.m_rareData->m_value;
    // AtomicString is really just a StringImpl* so the cast below is safe.
    // FIXME: Perhaps call sites could be changed to accept StringImpl?
    return *reinterpret_cast<const AtomicString*>(&m_data.m_value);
}

} // namespace WebCore

#endif // CSSSelector_h
