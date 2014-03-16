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

#ifndef WebCryptoAlgorithm_h
#define WebCryptoAlgorithm_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

#if INSIDE_BLINK
#include "wtf/PassOwnPtr.h"
#endif

namespace blink {

enum WebCryptoAlgorithmId {
    WebCryptoAlgorithmIdAesCbc,
    WebCryptoAlgorithmIdHmac,
    WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
    WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
    WebCryptoAlgorithmIdSha1,
    WebCryptoAlgorithmIdSha224, // FIXME: Delete once no longer referenced by chromium.
    WebCryptoAlgorithmIdSha256,
    WebCryptoAlgorithmIdSha384,
    WebCryptoAlgorithmIdSha512,
    WebCryptoAlgorithmIdAesGcm,
    WebCryptoAlgorithmIdRsaOaep,
    WebCryptoAlgorithmIdAesCtr,
    WebCryptoAlgorithmIdAesKw,
#if INSIDE_BLINK
    NumberOfWebCryptoAlgorithmId,
#endif
};

enum WebCryptoAlgorithmParamsType {
    WebCryptoAlgorithmParamsTypeNone,
    WebCryptoAlgorithmParamsTypeAesCbcParams,
    WebCryptoAlgorithmParamsTypeAesKeyGenParams,
    WebCryptoAlgorithmParamsTypeHmacImportParams,
    WebCryptoAlgorithmParamsTypeHmacKeyGenParams,
    WebCryptoAlgorithmParamsTypeRsaKeyGenParams,
    WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams,
    WebCryptoAlgorithmParamsTypeRsaHashedImportParams,
    WebCryptoAlgorithmParamsTypeAesGcmParams,
    WebCryptoAlgorithmParamsTypeRsaOaepParams,
    WebCryptoAlgorithmParamsTypeAesCtrParams,
};

class WebCryptoAesCbcParams;
class WebCryptoAesKeyGenParams;
class WebCryptoHmacImportParams;
class WebCryptoHmacKeyGenParams;
class WebCryptoRsaKeyGenParams;
class WebCryptoAesGcmParams;
class WebCryptoRsaOaepParams;
class WebCryptoAesCtrParams;
class WebCryptoRsaHashedKeyGenParams;
class WebCryptoRsaHashedImportParams;

class WebCryptoAlgorithmParams;
class WebCryptoAlgorithmPrivate;

// The WebCryptoAlgorithm represents a normalized algorithm and its parameters.
//   * Immutable
//   * Threadsafe
//   * Copiable (cheaply)
//
// If WebCryptoAlgorithm "isNull()" then it is invalid to call any of the other
// methods on it (other than destruction, assignment, or isNull()).
class WebCryptoAlgorithm {
public:
#if INSIDE_BLINK
    WebCryptoAlgorithm() { }
    BLINK_PLATFORM_EXPORT WebCryptoAlgorithm(WebCryptoAlgorithmId, PassOwnPtr<WebCryptoAlgorithmParams>);
#endif

    BLINK_PLATFORM_EXPORT static WebCryptoAlgorithm createNull();
    BLINK_PLATFORM_EXPORT static WebCryptoAlgorithm adoptParamsAndCreate(WebCryptoAlgorithmId, WebCryptoAlgorithmParams*);

    ~WebCryptoAlgorithm() { reset(); }

    WebCryptoAlgorithm(const WebCryptoAlgorithm& other) { assign(other); }
    WebCryptoAlgorithm& operator=(const WebCryptoAlgorithm& other)
    {
        assign(other);
        return *this;
    }

    BLINK_PLATFORM_EXPORT bool isNull() const;

    BLINK_PLATFORM_EXPORT WebCryptoAlgorithmId id() const;

    BLINK_PLATFORM_EXPORT WebCryptoAlgorithmParamsType paramsType() const;

    // Retrieves the type-specific parameters. The algorithm contains at most 1
    // type of parameters. Retrieving an invalid parameter will return 0.
    BLINK_PLATFORM_EXPORT const WebCryptoAesCbcParams* aesCbcParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoAesKeyGenParams* aesKeyGenParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoHmacImportParams* hmacImportParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoHmacKeyGenParams* hmacKeyGenParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoRsaKeyGenParams* rsaKeyGenParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoAesGcmParams* aesGcmParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoRsaOaepParams* rsaOaepParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoAesCtrParams* aesCtrParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoRsaHashedImportParams* rsaHashedImportParams() const;
    BLINK_PLATFORM_EXPORT const WebCryptoRsaHashedKeyGenParams* rsaHashedKeyGenParams() const;

private:
    BLINK_PLATFORM_EXPORT void assign(const WebCryptoAlgorithm& other);
    BLINK_PLATFORM_EXPORT void reset();

    WebPrivatePtr<WebCryptoAlgorithmPrivate> m_private;
};

} // namespace blink

#endif
