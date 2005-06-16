/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef _DOM_QNAME_h_
#define _DOM_QNAME_h_

#include "dom_atomicstring.h"

namespace DOM {

class QualifiedName {
public:
    class QualifiedNameImpl : public khtml::Shared<QualifiedNameImpl> {
    public:
        QualifiedNameImpl(const AtomicString& p, const AtomicString& l, const AtomicString& n) :m_prefix(p), m_localName(l), m_namespace(n) {}

        friend class QualifiedName;

    private:
        AtomicString m_prefix;
        AtomicString m_localName;
        AtomicString m_namespace;
    };

    QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n);
    QualifiedName(QualifiedNameImpl* inner);
    ~QualifiedName();

    QualifiedName(const QualifiedName& other);
    const QualifiedName& operator=(const QualifiedName& other);

    DOMStringImpl* localNamePtr() const { return localName().implementation(); }
    
    bool operator==(const QualifiedName& other) const { return m_impl == other.m_impl; }
    bool operator!=(const QualifiedName& other) const { return !(*this == other); }

    bool matches(const QualifiedName& other) const { return m_impl == other.m_impl || (localName() == other.localName() && namespaceURI() == other.namespaceURI()); }

    const AtomicString& prefix() const { return m_impl->m_prefix; }
    const AtomicString& localName() const { return m_impl->m_localName; }
    const AtomicString& namespaceURI() const { return m_impl->m_namespace; }

private:

    void ref() { m_impl->ref(); } 
    void deref();
    
    QualifiedNameImpl* m_impl;
};

bool operator==(const AtomicString& a, const QualifiedName& q);
inline bool operator!=(const AtomicString& a, const QualifiedName& q) { return a != q.localName(); }
bool operator==(const QualifiedName& q, const AtomicString& a);
inline bool operator!=(const QualifiedName& q, const AtomicString& a) { return a != q.localName(); }


}
#endif
