/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2017 Translational NeuroEngineering Laboratory, MGH

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "CrossingDetector.h"
#include "CrossingDetectorEditor.h"

#include <cmath> // for ceil, floor

CrossingDetector::CrossingDetector()
    : GenericProcessor      ("Crossing Detector")
    , thresholdType         (CONSTANT)
    , constantThresh        (0.0f)
    , indicatorChan         (-1)
    , indicatorTarget       (180.0f)
    , useIndicatorRange     (true)
    , startLearningRate     (0.02)
    , minLearningRate       (0.005)
    , decayRate             (0.00003)
    , adaptThreshPaused     (false)
    , useAdaptThreshRange   (true)
    , currLearningRate      (startLearningRate)
    , currLRDivisor         (1.0)
    , indicatorChanName     ("")
    , thresholdChannel      (-1)
    , inputChannel          (-1)
    , validSubProcFullID    (0)
    , eventChannel          (0)
    , posOn                 (true)
    , negOn                 (false)
    , eventDuration         (5)
    , timeout               (1000)
    , useBufferEndMask      (false)
    , bufferEndMaskMs       (3)
    , pastStrict            (1.0f)
    , pastSpan              (0)
    , futureStrict          (1.0f)
    , futureSpan            (0)
    , useJumpLimit          (false)
    , jumpLimit             (5.0f)
    , sampToReenable        (pastSpan + futureSpan + 1)
    , pastSamplesAbove      (0)
    , futureSamplesAbove    (0)
    , inputHistory          (pastSpan + futureSpan + 2)
    , thresholdHistory      (pastSpan + futureSpan + 2)
    , eventChannelPtr       (nullptr)
    , turnoffEvent          (nullptr)
{
    setProcessorType(PROCESSOR_TYPE_FILTER);

    indicatorRange[0] = -180.0f;
    indicatorRange[1] = 180.0f;
    adaptThreshRange[0] = -180.0f;
    adaptThreshRange[1] = 180.0f;
    randomThreshRange[0] = -180.0f;
    randomThreshRange[1] = 180.0f;
    thresholdVal = constantThresh;

    // make the event-related metadata descriptors
    eventMetaDataDescriptors.add(new MetaDataDescriptor(MetaDataDescriptor::INT64, 1, "Crossing Point",
        "Time when threshold was crossed", "crossing.point"));
    eventMetaDataDescriptors.add(new MetaDataDescriptor(MetaDataDescriptor::FLOAT, 1, "Crossing level",
        "Voltage level at first sample after crossing", "crossing.level"));
    eventMetaDataDescriptors.add(new MetaDataDescriptor(MetaDataDescriptor::FLOAT, 1, "Threshold",
        "Monitored voltage threshold", "crossing.threshold"));
    eventMetaDataDescriptors.add(new MetaDataDescriptor(MetaDataDescriptor::UINT8, 1, "Direction",
        "Direction of crossing: 1 = rising, 0 = falling", "crossing.direction"));
    eventMetaDataDescriptors.add(new MetaDataDescriptor(MetaDataDescriptor::DOUBLE, 1, "Learning rate",
        "If using adaptive threshold, current threshold learning rate", "crossing.threshold.learningrate"));
}

CrossingDetector::~CrossingDetector() {}

AudioProcessorEditor* CrossingDetector::createEditor()
{
    editor = new CrossingDetectorEditor(this);
    return editor;
}

void CrossingDetector::createEventChannels()
{
    // add detection event channel
    const DataChannel* in = getDataChannel(inputChannel);

    if (!in)
    {
        eventChannelPtr = nullptr;
        return;
    }

    float sampleRate = in->getSampleRate();
    EventChannel* chan = new EventChannel(EventChannel::TTL, 8, 1, sampleRate, this);
    chan->setName("Crossing detector output");
    chan->setDescription("Triggers whenever the input signal crosses a voltage threshold.");
    chan->setIdentifier("crossing.event");

    // metadata storing source data channel

    MetaDataDescriptor sourceChanDesc(MetaDataDescriptor::UINT16, 3, "Source Channel",
        "Index at its source, Source processor ID and Sub Processor index of the channel that triggers this event", "source.channel.identifier.full");
    MetaDataValue sourceChanVal(sourceChanDesc);
    uint16 sourceInfo[3];
    sourceInfo[0] = in->getSourceIndex();
    sourceInfo[1] = in->getSourceNodeID();
    sourceInfo[2] = in->getSubProcessorIdx();
    sourceChanVal.setValue(static_cast<const uint16*>(sourceInfo));
    chan->addMetaData(sourceChanDesc, sourceChanVal);

    // event-related metadata!
    for (auto desc : eventMetaDataDescriptors)
    {
        chan->addEventMetaData(desc);
    }

    eventChannelPtr = eventChannelArray.add(chan);
}

void CrossingDetector::process(AudioSampleBuffer& continuousBuffer)
{
    if (inputChannel < 0 || inputChannel >= continuousBuffer.getNumChannels() || !eventChannelPtr)
    {
        jassertfalse;
        return;
    }

    int nSamples = getNumSamples(inputChannel);
    const float* const rp = continuousBuffer.getReadPointer(inputChannel);
    juce::int64 startTs = getTimestamp(inputChannel);

    // turn off event from previous buffer if necessary
    int turnoffOffset = turnoffEvent ? jmax(0, int(turnoffEvent->getTimestamp() - startTs)) : -1;
    if (turnoffOffset >= 0 && turnoffOffset < nSamples)
    {
        addEvent(eventChannelPtr, turnoffEvent, turnoffOffset);
        turnoffEvent = nullptr;
    }

    const ThresholdType currThreshType = thresholdType;

    // adapt threshold if necessary
    if (currThreshType == ADAPTIVE && indicatorChan > -1)
    {
        checkForEvents();
    }

    // store threshold for each sample of current buffer
    if (currThresholds.size() < nSamples)
    {
        currThresholds.resize(nSamples);
    }
    float* const pThresh = currThresholds.getRawDataPointer();
    const float* const rpThreshChan = currThreshType == CHANNEL
        ? continuousBuffer.getReadPointer(thresholdChannel)
        : nullptr;

    // define lambdas to access history values more easily
    auto inputAt = [=](int index)
    {
        return index < 0 ? inputHistory[index] : rp[index];
    };

    auto thresholdAt = [&, this](int index)
    {
        return index < 0 ? thresholdHistory[index] : pThresh[index];
    };

    // loop over current buffer and add events for newly detected crossings
    for (int i = 0; i < nSamples; ++i)
    {
        // state to keep constant during each iteration
        bool currPosOn = posOn;
        bool currNegOn = negOn;

        // get and save threshold for this sample
        switch (currThreshType)
        {
        case CONSTANT:
        case ADAPTIVE: // adaptive threshold process updates constantThresh
            pThresh[i] = constantThresh;
            break;

        case RANDOM:
            pThresh[i] = currRandomThresh;
            break;

        case CHANNEL:
            pThresh[i] = rpThreshChan[i];
            break;
        }

        int indCross = i - futureSpan;

        // update pastSamplesA`bove and futureSamplesAbove
        if (pastSpan > 0)
        {
            int indLeaving = indCross - 2 - pastSpan;
            if (inputAt(indLeaving) > thresholdAt(indLeaving))
            {
                pastSamplesAbove--;
            }

            int indEntering = indCross - 2;
            if (inputAt(indEntering) > thresholdAt(indEntering))
            {
                pastSamplesAbove++;
            }
        }

        if (futureSpan > 0)
        {
            int indLeaving = indCross;
            if (inputAt(indLeaving) > thresholdAt(indLeaving))
            {
                futureSamplesAbove--;
            }

            int indEntering = indCross + futureSpan; // (== i)
            if (inputAt(indEntering) > thresholdAt(indEntering))
            {
                futureSamplesAbove++;
            }
        }

        if (indCross < sampToReenable ||
            (useBufferEndMask && nSamples - indCross > bufferEndMaskSamp))
        {
            // can't trigger an event now
            continue;
        }

        float preVal = inputAt(indCross - 1);
        float preThresh = thresholdAt(indCross - 1);
        float postVal = inputAt(indCross);
        float postThresh = thresholdAt(indCross);

        // check whether to trigger an event
        if (currPosOn && shouldTrigger(true, preVal, postVal, preThresh, postThresh) ||
            currNegOn && shouldTrigger(false, preVal, postVal, preThresh, postThresh))
        {
            // add event
            triggerEvent(startTs, indCross, nSamples, postThresh, postVal);
            
            // update sampToReenable
            sampToReenable = indCross + 1 + timeoutSamp;

            // if using random thresholds, set a new threshold
            if (currThreshType == RANDOM)
            {
                currRandomThresh = nextRandomThresh();
                thresholdVal = currRandomThresh;
            }
        }
    }

    // update inputHistory and thresholdHistory
    inputHistory.enqueueArray(rp, nSamples);
    thresholdHistory.enqueueArray(pThresh, nSamples);

    // shift sampToReenable so it is relative to the next buffer
    sampToReenable = jmax(0, sampToReenable - nSamples);
}

// all new values should be validated before this function is called!
void CrossingDetector::setParameter(int parameterIndex, float newValue)
{
    switch (parameterIndex)
    {
    case THRESH_TYPE:
        thresholdType = static_cast<ThresholdType>(static_cast<int>(newValue));

        switch (thresholdType)
        {
        case CONSTANT:
            thresholdVal = constantThresh;
            break;

        case ADAPTIVE:
            thresholdVal = constantThresh;
            restartAdaptiveThreshold();
            break;

        case RANDOM:
            // get new random threshold
            currRandomThresh = nextRandomThresh();
            thresholdVal = currRandomThresh;
            break;

        case CHANNEL:
            jassert(isCompatibleWithInput(thresholdChannel));
            thresholdVal = toChannelThreshString(thresholdChannel);
            break;
        }

        break;

    case CONST_THRESH:
        constantThresh = newValue;
        break;

    case INDICATOR_CHAN:
        if (newValue >= 0 && newValue < getTotalEventChannels())
        {
            if (!isValidIndicatorChan(getEventChannel(static_cast<int>(newValue))))
            {
                jassertfalse;
                return;
            }
            indicatorChan = static_cast<int>(newValue);
            indicatorChanName = getEventChannel(indicatorChan)->getName();
        }
        else
        {
            indicatorChan = -1;
            indicatorChanName = "";
        }
        break;

    case INDICATOR_TARGET:
        indicatorTarget = newValue;
        break;

    case USE_INDICATOR_RANGE:
        useIndicatorRange = newValue ? true : false;
        break;

    case MIN_INDICATOR:
        indicatorRange[0] = newValue;
        break;

    case MAX_INDICATOR:
        indicatorRange[1] = newValue;
        break;

    case START_LEARNING_RATE:
        startLearningRate = newValue;
        break;

    case MIN_LEARNING_RATE:
        minLearningRate = newValue;
        break;

    case DECAY_RATE:
        decayRate = newValue;
        break;

    case ADAPT_PAUSED:
        adaptThreshPaused = newValue ? true : false;
        break;

    case USE_THRESH_RANGE:
        useAdaptThreshRange = newValue ? true : false;
        break;

    case MIN_ADAPTED_THRESH:
        adaptThreshRange[0] = newValue;
        break;

    case MAX_ADAPTED_THRESH:
        adaptThreshRange[1] = newValue;
        break;

    case MIN_RAND_THRESH:
        randomThreshRange[0] = newValue;
        currRandomThresh = nextRandomThresh();
        if (thresholdType == RANDOM)
        {
            thresholdVal = currRandomThresh;
        }
        break;

    case MAX_RAND_THRESH:
        randomThreshRange[1] = newValue;
        currRandomThresh = nextRandomThresh();
        if (thresholdType == RANDOM)
        {
            thresholdVal = currRandomThresh;
        }
        break;

    case THRESH_CHAN:
        jassert(isCompatibleWithInput(static_cast<int>(newValue)));
        thresholdChannel = static_cast<int>(newValue);
        if (thresholdType == CHANNEL)
        {
            thresholdVal = toChannelThreshString(thresholdChannel);
        }
        break;

    case INPUT_CHAN:
        inputChannel = static_cast<int>(newValue);
        validSubProcFullID = getSubProcFullID(inputChannel);
   
        // make sure available threshold channels take into account new input channel
        static_cast<CrossingDetectorEditor*>(getEditor())->updateChannelThreshBox();

        // update signal chain, since the event channel metadata has to get updated.
        // pass nullptr instead of a pointer to the editor so that it just updates
        // settings and doesn't try to update the visible editors.
        CoreServices::updateSignalChain(nullptr);
        break;

    case EVENT_CHAN:
        eventChannel = static_cast<int>(newValue);
        break;

    case POS_ON:
        posOn = static_cast<bool>(newValue);
        break;

    case NEG_ON:
        negOn = static_cast<bool>(newValue);
        break;

    case EVENT_DUR:
        eventDuration = static_cast<int>(newValue);
        updateSampleRateDependentValues();
        break;

    case TIMEOUT:
        timeout = static_cast<int>(newValue);
        updateSampleRateDependentValues();
        break;

    case PAST_SPAN:
        pastSpan = static_cast<int>(newValue);
        sampToReenable = pastSpan + futureSpan + 1;

        inputHistory.reset();
        inputHistory.resize(pastSpan + futureSpan + 2);
        thresholdHistory.reset();
        thresholdHistory.resize(pastSpan + futureSpan + 2);

        // counters must reflect current contents of inputHistory and thresholdHistory
        pastSamplesAbove = 0;
        futureSamplesAbove = 0;
        break;

    case PAST_STRICT:
        pastStrict = newValue;
        break;

    case FUTURE_SPAN:
        futureSpan = static_cast<int>(newValue);
        sampToReenable = pastSpan + futureSpan + 1;

        inputHistory.reset();
        inputHistory.resize(pastSpan + futureSpan + 2);
        thresholdHistory.reset();
        thresholdHistory.resize(pastSpan + futureSpan + 2);

        // counters must reflect current contents of inputHistory and thresholdHistory
        pastSamplesAbove = 0;
        futureSamplesAbove = 0;
        break;

    case FUTURE_STRICT:
        futureStrict = newValue;
        break;

    case USE_JUMP_LIMIT:
        useJumpLimit = static_cast<bool>(newValue);
        break;

    case JUMP_LIMIT:
        jumpLimit = newValue;
        break;

    case USE_BUF_END_MASK:
        useBufferEndMask = static_cast<bool>(newValue);
        break;

    case BUF_END_MASK:
        bufferEndMaskMs = static_cast<int>(newValue);
        updateSampleRateDependentValues();
        break;
    }
}

bool CrossingDetector::enable()
{
    updateSampleRateDependentValues();
    restartAdaptiveThreshold();
    return isEnabled;
}

bool CrossingDetector::disable()
{
    // set this to pastSpan so that we don't trigger on old data when we start again.
    sampToReenable = pastSpan + futureSpan + 1;    
    // cancel any pending turning-off
    turnoffEvent = nullptr;
    return true;
}

// ----- private functions ------

void CrossingDetector::handleEvent(const EventChannel* eventInfo, const MidiMessage& event, int samplePosition)
{
    jassert(indicatorChan > -1);
    const EventChannel* adaptChanInfo = getEventChannel(indicatorChan);
    jassert(isValidIndicatorChan(adaptChanInfo));
    if (eventInfo == adaptChanInfo && thresholdType == ADAPTIVE && !adaptThreshPaused)
    {
        // get error of received event
        BinaryEventPtr binaryEvent = BinaryEvent::deserializeFromMessage(event, eventInfo);
        if (binaryEvent == nullptr)
        {
            return;
        }
        float eventValue = floatFromBinaryEvent(binaryEvent);
        float eventErr = errorFromTarget(eventValue);

        // update state
        currLRDivisor += decayRate;
        double currDecayingLR = currLearningRate - currMinLearningRate;
        currLearningRate = currDecayingLR / currLRDivisor + currMinLearningRate;

        // update threshold
        constantThresh -= currLearningRate * eventErr;
        if (useAdaptThreshRange)
        {
            constantThresh = toThresholdInRange(constantThresh);
        }
        thresholdVal = constantThresh;
    }
}

void CrossingDetector::restartAdaptiveThreshold()
{
    currLRDivisor = 1.0;
    currLearningRate = startLearningRate;
    currMinLearningRate = minLearningRate;
}

float CrossingDetector::errorFromTarget(float x) const
{
    if (useIndicatorRange)
    {
        float rangeSize = indicatorRange[1] - indicatorRange[0];
        jassert(rangeSize >= 0);
        float linearErr = x - indicatorTarget;
        if (std::abs(linearErr) < rangeSize / 2)
        {
            return linearErr;
        }
        else
        {
            return linearErr > 0 ? linearErr - rangeSize : linearErr + rangeSize;
        }
    }
    else
    {
        return x - indicatorTarget;
    }
}

float CrossingDetector::toEquivalentInRange(float x, const float* range)
{
    if (!range)
    {
        jassertfalse;
        return x;
    }
    float top = range[1], bottom = range[0];
    if (x <= top && x >= bottom)
    {
        return x;
    }
    float rangeSize = top - bottom;
    jassert(rangeSize >= 0);
    if (rangeSize == 0)
    {
        return bottom;
    }

    float rem = fmod(x - bottom, rangeSize);
    return rem > 0 ? bottom + rem : bottom + rem + rangeSize;
}

float CrossingDetector::toIndicatorInRange(float x) const
{
    return toEquivalentInRange(x, indicatorRange);
}

float CrossingDetector::toThresholdInRange(float x) const
{
    return toEquivalentInRange(x, adaptThreshRange);
}

float CrossingDetector::floatFromBinaryEvent(BinaryEventPtr& eventPtr)
{
    jassert(eventPtr != nullptr);
    const void *ptr = eventPtr->getBinaryDataPointer();
    switch (eventPtr->getBinaryType())
    {
    case EventChannel::INT8_ARRAY:
        return static_cast<float>(*(static_cast<const juce::int8*>(ptr)));
    case EventChannel::UINT8_ARRAY:
        return static_cast<float>(*(static_cast<const juce::uint8*>(ptr)));
    case EventChannel::INT16_ARRAY:
        return static_cast<float>(*(static_cast<const juce::int16*>(ptr)));
    case EventChannel::UINT16_ARRAY:
        return static_cast<float>(*(static_cast<const juce::uint16*>(ptr)));
    case EventChannel::INT32_ARRAY:
        return static_cast<float>(*(static_cast<const juce::int32*>(ptr)));
    case EventChannel::UINT32_ARRAY:
        return static_cast<float>(*(static_cast<const juce::uint32*>(ptr)));
    case EventChannel::INT64_ARRAY:
        return static_cast<float>(*(static_cast<const juce::int64*>(ptr)));
    case EventChannel::UINT64_ARRAY:
        return static_cast<float>(*(static_cast<const juce::uint64*>(ptr)));
    case EventChannel::FLOAT_ARRAY:
        return *(static_cast<const float*>(ptr));
    case EventChannel::DOUBLE_ARRAY:
        return static_cast<float>(*(static_cast<const double*>(ptr)));
    default:
        jassertfalse;
        return 0;
    }
}

bool CrossingDetector::isValidIndicatorChan(const EventChannel* eventInfo)
{
    EventChannel::EventChannelTypes type = eventInfo->getChannelType();
    bool isBinary = type >= EventChannel::BINARY_BASE_VALUE && type < EventChannel::INVALID;
    bool isNonempty = eventInfo->getLength() > 0;
    return isBinary && isNonempty;
}

float CrossingDetector::nextRandomThresh()
{
    float range = randomThreshRange[1] - randomThreshRange[0];
    return randomThreshRange[0] + range * rng.nextFloat();
}

juce::uint32 CrossingDetector::getSubProcFullID(int chanNum) const
{
    if (chanNum < 0) { return 0; }
    const DataChannel* chan = getDataChannel(chanNum);
    jassert(chan != nullptr);
    uint16 sourceNodeID = chan->getSourceNodeID();
    uint16 subProcessorIdx = chan->getSubProcessorIdx();
    return getProcessorFullId(sourceNodeID, subProcessorIdx);
}

bool CrossingDetector::isCompatibleWithInput(int chanNum) const
{
    if (chanNum == inputChannel || getDataChannel(chanNum) == nullptr)
    {
        return false;
    }

    return getSubProcFullID(chanNum) == validSubProcFullID;
}

String CrossingDetector::toChannelThreshString(int chanNum)
{
    return "<chan " + String(chanNum + 1) + ">";
}

bool CrossingDetector::shouldTrigger(bool direction, float preVal, float postVal,
    float preThresh, float postThresh)
{
    jassert(pastSamplesAbove >= 0 && futureSamplesAbove >= 0);

    // check jumpLimit
    if (useJumpLimit && abs(postVal - preVal) >= jumpLimit)
    {
        return false;
    }

    // number of samples required before and after crossing threshold
    int pastSamplesNeeded = pastSpan ? static_cast<int>(ceil(pastSpan * pastStrict)) : 0;
    int futureSamplesNeeded = futureSpan ? static_cast<int>(ceil(futureSpan * futureStrict)) : 0;
    
    // four conditions for the event
    bool preSat = direction != (preVal > preThresh);
    bool postSat = direction == (postVal > postThresh);
    bool pastSat = (direction ? pastSpan - pastSamplesAbove : pastSamplesAbove) >= pastSamplesNeeded;
    bool futureSat = (direction ? futureSamplesAbove : futureSpan - futureSamplesAbove) >= futureSamplesNeeded;
    
    return preSat && postSat && pastSat && futureSat;
}

void CrossingDetector::triggerEvent(juce::int64 bufferTs, int crossingOffset,
    int bufferLength, float threshold, float crossingLevel)
{
    // Construct metadata array
    // The order has to match the order the descriptors are stored in createEventChannels.
    MetaDataValueArray mdArray;

    int mdInd = 0;
    MetaDataValue* crossingPointVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
    crossingPointVal->setValue(bufferTs + crossingOffset);
    mdArray.add(crossingPointVal);

    MetaDataValue* crossingLevelVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
    crossingLevelVal->setValue(crossingLevel);
    mdArray.add(crossingLevelVal);

    MetaDataValue* threshVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
    threshVal->setValue(threshold);
    mdArray.add(threshVal);

    MetaDataValue* directionVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
    directionVal->setValue(static_cast<juce::uint8>(crossingLevel > threshold));
    mdArray.add(directionVal);

    MetaDataValue* learningRateVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
    learningRateVal->setValue(thresholdType == ADAPTIVE ? currLearningRate : 0);
    mdArray.add(learningRateVal);

    // Create events
    int currEventChan = eventChannel;
    juce::uint8 ttlDataOn = 1 << currEventChan;
    int sampleNumOn = std::max(crossingOffset, 0);
    juce::int64 eventTsOn = bufferTs + sampleNumOn;
    TTLEventPtr eventOn = TTLEvent::createTTLEvent(eventChannelPtr, eventTsOn,
        &ttlDataOn, sizeof(juce::uint8), mdArray, currEventChan);
    addEvent(eventChannelPtr, eventOn, sampleNumOn);

    juce::uint8 ttlDataOff = 0;
    int sampleNumOff = sampleNumOn + eventDurationSamp;
    juce::int64 eventTsOff = bufferTs + sampleNumOff;
    TTLEventPtr eventOff = TTLEvent::createTTLEvent(eventChannelPtr, eventTsOff,
        &ttlDataOff, sizeof(juce::uint8), mdArray, currEventChan);

    // Add or schedule turning-off event
    // We don't care whether there are other turning-offs scheduled to occur either in
    // this buffer or later. The abilities to change event duration during acquisition and for
    // events to be longer than the timeout period create a lot of possibilities and edge cases,
    // but overwriting turnoffEvent unconditionally guarantees that this and all previously
    // turned-on events will be turned off by this "turning-off" if they're not already off.
    if (sampleNumOff <= bufferLength)
    {
        // add event now
        addEvent(eventChannelPtr, eventOff, sampleNumOff);
    }
    else
    {
        // save for later
        turnoffEvent = eventOff;
    }
}

void CrossingDetector::updateSampleRateDependentValues()
{
    const DataChannel* inChan = getDataChannel(inputChannel);
    if (inChan == nullptr) { return; }
    float sampleRate = inChan->getSampleRate();

    eventDurationSamp = int(std::ceil(eventDuration * sampleRate / 1000.0f));
    timeoutSamp = int(std::floor(timeout * sampleRate / 1000.0f));
    bufferEndMaskSamp = int(std::ceil(bufferEndMaskMs * sampleRate / 1000.0f));
}