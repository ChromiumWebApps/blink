/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "modules/websockets/NewWebSocketChannelImpl.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/UniqueIdentifier.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "modules/websockets/WebSocketFrame.h"
#include "platform/Logging.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSocketHandshakeRequestInfo.h"
#include "public/platform/WebSocketHandshakeResponseInfo.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"

using blink::WebSocketHandle;

namespace WebCore {

class NewWebSocketChannelImpl::BlobLoader FINAL : public FileReaderLoaderClient {
public:
    BlobLoader(PassRefPtr<BlobDataHandle>, NewWebSocketChannelImpl*);
    virtual ~BlobLoader() { }

    void cancel();

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() OVERRIDE { }
    virtual void didReceiveData() OVERRIDE { }
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

private:
    NewWebSocketChannelImpl* m_channel;
    FileReaderLoader m_loader;
};

NewWebSocketChannelImpl::BlobLoader::BlobLoader(PassRefPtr<BlobDataHandle> blobDataHandle, NewWebSocketChannelImpl* channel)
    : m_channel(channel)
    , m_loader(FileReaderLoader::ReadAsArrayBuffer, this)
{
    m_loader.start(channel->executionContext(), blobDataHandle);
}

void NewWebSocketChannelImpl::BlobLoader::cancel()
{
    m_loader.cancel();
    // didFail will be called immediately.
    // |this| is deleted here.
}

void NewWebSocketChannelImpl::BlobLoader::didFinishLoading()
{
    m_channel->didFinishLoadingBlob(m_loader.arrayBufferResult());
    // |this| is deleted here.
}

void NewWebSocketChannelImpl::BlobLoader::didFail(FileError::ErrorCode errorCode)
{
    m_channel->didFailLoadingBlob(errorCode);
    // |this| is deleted here.
}

NewWebSocketChannelImpl::NewWebSocketChannelImpl(ExecutionContext* context, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    : ContextLifecycleObserver(context)
    , m_handle(adoptPtr(blink::Platform::current()->createWebSocketHandle()))
    , m_client(client)
    , m_identifier(0)
    , m_sendingQuota(0)
    , m_receivedDataSizeForFlowControl(receivedDataSizeForFlowControlHighWaterMark * 2) // initial quota
    , m_bufferedAmount(0)
    , m_sentSizeOfTopMessage(0)
    , m_sourceURLAtConstruction(sourceURL)
    , m_lineNumberAtConstruction(lineNumber)
{
    if (context->isDocument() && toDocument(context)->page())
        m_identifier = createUniqueIdentifier();
}

NewWebSocketChannelImpl::~NewWebSocketChannelImpl()
{
    abortAsyncOperations();
}

void NewWebSocketChannelImpl::connect(const KURL& url, const String& protocol)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p connect()", this);
    if (!m_handle)
        return;
    m_url = url;
    Vector<String> protocols;
    // Avoid placing an empty token in the Vector when the protocol string is
    // empty.
    if (!protocol.isEmpty()) {
        // Since protocol is already verified and escaped, we can simply split
        // it.
        protocol.split(", ", true, protocols);
    }
    blink::WebVector<blink::WebString> webProtocols(protocols.size());
    for (size_t i = 0; i < protocols.size(); ++i) {
        webProtocols[i] = protocols[i];
    }
    String origin = executionContext()->securityOrigin()->toString();
    m_handle->connect(url, webProtocols, origin, this);
    flowControlIfNecessary();
    if (m_identifier)
        InspectorInstrumentation::didCreateWebSocket(document(), m_identifier, url, protocol);
}

String NewWebSocketChannelImpl::subprotocol()
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p subprotocol()", this);
    return m_subprotocol;
}

String NewWebSocketChannelImpl::extensions()
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p extensions()", this);
    return m_extensions;
}

WebSocketChannel::SendResult NewWebSocketChannelImpl::send(const String& message)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p sendText(%s)", this, message.utf8().data());
    if (m_identifier) {
        // FIXME: Change the inspector API to show the entire message instead
        // of individual frames.
        CString data = message.utf8();
        WebSocketFrame frame(WebSocketFrame::OpCodeText, data.data(), data.length(), WebSocketFrame::Final | WebSocketFrame::Masked);
        InspectorInstrumentation::didSendWebSocketFrame(document(), m_identifier, frame.opCode, frame.masked, frame.payload, frame.payloadLength);
    }
    m_messages.append(Message(message));
    sendInternal();
    return SendSuccess;
}

WebSocketChannel::SendResult NewWebSocketChannelImpl::send(PassRefPtr<BlobDataHandle> blobDataHandle)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p sendBlob(%s, %s, %llu)", this, blobDataHandle->uuid().utf8().data(), blobDataHandle->type().utf8().data(), blobDataHandle->size());
    if (m_identifier) {
        // FIXME: Change the inspector API to show the entire message instead
        // of individual frames.
        // FIXME: We can't access the data here.
        // Since Binary data are not displayed in Inspector, this does not
        // affect actual behavior.
        WebSocketFrame frame(WebSocketFrame::OpCodeBinary, "", 0, WebSocketFrame::Final | WebSocketFrame::Masked);
        InspectorInstrumentation::didSendWebSocketFrame(document(), m_identifier, frame.opCode, frame.masked, frame.payload, frame.payloadLength);
    }
    m_messages.append(Message(blobDataHandle));
    sendInternal();
    return SendSuccess;
}

WebSocketChannel::SendResult NewWebSocketChannelImpl::send(const ArrayBuffer& buffer, unsigned byteOffset, unsigned byteLength)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p sendArrayBuffer(%p, %u, %u)", this, buffer.data(), byteOffset, byteLength);
    if (m_identifier) {
        // FIXME: Change the inspector API to show the entire message instead
        // of individual frames.
        WebSocketFrame frame(WebSocketFrame::OpCodeBinary, static_cast<const char*>(buffer.data()) + byteOffset, byteLength, WebSocketFrame::Final | WebSocketFrame::Masked);
        InspectorInstrumentation::didSendWebSocketFrame(document(), m_identifier, frame.opCode, frame.masked, frame.payload, frame.payloadLength);
    }
    // buffer.slice copies its contents.
    m_messages.append(buffer.slice(byteOffset, byteOffset + byteLength));
    sendInternal();
    return SendSuccess;
}

unsigned long NewWebSocketChannelImpl::bufferedAmount() const
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p bufferedAmount()", this);
    return m_bufferedAmount;
}

void NewWebSocketChannelImpl::close(int code, const String& reason)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p close(%d, %s)", this, code, reason.utf8().data());
    ASSERT(m_handle);
    unsigned short codeToSend = static_cast<unsigned short>(code == CloseEventCodeNotSpecified ? CloseEventCodeNoStatusRcvd : code);
    m_handle->close(codeToSend, reason);
}

void NewWebSocketChannelImpl::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p fail(%s)", this, reason.utf8().data());
    // m_handle and m_client can be null here.

    if (m_identifier)
        InspectorInstrumentation::didReceiveWebSocketFrameError(document(), m_identifier, reason);
    const String message = "WebSocket connection to '" + m_url.elidedString() + "' failed: " + reason;
    executionContext()->addConsoleMessage(JSMessageSource, level, message, sourceURL, lineNumber);

    if (m_client)
        m_client->didReceiveMessageError();
    // |reason| is only for logging and should not be provided for scripts,
    // hence close reason must be empty.
    handleDidClose(false, CloseEventCodeAbnormalClosure, String());
    // handleDidClose may delete this object.
}

void NewWebSocketChannelImpl::disconnect()
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p disconnect()", this);
    if (m_identifier)
        InspectorInstrumentation::didCloseWebSocket(document(), m_identifier);
    abortAsyncOperations();
    m_handle.clear();
    m_client = 0;
    m_identifier = 0;
}

void NewWebSocketChannelImpl::suspend()
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p suspend()", this);
}

void NewWebSocketChannelImpl::resume()
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p resume()", this);
}

NewWebSocketChannelImpl::Message::Message(const String& text)
    : type(MessageTypeText)
    , text(text.utf8(StrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD)) { }

NewWebSocketChannelImpl::Message::Message(PassRefPtr<BlobDataHandle> blobDataHandle)
    : type(MessageTypeBlob)
    , blobDataHandle(blobDataHandle) { }

NewWebSocketChannelImpl::Message::Message(PassRefPtr<ArrayBuffer> arrayBuffer)
    : type(MessageTypeArrayBuffer)
    , arrayBuffer(arrayBuffer) { }

void NewWebSocketChannelImpl::sendInternal()
{
    ASSERT(m_handle);
    unsigned long bufferedAmount = m_bufferedAmount;
    while (!m_messages.isEmpty() && m_sendingQuota > 0 && !m_blobLoader) {
        bool final = false;
        const Message& message = m_messages.first();
        switch (message.type) {
        case MessageTypeText: {
            WebSocketHandle::MessageType type =
                m_sentSizeOfTopMessage ? WebSocketHandle::MessageTypeContinuation : WebSocketHandle::MessageTypeText;
            size_t size = std::min(static_cast<size_t>(m_sendingQuota), message.text.length() - m_sentSizeOfTopMessage);
            final = (m_sentSizeOfTopMessage + size == message.text.length());
            m_handle->send(final, type, message.text.data() + m_sentSizeOfTopMessage, size);
            m_sentSizeOfTopMessage += size;
            m_sendingQuota -= size;
            break;
        }
        case MessageTypeBlob:
            ASSERT(!m_blobLoader);
            m_blobLoader = adoptPtr(new BlobLoader(message.blobDataHandle, this));
            break;
        case MessageTypeArrayBuffer: {
            WebSocketHandle::MessageType type =
                m_sentSizeOfTopMessage ? WebSocketHandle::MessageTypeContinuation : WebSocketHandle::MessageTypeBinary;
            size_t size = std::min(static_cast<size_t>(m_sendingQuota), message.arrayBuffer->byteLength() - m_sentSizeOfTopMessage);
            final = (m_sentSizeOfTopMessage + size == message.arrayBuffer->byteLength());
            m_handle->send(final, type, static_cast<const char*>(message.arrayBuffer->data()) + m_sentSizeOfTopMessage, size);
            m_sentSizeOfTopMessage += size;
            m_sendingQuota -= size;
            break;
        }
        }
        if (final) {
            m_messages.removeFirst();
            m_sentSizeOfTopMessage = 0;
        }
    }
    if (m_client && m_bufferedAmount != bufferedAmount) {
        m_client->didUpdateBufferedAmount(m_bufferedAmount);
    }
}

void NewWebSocketChannelImpl::flowControlIfNecessary()
{
    if (!m_handle || m_receivedDataSizeForFlowControl < receivedDataSizeForFlowControlHighWaterMark) {
        return;
    }
    m_handle->flowControl(m_receivedDataSizeForFlowControl);
    m_receivedDataSizeForFlowControl = 0;
}

void NewWebSocketChannelImpl::abortAsyncOperations()
{
    if (m_blobLoader) {
        m_blobLoader->cancel();
        m_blobLoader.clear();
    }
}

void NewWebSocketChannelImpl::handleDidClose(bool wasClean, unsigned short code, const String& reason)
{
    m_handle.clear();
    abortAsyncOperations();
    if (!m_client) {
        return;
    }
    WebSocketChannelClient* client = m_client;
    m_client = 0;
    WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
        wasClean ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete;
    client->didClose(m_bufferedAmount, status, code, reason);
    // client->didClose may delete this object.
}

Document* NewWebSocketChannelImpl::document()
{
    ASSERT(m_identifier);
    ExecutionContext* context = executionContext();
    ASSERT(context->isDocument());
    return toDocument(context);
}

void NewWebSocketChannelImpl::didConnect(WebSocketHandle* handle, bool fail, const blink::WebString& selectedProtocol, const blink::WebString& extensions)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didConnect(%p, %d, %s, %s)", this, handle, fail, selectedProtocol.utf8().data(), extensions.utf8().data());
    ASSERT(m_handle);
    ASSERT(handle == m_handle);
    ASSERT(m_client);
    if (fail) {
        failAsError("Cannot connect to " + m_url.string() + ".");
        // failAsError may delete this object.
        return;
    }
    m_subprotocol = selectedProtocol;
    m_extensions = extensions;
    m_client->didConnect();
}

void NewWebSocketChannelImpl::didStartOpeningHandshake(WebSocketHandle* handle, const blink::WebSocketHandshakeRequestInfo& request)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didStartOpeningHandshake(%p)", this, handle);
    if (m_identifier) {
        InspectorInstrumentation::willSendWebSocketHandshakeRequest(document(), m_identifier, &request.toCoreRequest());
        m_handshakeRequest = WebSocketHandshakeRequest::create(request.toCoreRequest());
    }
}

void NewWebSocketChannelImpl::didFinishOpeningHandshake(WebSocketHandle* handle, const blink::WebSocketHandshakeResponseInfo& response)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didFinishOpeningHandshake(%p)", this, handle);
    if (m_identifier) {
        InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(document(), m_identifier, m_handshakeRequest.get(), &response.toCoreResponse());
    }
    m_handshakeRequest.clear();
}

void NewWebSocketChannelImpl::didFail(WebSocketHandle* handle, const blink::WebString& message)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didFail(%p, %s)", this, handle, message.utf8().data());
    // This function is called when the browser is required to fail the
    // WebSocketConnection. Hence we fail this channel by calling
    // |this->failAsError| function.
    failAsError(message);
    // |this| may be deleted.
}

void NewWebSocketChannelImpl::didReceiveData(WebSocketHandle* handle, bool fin, WebSocketHandle::MessageType type, const char* data, size_t size)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didReceiveData(%p, %d, %d, (%p, %zu))", this, handle, fin, type, data, size);
    ASSERT(m_handle);
    ASSERT(handle == m_handle);
    ASSERT(m_client);
    // Non-final frames cannot be empty.
    ASSERT(fin || size);
    switch (type) {
    case WebSocketHandle::MessageTypeText:
        ASSERT(m_receivingMessageData.isEmpty());
        m_receivingMessageTypeIsText = true;
        break;
    case WebSocketHandle::MessageTypeBinary:
        ASSERT(m_receivingMessageData.isEmpty());
        m_receivingMessageTypeIsText = false;
        break;
    case WebSocketHandle::MessageTypeContinuation:
        ASSERT(!m_receivingMessageData.isEmpty());
        break;
    }

    m_receivingMessageData.append(data, size);
    m_receivedDataSizeForFlowControl += size;
    flowControlIfNecessary();
    if (!fin) {
        return;
    }
    if (m_identifier) {
        // FIXME: Change the inspector API to show the entire message instead
        // of individual frames.
        WebSocketFrame::OpCode opcode = m_receivingMessageTypeIsText ? WebSocketFrame::OpCodeText : WebSocketFrame::OpCodeBinary;
        WebSocketFrame frame(opcode, m_receivingMessageData.data(), m_receivingMessageData.size(), WebSocketFrame::Final);
        InspectorInstrumentation::didReceiveWebSocketFrame(document(), m_identifier, frame.opCode, frame.masked, frame.payload, frame.payloadLength);
    }
    if (m_receivingMessageTypeIsText) {
        String message = m_receivingMessageData.isEmpty() ? emptyString() : String::fromUTF8(m_receivingMessageData.data(), m_receivingMessageData.size());
        m_receivingMessageData.clear();
        if (message.isNull()) {
            failAsError("Could not decode a text frame as UTF-8.");
            // failAsError may delete this object.
        } else {
            m_client->didReceiveMessage(message);
        }
    } else {
        OwnPtr<Vector<char> > binaryData = adoptPtr(new Vector<char>);
        binaryData->swap(m_receivingMessageData);
        m_client->didReceiveBinaryData(binaryData.release());
    }
}

void NewWebSocketChannelImpl::didClose(WebSocketHandle* handle, bool wasClean, unsigned short code, const blink::WebString& reason)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didClose(%p, %d, %u, %s)", this, handle, wasClean, code, String(reason).utf8().data());
    ASSERT(m_handle);
    m_handle.clear();
    if (m_identifier) {
        InspectorInstrumentation::didCloseWebSocket(document(), m_identifier);
        m_identifier = 0;
    }

    handleDidClose(wasClean, code, reason);
    // handleDidClose may delete this object.
}

void NewWebSocketChannelImpl::didReceiveFlowControl(WebSocketHandle* handle, int64_t quota)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didReceiveFlowControl(%p, %ld)", this, handle, static_cast<long>(quota));
    ASSERT(m_handle);
    m_sendingQuota += quota;
    sendInternal();
}

void NewWebSocketChannelImpl::didStartClosingHandshake(WebSocketHandle* handle)
{
    WTF_LOG(Network, "NewWebSocketChannelImpl %p didStartClosingHandshake(%p)", this, handle);
    if (m_client)
        m_client->didStartClosingHandshake();
}

void NewWebSocketChannelImpl::didFinishLoadingBlob(PassRefPtr<ArrayBuffer> buffer)
{
    m_blobLoader.clear();
    ASSERT(m_handle);
    // The loaded blob is always placed on m_messages[0].
    ASSERT(m_messages.size() > 0 && m_messages.first().type == MessageTypeBlob);
    // We replace it with the loaded blob.
    m_messages.first() = Message(buffer);
    sendInternal();
}

void NewWebSocketChannelImpl::didFailLoadingBlob(FileError::ErrorCode errorCode)
{
    m_blobLoader.clear();
    if (errorCode == FileError::ABORT_ERR) {
        // The error is caused by cancel().
        return;
    }
    // FIXME: Generate human-friendly reason message.
    failAsError("Failed to load Blob: error code = " + String::number(errorCode));
    // |this| can be deleted here.
}

} // namespace WebCore
