/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGSMILElement_h
#define SVGSMILElement_h

#include "SVGNames.h"
#include "core/svg/SVGElement.h"
#include "core/svg/animation/SMILTime.h"
#include "wtf/HashMap.h"

namespace WebCore {

class ConditionEventListener;
class SMILTimeContainer;
class SVGSMILElement;

template<typename T> class EventSender;
typedef EventSender<SVGSMILElement> SMILEventSender;

// This class implements SMIL interval timing model as needed for SVG animation.
class SVGSMILElement : public SVGElement {
public:
    SVGSMILElement(const QualifiedName&, Document&);
    virtual ~SVGSMILElement();

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    virtual bool hasValidAttributeType() = 0;
    virtual bool hasValidAttributeName();
    virtual void animationAttributeChanged() = 0;

    SMILTimeContainer* timeContainer() const { return m_timeContainer.get(); }

    SVGElement* targetElement() const { return m_targetElement; }
    const QualifiedName& attributeName() const { return m_attributeName; }

    void beginByLinkActivation();

    enum Restart {
        RestartAlways,
        RestartWhenNotActive,
        RestartNever
    };

    Restart restart() const;

    enum FillMode {
        FillRemove,
        FillFreeze
    };

    FillMode fill() const;

    SMILTime dur() const;
    SMILTime repeatDur() const;
    SMILTime repeatCount() const;
    SMILTime maxValue() const;
    SMILTime minValue() const;

    SMILTime elapsed() const;

    SMILTime intervalBegin() const { return m_intervalBegin; }
    SMILTime previousIntervalBegin() const { return m_previousIntervalBegin; }
    SMILTime simpleDuration() const;

    void seekToIntervalCorrespondingToTime(SMILTime elapsed);
    bool progress(SMILTime elapsed, SVGSMILElement* resultsElement, bool seekToTime);
    SMILTime nextProgressTime() const;

    void reset();

    static SMILTime parseClockValue(const String&);
    static SMILTime parseOffsetValue(const String&);

    bool isContributing(SMILTime elapsed) const;
    bool isFrozen() const;

    unsigned documentOrderIndex() const { return m_documentOrderIndex; }
    void setDocumentOrderIndex(unsigned index) { m_documentOrderIndex = index; }

    virtual void resetAnimatedType() = 0;
    virtual void clearAnimatedType(SVGElement* targetElement) = 0;
    virtual void applyResultsToTarget() = 0;

    void connectSyncBaseConditions();
    void connectEventBaseConditions();

    void dispatchPendingEvent(SMILEventSender*);
    void dispatchRepeatEvents(unsigned);

    virtual bool isSVGDiscardElement() const { return false; }

protected:
    void addBeginTime(SMILTime eventTime, SMILTime endTime, SMILTimeWithOrigin::Origin = SMILTimeWithOrigin::ParserOrigin);
    void addEndTime(SMILTime eventTime, SMILTime endTime, SMILTimeWithOrigin::Origin = SMILTimeWithOrigin::ParserOrigin);

    void setInactive() { m_activeState = Inactive; }

    // Sub-classes may need to take action when the target is changed.
    virtual void setTargetElement(SVGElement*);
    virtual void setAttributeName(const QualifiedName&);

private:
    virtual void buildPendingResource() OVERRIDE;
    void clearResourceAndEventBaseReferences();
    void clearConditions();

    virtual void startedActiveInterval() = 0;
    void endedActiveInterval();
    virtual void updateAnimation(float percent, unsigned repeat, SVGSMILElement* resultElement) = 0;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }

    enum BeginOrEnd {
        Begin,
        End
    };

    SMILTime findInstanceTime(BeginOrEnd, SMILTime minimumTime, bool equalsMinimumOK) const;
    void resolveFirstInterval();
    bool resolveNextInterval();
    void resolveInterval(bool first, SMILTime& beginResult, SMILTime& endResult) const;
    SMILTime resolveActiveEnd(SMILTime resolvedBegin, SMILTime resolvedEnd) const;
    SMILTime repeatingDuration() const;

    enum RestartedInterval {
        DidNotRestartInterval,
        DidRestartInterval
    };

    RestartedInterval maybeRestartInterval(SMILTime elapsed);
    void beginListChanged(SMILTime eventTime);
    void endListChanged(SMILTime eventTime);

    // This represents conditions on elements begin or end list that need to be resolved on runtime
    // for example <animate begin="otherElement.begin + 8s; button.click" ... />
    struct Condition {
        enum Type {
            EventBase,
            Syncbase,
            AccessKey
        };

        Condition(Type, BeginOrEnd, const String& baseID, const String& name, SMILTime offset, int repeat = -1);
        Type m_type;
        BeginOrEnd m_beginOrEnd;
        String m_baseID;
        String m_name;
        SMILTime m_offset;
        int m_repeat;
        RefPtr<Element> m_syncbase;
        RefPtr<ConditionEventListener> m_eventListener;
    };
    bool parseCondition(const String&, BeginOrEnd beginOrEnd);
    void parseBeginOrEnd(const String&, BeginOrEnd beginOrEnd);
    SVGElement* eventBaseFor(const Condition&);

    void disconnectSyncBaseConditions();
    void disconnectEventBaseConditions();

    // Event base timing
    void handleConditionEvent(Event*, Condition*);

    void notifyDependentsIntervalChanged();
    void createInstanceTimesFromSyncbase(SVGSMILElement* syncbase);
    void addSyncBaseDependent(SVGSMILElement*);
    void removeSyncBaseDependent(SVGSMILElement*);

    enum ActiveState {
        Inactive,
        Active,
        Frozen
    };

    QualifiedName m_attributeName;

    ActiveState determineActiveState(SMILTime elapsed) const;
    float calculateAnimationPercentAndRepeat(SMILTime elapsed, unsigned& repeat) const;
    SMILTime calculateNextProgressTime(SMILTime elapsed) const;

    mutable SVGElement* m_targetElement;

    Vector<Condition> m_conditions;
    bool m_syncBaseConditionsConnected;
    bool m_hasEndEventConditions;

    bool m_isWaitingForFirstInterval;

    typedef HashSet<SVGSMILElement*> TimeDependentSet;
    TimeDependentSet m_syncBaseDependents;

    // Instance time lists
    Vector<SMILTimeWithOrigin> m_beginTimes;
    Vector<SMILTimeWithOrigin> m_endTimes;

    // This is the upcoming or current interval
    SMILTime m_intervalBegin;
    SMILTime m_intervalEnd;

    SMILTime m_previousIntervalBegin;

    ActiveState m_activeState;
    float m_lastPercent;
    unsigned m_lastRepeat;

    SMILTime m_nextProgressTime;

    RefPtr<SMILTimeContainer> m_timeContainer;
    unsigned m_documentOrderIndex;

    Vector<unsigned> m_repeatEventCountList;

    mutable SMILTime m_cachedDur;
    mutable SMILTime m_cachedRepeatDur;
    mutable SMILTime m_cachedRepeatCount;
    mutable SMILTime m_cachedMin;
    mutable SMILTime m_cachedMax;

    friend class ConditionEventListener;
};

inline bool isSVGSMILElement(const Node& node)
{
    return node.hasTagName(SVGNames::setTag) || node.hasTagName(SVGNames::animateTag) || node.hasTagName(SVGNames::animateMotionTag)
        || node.hasTagName(SVGNames::animateTransformTag) || node.hasTagName((SVGNames::discardTag));
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGSMILElement);

}

#endif // SVGSMILElement_h
