/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSFunctionValue_h
#define CSSFunctionValue_h

#include "core/css/CSSValue.h"

namespace WebCore {

class CSSValueList;
struct CSSParserFunction;

class CSSFunctionValue : public CSSValue {
public:
    static PassRefPtrWillBeRawPtr<CSSFunctionValue> create(CSSParserFunction* function)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new CSSFunctionValue(function));
    }

    static PassRefPtrWillBeRawPtr<CSSFunctionValue> create(String name, PassRefPtrWillBeRawPtr<CSSValueList> args)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new CSSFunctionValue(name, args));
    }

    String customCSSText() const;

    bool equals(const CSSFunctionValue&) const;

    CSSValueList* arguments() const { return m_args.get(); }

    void traceAfterDispatch(Visitor*);

private:
    explicit CSSFunctionValue(CSSParserFunction*);
    CSSFunctionValue(String, PassRefPtrWillBeRawPtr<CSSValueList>);

    String m_name;
    RefPtrWillBeMember<CSSValueList> m_args;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFunctionValue, isFunctionValue());

} // namespace WebCore

#endif

