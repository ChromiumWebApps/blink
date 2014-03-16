/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLBodyElement_h
#define HTMLBodyElement_h

#include "core/html/HTMLElement.h"

namespace WebCore {

class Document;

class HTMLBodyElement FINAL : public HTMLElement {
public:
    static PassRefPtr<HTMLBodyElement> create(Document&);
    virtual ~HTMLBodyElement();

    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(blur);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(focus);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(load);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(resize);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(scroll);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(orientationchange);

private:
    explicit HTMLBodyElement(Document&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;

    virtual bool supportsFocus() const OVERRIDE;

    virtual int scrollLeft() OVERRIDE;
    virtual void setScrollLeft(int) OVERRIDE;

    virtual int scrollTop() OVERRIDE;
    virtual void setScrollTop(int) OVERRIDE;

    virtual int scrollHeight() OVERRIDE;
    virtual int scrollWidth() OVERRIDE;
};

} //namespace

#endif
