/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "modules/webaudio/PannerNode.h"

#include "core/dom/ExecutionContext.h"
#include "platform/audio/HRTFPanner.h"
#include "modules/webaudio/AudioBufferSourceNode.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "wtf/MathExtras.h"

using namespace std;

namespace WebCore {

static void fixNANs(double &x)
{
    if (std::isnan(x) || std::isinf(x))
        x = 0.0;
}

PannerNode::PannerNode(AudioContext* context, float sampleRate)
    : AudioNode(context, sampleRate)
    , m_panningModel(Panner::PanningModelHRTF)
    , m_position(0, 0, 0)
    , m_orientation(1, 0, 0)
    , m_velocity(0, 0, 0)
    , m_cachedPosition(0, 0, 0)
    , m_cachedOrientation(1, 0, 0)
    , m_cachedVelocity(0, 0, 0)
    , m_lastGain(-1.0)
    , m_cachedAzimuth(0)
    , m_cachedElevation(0)
    , m_cachedDistanceConeGain(0)
    , m_cachedDopplerRate(1)
    , m_connectionCount(0)
{
    // Load the HRTF database asynchronously so we don't block the Javascript thread while creating the HRTF database.
    // The HRTF panner will return zeroes until the database is loaded.
    m_hrtfDatabaseLoader = HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(context->sampleRate());

    ScriptWrappable::init(this);
    addInput(adoptPtr(new AudioNodeInput(this)));
    addOutput(adoptPtr(new AudioNodeOutput(this, 2)));

    // Node-specific default mixing rules.
    m_channelCount = 2;
    m_channelCountMode = ClampedMax;
    m_channelInterpretation = AudioBus::Speakers;

    m_distanceGain = AudioParam::create(context, "distanceGain", 1.0, 0.0, 1.0);
    m_coneGain = AudioParam::create(context, "coneGain", 1.0, 0.0, 1.0);

    m_cachedListener = AudioListener::create();

    setNodeType(NodeTypePanner);

    initialize();
}

PannerNode::~PannerNode()
{
    uninitialize();
}

void PannerNode::pullInputs(size_t framesToProcess)
{
    // We override pullInputs(), so we can detect new AudioSourceNodes which have connected to us when new connections are made.
    // These AudioSourceNodes need to be made aware of our existence in order to handle doppler shift pitch changes.
    if (m_connectionCount != context()->connectionCount()) {
        m_connectionCount = context()->connectionCount();

        // A map for keeping track if we have visited a node or not. This prevents feedback loops
        // from recursing infinitely. See crbug.com/331446.
        HashMap<AudioNode*, bool> visitedNodes;

        // Recursively go through all nodes connected to us
        notifyAudioSourcesConnectedToNode(this, visitedNodes);
    }

    AudioNode::pullInputs(framesToProcess);
}

void PannerNode::process(size_t framesToProcess)
{
    AudioBus* destination = output(0)->bus();

    if (!isInitialized() || !input(0)->isConnected() || !m_panner.get()) {
        destination->zero();
        return;
    }

    AudioBus* source = input(0)->bus();
    if (!source) {
        destination->zero();
        return;
    }

    // HRTFDatabase should be loaded before proceeding for offline audio context when panningModel() is "HRTF".
    if (panningModel() == "HRTF" && !m_hrtfDatabaseLoader->isLoaded()) {
        if (context()->isOfflineContext()) {
            m_hrtfDatabaseLoader->waitForLoaderThreadCompletion();
        } else {
            destination->zero();
            return;
        }
    }

    // The audio thread can't block on this lock, so we call tryLock() instead.
    MutexTryLocker tryLocker(m_pannerLock);
    if (tryLocker.locked()) {
        // Apply the panning effect.
        double azimuth;
        double elevation;
        azimuthElevation(&azimuth, &elevation);

        m_panner->pan(azimuth, elevation, source, destination, framesToProcess);

        // Get the distance and cone gain.
        float totalGain = distanceConeGain();

        // Snap to desired gain at the beginning.
        if (m_lastGain == -1.0)
            m_lastGain = totalGain;

        // Apply gain in-place with de-zippering.
        destination->copyWithGainFrom(*destination, &m_lastGain, totalGain);

        // Update the cached listener in case listener has moved.
        updateCachedListener();
        // Now update the cached source location in case the source has changed.
        updateCachedSourceLocationInfo();
    } else {
        // Too bad - The tryLock() failed. We must be in the middle of changing the panner.
        destination->zero();
    }
}

void PannerNode::initialize()
{
    if (isInitialized())
        return;

    m_panner = Panner::create(m_panningModel, sampleRate(), m_hrtfDatabaseLoader.get());

    AudioNode::initialize();
}

void PannerNode::uninitialize()
{
    if (!isInitialized())
        return;

    m_panner.clear();
    AudioNode::uninitialize();
}

AudioListener* PannerNode::listener()
{
    return context()->listener();
}

String PannerNode::panningModel() const
{
    switch (m_panningModel) {
    case EQUALPOWER:
        return "equalpower";
    case HRTF:
        return "HRTF";
    default:
        ASSERT_NOT_REACHED();
        return "HRTF";
    }
}

void PannerNode::setPanningModel(const String& model)
{
    if (model == "equalpower")
        setPanningModel(EQUALPOWER);
    else if (model == "HRTF")
        setPanningModel(HRTF);
    else
        ASSERT_NOT_REACHED();
}

bool PannerNode::setPanningModel(unsigned model)
{
    switch (model) {
    case EQUALPOWER:
    case HRTF:
        if (!m_panner.get() || model != m_panningModel) {
            // This synchronizes with process().
            MutexLocker processLocker(m_pannerLock);

            OwnPtr<Panner> newPanner = Panner::create(model, sampleRate(), m_hrtfDatabaseLoader.get());
            m_panner = newPanner.release();
            m_panningModel = model;
        }
        break;
    default:
        return false;
    }

    return true;
}

void PannerNode::setPosition(float x, float y, float z)
{
    // FIXME : consider thread safety about m_position in audio thread.
    // See http://crbugs.com/350583.
    FloatPoint3D position = FloatPoint3D(x, y, z);

    if (m_position == position)
        return;

    m_position = position;
}

void PannerNode::setOrientation(float x, float y, float z)
{
    // FIXME : consider thread safety about m_orientation in audio thread.
    // See http://crbugs.com/350583.
    FloatPoint3D orientation = FloatPoint3D(x, y, z);

    if (m_orientation == orientation)
        return;

    m_orientation = orientation;
}

void PannerNode::setVelocity(float x, float y, float z)
{
    // FIXME : consider thread safety about m_velocity in audio thread.
    // See http://crbugs.com/350583.
    FloatPoint3D velocity = FloatPoint3D(x, y, z);

    if (m_velocity == velocity)
        return;

    m_velocity = velocity;
}

String PannerNode::distanceModel() const
{
    switch (const_cast<PannerNode*>(this)->m_distanceEffect.model()) {
    case DistanceEffect::ModelLinear:
        return "linear";
    case DistanceEffect::ModelInverse:
        return "inverse";
    case DistanceEffect::ModelExponential:
        return "exponential";
    default:
        ASSERT_NOT_REACHED();
        return "inverse";
    }
}

void PannerNode::setDistanceModel(const String& model)
{
    if (model == "linear")
        setDistanceModel(DistanceEffect::ModelLinear);
    else if (model == "inverse")
        setDistanceModel(DistanceEffect::ModelInverse);
    else if (model == "exponential")
        setDistanceModel(DistanceEffect::ModelExponential);
    else
        ASSERT_NOT_REACHED();
}

bool PannerNode::setDistanceModel(unsigned model)
{
    switch (model) {
    case DistanceEffect::ModelLinear:
    case DistanceEffect::ModelInverse:
    case DistanceEffect::ModelExponential:
        m_distanceEffect.setModel(static_cast<DistanceEffect::ModelType>(model), true);
        break;
    default:
        return false;
    }

    return true;
}

void PannerNode::calculateAzimuthElevation(double* outAzimuth, double* outElevation)
{
    double azimuth = 0.0;

    // Calculate the source-listener vector
    FloatPoint3D listenerPosition = listener()->position();
    FloatPoint3D sourceListener = m_position - listenerPosition;

    if (sourceListener.isZero()) {
        // degenerate case if source and listener are at the same point
        *outAzimuth = 0.0;
        *outElevation = 0.0;
        return;
    }

    sourceListener.normalize();

    // Align axes
    FloatPoint3D listenerFront = listener()->orientation();
    FloatPoint3D listenerUp = listener()->upVector();
    FloatPoint3D listenerRight = listenerFront.cross(listenerUp);
    listenerRight.normalize();

    FloatPoint3D listenerFrontNorm = listenerFront;
    listenerFrontNorm.normalize();

    FloatPoint3D up = listenerRight.cross(listenerFrontNorm);

    float upProjection = sourceListener.dot(up);

    FloatPoint3D projectedSource = sourceListener - upProjection * up;
    projectedSource.normalize();

    azimuth = 180.0 * acos(projectedSource.dot(listenerRight)) / piDouble;
    fixNANs(azimuth); // avoid illegal values

    // Source  in front or behind the listener
    double frontBack = projectedSource.dot(listenerFrontNorm);
    if (frontBack < 0.0)
        azimuth = 360.0 - azimuth;

    // Make azimuth relative to "front" and not "right" listener vector
    if ((azimuth >= 0.0) && (azimuth <= 270.0))
        azimuth = 90.0 - azimuth;
    else
        azimuth = 450.0 - azimuth;

    // Elevation
    double elevation = 90.0 - 180.0 * acos(sourceListener.dot(up)) / piDouble;
    fixNANs(elevation); // avoid illegal values

    if (elevation > 90.0)
        elevation = 180.0 - elevation;
    else if (elevation < -90.0)
        elevation = -180.0 - elevation;

    if (outAzimuth)
        *outAzimuth = azimuth;
    if (outElevation)
        *outElevation = elevation;
}


double PannerNode::calculateDopplerRate()
{
    double dopplerShift = 1.0;
    double dopplerFactor = listener()->dopplerFactor();

    if (dopplerFactor > 0.0) {
        double speedOfSound = listener()->speedOfSound();

        const FloatPoint3D &sourceVelocity = m_velocity;
        const FloatPoint3D &listenerVelocity = listener()->velocity();

        // Don't bother if both source and listener have no velocity
        bool sourceHasVelocity = !sourceVelocity.isZero();
        bool listenerHasVelocity = !listenerVelocity.isZero();

        if (sourceHasVelocity || listenerHasVelocity) {
            // Calculate the source to listener vector
            FloatPoint3D listenerPosition = listener()->position();
            FloatPoint3D sourceToListener = m_position - listenerPosition;

            double sourceListenerMagnitude = sourceToListener.length();

            double listenerProjection = sourceToListener.dot(listenerVelocity) / sourceListenerMagnitude;
            double sourceProjection = sourceToListener.dot(sourceVelocity) / sourceListenerMagnitude;

            listenerProjection = -listenerProjection;
            sourceProjection = -sourceProjection;

            double scaledSpeedOfSound = speedOfSound / dopplerFactor;
            listenerProjection = min(listenerProjection, scaledSpeedOfSound);
            sourceProjection = min(sourceProjection, scaledSpeedOfSound);

            dopplerShift = ((speedOfSound - dopplerFactor * listenerProjection) / (speedOfSound - dopplerFactor * sourceProjection));
            fixNANs(dopplerShift); // avoid illegal values

            // Limit the pitch shifting to 4 octaves up and 3 octaves down.
            if (dopplerShift > 16.0)
                dopplerShift = 16.0;
            else if (dopplerShift < 0.125)
                dopplerShift = 0.125;
        }
    }

    return dopplerShift;
}

float PannerNode::calculateDistanceConeGain()
{
    FloatPoint3D listenerPosition = listener()->position();

    double listenerDistance = m_position.distanceTo(listenerPosition);
    double distanceGain = m_distanceEffect.gain(listenerDistance);

    m_distanceGain->setValue(static_cast<float>(distanceGain));

    double coneGain = m_coneEffect.gain(m_position, m_orientation, listenerPosition);

    m_coneGain->setValue(static_cast<float>(coneGain));

    return float(distanceGain * coneGain);
}

void PannerNode::azimuthElevation(double* outAzimuth, double* outElevation)
{
    if (isAzimuthElevationDirty())
        calculateAzimuthElevation(&m_cachedAzimuth, &m_cachedElevation);

    *outAzimuth = m_cachedAzimuth;
    *outElevation = m_cachedElevation;
}

double PannerNode::dopplerRate()
{
    if (isDopplerRateDirty())
        m_cachedDopplerRate = calculateDopplerRate();

    return m_cachedDopplerRate;
}

float PannerNode::distanceConeGain()
{
    if (isDistanceConeGainDirty())
        m_cachedDistanceConeGain = calculateDistanceConeGain();

    return m_cachedDistanceConeGain;
}

bool PannerNode::isAzimuthElevationDirty()
{
    // Do a quick test and return if possible.
    if (m_cachedPosition != m_position)
        return true;

    if (m_cachedListener->position() != listener()->position()
        || m_cachedListener->orientation() != listener()->orientation()
        || m_cachedListener->upVector() != listener()->upVector())
        return true;

    return false;
}

bool PannerNode::isDistanceConeGainDirty()
{
    // Do a quick test and return if possible.
    if (m_cachedPosition != m_position || m_cachedOrientation != m_orientation)
        return true;

    if (m_cachedListener->position() != listener()->position())
        return true;

    return false;
}

bool PannerNode::isDopplerRateDirty()
{
    // Do a quick test and return if possible.
    if (m_cachedPosition != m_position || m_cachedVelocity != m_velocity)
        return true;

    if (m_cachedListener->position() != listener()->position()
        || m_cachedListener->velocity() != listener()->velocity()
        || m_cachedListener->dopplerFactor() != listener()->dopplerFactor()
        || m_cachedListener->speedOfSound() != listener()->speedOfSound())
        return true;

    return false;
}

void PannerNode::notifyAudioSourcesConnectedToNode(AudioNode* node, HashMap<AudioNode*, bool>& visitedNodes)
{
    ASSERT(node);
    if (!node)
        return;

    // First check if this node is an AudioBufferSourceNode. If so, let it know about us so that doppler shift pitch can be taken into account.
    if (node->nodeType() == NodeTypeAudioBufferSource) {
        AudioBufferSourceNode* bufferSourceNode = static_cast<AudioBufferSourceNode*>(node);
        bufferSourceNode->setPannerNode(this);
    } else {
        // Go through all inputs to this node.
        for (unsigned i = 0; i < node->numberOfInputs(); ++i) {
            AudioNodeInput* input = node->input(i);

            // For each input, go through all of its connections, looking for AudioBufferSourceNodes.
            for (unsigned j = 0; j < input->numberOfRenderingConnections(); ++j) {
                AudioNodeOutput* connectedOutput = input->renderingOutput(j);
                AudioNode* connectedNode = connectedOutput->node();
                HashMap<AudioNode*, bool>::iterator iterator = visitedNodes.find(connectedNode);

                // If we've seen this node already, we don't need to process it again. Otherwise,
                // mark it as visited and recurse through the node looking for sources.
                if (iterator == visitedNodes.end()) {
                    visitedNodes.set(connectedNode, true);
                    notifyAudioSourcesConnectedToNode(connectedNode, visitedNodes); // recurse
                }
            }
        }
    }
}

void PannerNode::updateCachedListener()
{
    m_cachedListener->setPosition(listener()->position());
    m_cachedListener->setOrientation(listener()->orientation());
    m_cachedListener->setUpVector(listener()->upVector());
    m_cachedListener->setVelocity(listener()->velocity());
    m_cachedListener->setDopplerFactor(listener()->dopplerFactor());
    m_cachedListener->setSpeedOfSound(listener()->speedOfSound());
}

void PannerNode::updateCachedSourceLocationInfo()
{
    m_cachedPosition = m_position;
    m_cachedOrientation = m_orientation;
    m_cachedVelocity = m_velocity;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
