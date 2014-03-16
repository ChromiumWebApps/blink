/*
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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

#ifndef PlatformSpeechSynthesizerMock_h
#define PlatformSpeechSynthesizerMock_h

#include "platform/Timer.h"
#include "platform/speech/PlatformSpeechSynthesizer.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class PlatformSpeechSynthesizerMock FINAL : public PlatformSpeechSynthesizer {
public:
    static PassOwnPtr<PlatformSpeechSynthesizerMock> create(PlatformSpeechSynthesizerClient*);

    virtual ~PlatformSpeechSynthesizerMock();
    virtual void speak(PassRefPtr<PlatformSpeechSynthesisUtterance>) OVERRIDE;
    virtual void pause() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void cancel() OVERRIDE;

private:
    explicit PlatformSpeechSynthesizerMock(PlatformSpeechSynthesizerClient*);
    virtual void initializeVoiceList() OVERRIDE;
    void speakingFinished(Timer<PlatformSpeechSynthesizerMock>*);
    void speakingErrorOccurred(Timer<PlatformSpeechSynthesizerMock>*);

    Timer<PlatformSpeechSynthesizerMock> m_speakingFinishedTimer;
    Timer<PlatformSpeechSynthesizerMock> m_speakingErrorOccurredTimer;
    RefPtr<PlatformSpeechSynthesisUtterance> m_utterance;
};

} // namespace WebCore

#endif // PlatformSpeechSynthesizer_h
