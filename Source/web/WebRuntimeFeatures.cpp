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
#include "WebRuntimeFeatures.h"

#include "RuntimeEnabledFeatures.h"
#include "WebMediaPlayerClientImpl.h"

using namespace WebCore;

namespace blink {

void WebRuntimeFeatures::enableStableFeatures(bool enable)
{
    // FIXME: This entire method should be removed once it is no longer called by Chromium.
    ASSERT(enable);
}

void WebRuntimeFeatures::enableExperimentalFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableBleedingEdgeFastPaths(bool enable)
{
    ASSERT(enable);
    RuntimeEnabledFeatures::setBleedingEdgeFastPathsEnabled(enable);
    RuntimeEnabledFeatures::setSubpixelFontScalingEnabled(enable || RuntimeEnabledFeatures::subpixelFontScalingEnabled());
    RuntimeEnabledFeatures::setCSSWillChangeEnabled(enable);
}

void WebRuntimeFeatures::enableTestOnlyFeatures(bool enable)
{
    RuntimeEnabledFeatures::setTestFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableApplicationCache(bool enable)
{
    RuntimeEnabledFeatures::setApplicationCacheEnabled(enable);
}

void WebRuntimeFeatures::enableDatabase(bool enable)
{
    RuntimeEnabledFeatures::setDatabaseEnabled(enable);
}

void WebRuntimeFeatures::enableDeviceMotion(bool enable)
{
    RuntimeEnabledFeatures::setDeviceMotionEnabled(enable);
}

void WebRuntimeFeatures::enableDeviceOrientation(bool enable)
{
    RuntimeEnabledFeatures::setDeviceOrientationEnabled(enable);
}

void WebRuntimeFeatures::enableDialogElement(bool enable)
{
    RuntimeEnabledFeatures::setDialogElementEnabled(enable);
}

void WebRuntimeFeatures::enableEncryptedMedia(bool enable)
{
    RuntimeEnabledFeatures::setEncryptedMediaEnabled(enable);
    // FIXME: Hack to allow MediaKeyError to be enabled for either version.
    RuntimeEnabledFeatures::setEncryptedMediaAnyVersionEnabled(
        RuntimeEnabledFeatures::encryptedMediaEnabled()
        || RuntimeEnabledFeatures::prefixedEncryptedMediaEnabled());
}

bool WebRuntimeFeatures::isEncryptedMediaEnabled()
{
    return RuntimeEnabledFeatures::encryptedMediaEnabled();
}

void WebRuntimeFeatures::enablePrefixedEncryptedMedia(bool enable)
{
    RuntimeEnabledFeatures::setPrefixedEncryptedMediaEnabled(enable);
    // FIXME: Hack to allow MediaKeyError to be enabled for either version.
    RuntimeEnabledFeatures::setEncryptedMediaAnyVersionEnabled(
        RuntimeEnabledFeatures::encryptedMediaEnabled()
        || RuntimeEnabledFeatures::prefixedEncryptedMediaEnabled());
}

bool WebRuntimeFeatures::isPrefixedEncryptedMediaEnabled()
{
    return RuntimeEnabledFeatures::prefixedEncryptedMediaEnabled();
}

void WebRuntimeFeatures::enableDirectWrite(bool enable)
{
    RuntimeEnabledFeatures::setDirectWriteEnabled(enable);
    RuntimeEnabledFeatures::setSubpixelFontScalingEnabled(enable || RuntimeEnabledFeatures::subpixelFontScalingEnabled());
}

void WebRuntimeFeatures::enableExperimentalCanvasFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableFastTextAutosizing(bool enable)
{
    RuntimeEnabledFeatures::setFastTextAutosizingEnabled(enable);
}

void WebRuntimeFeatures::enableFileSystem(bool enable)
{
    RuntimeEnabledFeatures::setFileSystemEnabled(enable);
}

void WebRuntimeFeatures::enableGamepad(bool enable)
{
    RuntimeEnabledFeatures::setGamepadEnabled(enable);
}

void WebRuntimeFeatures::enableLazyLayout(bool enable)
{
    // FIXME: Remove this once Chromium stops calling this.
}

void WebRuntimeFeatures::enableLocalStorage(bool enable)
{
    RuntimeEnabledFeatures::setLocalStorageEnabled(enable);
}

void WebRuntimeFeatures::enableMediaPlayer(bool enable)
{
    RuntimeEnabledFeatures::setMediaEnabled(enable);
}

void WebRuntimeFeatures::enableWebKitMediaSource(bool enable)
{
    RuntimeEnabledFeatures::setWebKitMediaSourceEnabled(enable);
}

void WebRuntimeFeatures::enableMediaSource(bool enable)
{
    RuntimeEnabledFeatures::setMediaSourceEnabled(enable);
}

void WebRuntimeFeatures::enableMediaStream(bool enable)
{
    RuntimeEnabledFeatures::setMediaStreamEnabled(enable);
}

void WebRuntimeFeatures::enableNotifications(bool enable)
{
    RuntimeEnabledFeatures::setNotificationsEnabled(enable);
}

void WebRuntimeFeatures::enableNavigatorContentUtils(bool enable)
{
    RuntimeEnabledFeatures::setNavigatorContentUtilsEnabled(enable);
}

void WebRuntimeFeatures::enableOrientationEvent(bool enable)
{
    RuntimeEnabledFeatures::setOrientationEventEnabled(enable);
}

void WebRuntimeFeatures::enablePagePopup(bool enable)
{
    RuntimeEnabledFeatures::setPagePopupEnabled(enable);
}

void WebRuntimeFeatures::enablePeerConnection(bool enable)
{
    RuntimeEnabledFeatures::setPeerConnectionEnabled(enable);
}

void WebRuntimeFeatures::enableRequestAutocomplete(bool enable)
{
    RuntimeEnabledFeatures::setRequestAutocompleteEnabled(enable);
}

void WebRuntimeFeatures::enableScriptedSpeech(bool enable)
{
    RuntimeEnabledFeatures::setScriptedSpeechEnabled(enable);
}

void WebRuntimeFeatures::enableServiceWorker(bool enable)
{
    RuntimeEnabledFeatures::setServiceWorkerEnabled(enable);
}

void WebRuntimeFeatures::enableSessionStorage(bool enable)
{
    RuntimeEnabledFeatures::setSessionStorageEnabled(enable);
}

void WebRuntimeFeatures::enableSpeechInput(bool enable)
{
    RuntimeEnabledFeatures::setSpeechInputEnabled(enable);
}

void WebRuntimeFeatures::enableSpeechSynthesis(bool enable)
{
    RuntimeEnabledFeatures::setSpeechSynthesisEnabled(enable);
}

void WebRuntimeFeatures::enableTouch(bool enable)
{
    RuntimeEnabledFeatures::setTouchEnabled(enable);
}

void WebRuntimeFeatures::enableTouchIconLoading(bool enable)
{
    RuntimeEnabledFeatures::setTouchIconLoadingEnabled(enable);
}

void WebRuntimeFeatures::enableWebAnimationsCSS(bool enable)
{
    // FIXME: Remove this method once the runtime flags are removed from Chromium.
    ASSERT(enable);
}

void WebRuntimeFeatures::enableWebAnimationsSVG(bool enable)
{
    RuntimeEnabledFeatures::setWebAnimationsSVGEnabled(enable);
}

void WebRuntimeFeatures::enableWebAudio(bool enable)
{
    RuntimeEnabledFeatures::setWebAudioEnabled(enable);
}

void WebRuntimeFeatures::enableWebGLDraftExtensions(bool enable)
{
    RuntimeEnabledFeatures::setWebGLDraftExtensionsEnabled(enable);
}

void WebRuntimeFeatures::enableWebMIDI(bool enable)
{
    return RuntimeEnabledFeatures::setWebMIDIEnabled(enable);
}

void WebRuntimeFeatures::enableHTMLImports(bool enable)
{
    RuntimeEnabledFeatures::setHTMLImportsEnabled(enable);
}

void WebRuntimeFeatures::enableXSLT(bool enable)
{
    RuntimeEnabledFeatures::setXSLTEnabled(enable);
}

void WebRuntimeFeatures::enableOverlayScrollbars(bool enable)
{
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::enableOverlayFullscreenVideo(bool enable)
{
    RuntimeEnabledFeatures::setOverlayFullscreenVideoEnabled(enable);
}

void WebRuntimeFeatures::enableSharedWorker(bool enable)
{
    RuntimeEnabledFeatures::setSharedWorkerEnabled(enable);
}

void WebRuntimeFeatures::enableRepaintAfterLayout(bool enable)
{
    RuntimeEnabledFeatures::setRepaintAfterLayoutEnabled(enable);
}

void WebRuntimeFeatures::enableExperimentalWebSocket(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalWebSocketEnabled(enable);
}

void WebRuntimeFeatures::enableTargetedStyleRecalc(bool enable)
{
    RuntimeEnabledFeatures::setTargetedStyleRecalcEnabled(enable);
}

} // namespace blink
