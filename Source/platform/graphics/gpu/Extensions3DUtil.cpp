// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"

#include "public/platform/WebGraphicsContext3D.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

namespace {

void splitStringHelper(const String& str, HashSet<String>& set)
{
    Vector<String> substrings;
    str.split(" ", substrings);
    for (size_t i = 0; i < substrings.size(); ++i)
        set.add(substrings[i]);
}

} // anonymous namespace

Extensions3DUtil::Extensions3DUtil(blink::WebGraphicsContext3D* context)
    : m_context(context)
{
    initializeExtensions();
}

Extensions3DUtil::~Extensions3DUtil()
{
}

void Extensions3DUtil::initializeExtensions()
{
    bool success = m_context->makeContextCurrent();
    ASSERT(success);
    if (!success)
        return;

    String extensionsString = m_context->getString(GL_EXTENSIONS);
    splitStringHelper(extensionsString, m_enabledExtensions);

    String requestableExtensionsString = m_context->getRequestableExtensionsCHROMIUM();
    splitStringHelper(requestableExtensionsString, m_requestableExtensions);
}


bool Extensions3DUtil::supportsExtension(const String& name)
{
    return m_enabledExtensions.contains(name) || m_requestableExtensions.contains(name);
}

bool Extensions3DUtil::ensureExtensionEnabled(const String& name)
{
    if (m_enabledExtensions.contains(name))
        return true;

    if (m_requestableExtensions.contains(name)) {
        m_context->requestExtensionCHROMIUM(name.ascii().data());
        m_enabledExtensions.clear();
        m_requestableExtensions.clear();
        initializeExtensions();
    }
    return m_enabledExtensions.contains(name);
}

bool Extensions3DUtil::isExtensionEnabled(const String& name)
{
    return m_enabledExtensions.contains(name);
}

bool Extensions3DUtil::canUseCopyTextureCHROMIUM(GLenum destFormat, GLenum destType, GLint level)
{
    // FIXME: restriction of (RGB || RGBA)/UNSIGNED_BYTE/(Level 0) should be lifted when
    // WebGraphicsContext3D::copyTextureCHROMIUM(...) are fully functional.
    if ((destFormat == GL_RGB || destFormat == GL_RGBA)
        && destType == GL_UNSIGNED_BYTE
        && !level)
        return true;
    return false;
}

} // namespace WebCore
