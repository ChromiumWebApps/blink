/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DescendantInvalidationSet_h
#define DescendantInvalidationSet_h

#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

class Element;

// Tracks data to determine which elements of a DOM subtree need to have style
// recalculated.
class DescendantInvalidationSet FINAL : public RefCounted<DescendantInvalidationSet> {
public:
    static PassRefPtr<DescendantInvalidationSet> create()
    {
        return adoptRef(new DescendantInvalidationSet);
    }

    void combine(const DescendantInvalidationSet& other);

    void addClass(const AtomicString& className);
    void addId(const AtomicString& id);
    void addTagName(const AtomicString& tagName);

    // Appends the classes in this DescendantInvalidationSet to the vector.
    void getClasses(Vector<AtomicString>& classes) const;

    void setWholeSubtreeInvalid();
    bool wholeSubtreeInvalid() const { return m_allDescendantsMightBeInvalid; }
private:
    DescendantInvalidationSet();

    HashSet<AtomicString>& ensureClassSet();
    HashSet<AtomicString>& ensureIdSet();
    HashSet<AtomicString>& ensureTagNameSet();

    // If true, all descendants might be invalidated, so a full subtree recalc is required.
    bool m_allDescendantsMightBeInvalid;

    // FIXME: optimize this if it becomes a memory issue.
    OwnPtr<HashSet<AtomicString> > m_classes;
    OwnPtr<HashSet<AtomicString> > m_ids;
    OwnPtr<HashSet<AtomicString> > m_tagNames;
};

} // namespace WebCore

#endif // DescendantInvalidationSet_h
