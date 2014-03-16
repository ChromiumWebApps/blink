/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/html/canvas/WebGLVertexArrayObjectOES.h"

#include "core/html/canvas/WebGLRenderingContextBase.h"

namespace WebCore {

PassRefPtr<WebGLVertexArrayObjectOES> WebGLVertexArrayObjectOES::create(WebGLRenderingContextBase* ctx, VaoType type)
{
    return adoptRef(new WebGLVertexArrayObjectOES(ctx, type));
}

WebGLVertexArrayObjectOES::WebGLVertexArrayObjectOES(WebGLRenderingContextBase* ctx, VaoType type)
    : WebGLContextObject(ctx)
    , m_type(type)
    , m_hasEverBeenBound(false)
    , m_boundElementArrayBuffer(nullptr)
{
    ScriptWrappable::init(this);
    m_vertexAttribState.resize(ctx->maxVertexAttribs());

    switch (m_type) {
    case VaoTypeDefault:
        break;
    default:
        setObject(context()->webGraphicsContext3D()->createVertexArrayOES());
        break;
    }
}

WebGLVertexArrayObjectOES::~WebGLVertexArrayObjectOES()
{
    deleteObject(0);
}

void WebGLVertexArrayObjectOES::deleteObjectImpl(blink::WebGraphicsContext3D* context3d, Platform3DObject object)
{
    switch (m_type) {
    case VaoTypeDefault:
        break;
    default:
        context()->webGraphicsContext3D()->deleteVertexArrayOES(object);
        break;
    }

    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context3d);

    for (size_t i = 0; i < m_vertexAttribState.size(); ++i) {
        VertexAttribState& state = m_vertexAttribState[i];
        if (state.bufferBinding)
            state.bufferBinding->onDetached(context3d);
    }
}

void WebGLVertexArrayObjectOES::setElementArrayBuffer(PassRefPtr<WebGLBuffer> buffer)
{
    if (buffer)
        buffer->onAttached();
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context()->webGraphicsContext3D());
    m_boundElementArrayBuffer = buffer;

}

void WebGLVertexArrayObjectOES::setVertexAttribState(
    GLuint index, GLsizei bytesPerElement, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset, PassRefPtr<WebGLBuffer> buffer)
{
    GLsizei validatedStride = stride ? stride : bytesPerElement;

    VertexAttribState& state = m_vertexAttribState[index];

    if (buffer)
        buffer->onAttached();
    if (state.bufferBinding)
        state.bufferBinding->onDetached(context()->webGraphicsContext3D());

    state.bufferBinding = buffer;
    state.bytesPerElement = bytesPerElement;
    state.size = size;
    state.type = type;
    state.normalized = normalized;
    state.stride = validatedStride;
    state.originalStride = stride;
    state.offset = offset;
}

void WebGLVertexArrayObjectOES::unbindBuffer(PassRefPtr<WebGLBuffer> buffer)
{
    if (m_boundElementArrayBuffer == buffer) {
        m_boundElementArrayBuffer->onDetached(context()->webGraphicsContext3D());
        m_boundElementArrayBuffer = nullptr;
    }

    for (size_t i = 0; i < m_vertexAttribState.size(); ++i) {
        VertexAttribState& state = m_vertexAttribState[i];
        if (state.bufferBinding == buffer) {
            buffer->onDetached(context()->webGraphicsContext3D());
            state.bufferBinding = nullptr;
        }
    }
}

void WebGLVertexArrayObjectOES::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    VertexAttribState& state = m_vertexAttribState[index];
    state.divisor = divisor;
}

}
