/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef TextCheckingHelper_h
#define TextCheckingHelper_h

#include "platform/text/TextChecking.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class LocalFrame;
class Range;
class Position;
class SpellCheckerClient;
class TextCheckerClient;
struct TextCheckingResult;

class TextCheckingParagraph {
public:
    explicit TextCheckingParagraph(PassRefPtr<Range> checkingRange);
    TextCheckingParagraph(PassRefPtr<Range> checkingRange, PassRefPtr<Range> paragraphRange);
    ~TextCheckingParagraph();

    int rangeLength() const;
    PassRefPtr<Range> subrange(int characterOffset, int characterCount) const;
    int offsetTo(const Position&, ExceptionState&) const;
    void expandRangeToNextEnd();

    const String& text() const;
    // Why not let clients call these functions on text() themselves?
    String textSubstring(unsigned pos, unsigned len = INT_MAX) const { return text().substring(pos, len); }
    UChar textCharAt(int index) const { return text()[static_cast<unsigned>(index)]; }

    bool isEmpty() const;
    bool isTextEmpty() const { return text().isEmpty(); }
    bool isRangeEmpty() const { return checkingStart() >= checkingEnd(); }

    int checkingStart() const;
    int checkingEnd() const;
    int checkingLength() const;
    String checkingSubstring() const { return textSubstring(checkingStart(), checkingLength()); }

    bool checkingRangeCovers(int location, int length) const { return location < checkingEnd() && location + length > checkingStart(); }
    PassRefPtr<Range> paragraphRange() const;
    PassRefPtr<Range> checkingRange() const { return m_checkingRange; }

private:
    void invalidateParagraphRangeValues();
    PassRefPtr<Range> offsetAsRange() const;

    RefPtr<Range> m_checkingRange;
    mutable RefPtr<Range> m_paragraphRange;
    mutable RefPtr<Range> m_offsetAsRange;
    mutable String m_text;
    mutable int m_checkingStart;
    mutable int m_checkingEnd;
    mutable int m_checkingLength;
};

class TextCheckingHelper {
    WTF_MAKE_NONCOPYABLE(TextCheckingHelper);
public:
    TextCheckingHelper(SpellCheckerClient&, PassRefPtr<Range>);
    ~TextCheckingHelper();

    String findFirstMisspelling(int& firstMisspellingOffset, bool markAll, RefPtr<Range>& firstMisspellingRange);
    String findFirstMisspellingOrBadGrammar(bool checkGrammar, bool& outIsSpelling, int& outFirstFoundOffset, GrammarDetail& outGrammarDetail);
    String findFirstBadGrammar(GrammarDetail& outGrammarDetail, int& outGrammarPhraseOffset, bool markAll);
    void markAllMisspellings(RefPtr<Range>& firstMisspellingRange);
    void markAllBadGrammar();

private:
    SpellCheckerClient* m_client;
    RefPtr<Range> m_range;

    int findFirstGrammarDetail(const Vector<GrammarDetail>& grammarDetails, int badGrammarPhraseLocation, int startOffset, int endOffset, bool markAll) const;
    bool unifiedTextCheckerEnabled() const;
};

void checkTextOfParagraph(TextCheckerClient&, const String&, TextCheckingTypeMask, Vector<TextCheckingResult>&);

bool unifiedTextCheckerEnabled(const LocalFrame*);

} // namespace WebCore

#endif // TextCheckingHelper_h
