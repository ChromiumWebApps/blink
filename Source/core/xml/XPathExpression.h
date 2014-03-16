/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XPathExpression_h
#define XPathExpression_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;
class Node;
class XPathNSResolver;
class XPathResult;

namespace XPath {
class Expression;
}

class XPathExpression : public RefCountedWillBeGarbageCollectedFinalized<XPathExpression>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<XPathExpression> create()
    {
        return adoptRefWillBeNoop(new XPathExpression);
    }
    ~XPathExpression();

    static PassRefPtrWillBeRawPtr<XPathExpression> createExpression(const String& expression, PassRefPtrWillBeRawPtr<XPathNSResolver>, ExceptionState&);
    PassRefPtrWillBeRawPtr<XPathResult> evaluate(Node* contextNode, unsigned short type, XPathResult*, ExceptionState&);

    void trace(Visitor*) { }

private:
    XPathExpression()
    {
        ScriptWrappable::init(this);
    }

    XPath::Expression* m_topExpression;
};

}

#endif // XPathExpression_h
