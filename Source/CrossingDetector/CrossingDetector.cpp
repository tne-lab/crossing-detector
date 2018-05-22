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

CrossingDetector::CrossingDetector()
    : GenericProcessor  ("Crossing Detector")
    , thresholdType     (CONSTANT)
    , constantThresh    (0.0f)
    , minRandomThresh   (-180)
    , maxRandomThresh   (180)
    , thresholdChannel  (-1)
    , inputChannel      (0)
    , validSubProcFullID(0)
    , eventChannel      (0)
    , posOn             (true)
    , negOn             (false)
    , eventDuration     (5)
    , timeout           (1000)
    , pastStrict        (1.0f)
    , pastSpan          (0)
    , futureStrict      (1.0f)
    , futureSpan        (0)
    , useJumpLimit      (false)
    , jumpLimit         (5.0f)
    , sampToReenable    (pastSpan + futureSpan + 1)
    , pastCounter       (0)
    , futureCounter     (0)
    , inputHistory      (pastSpan + futureSpan + 2)
    , thresholdHistory  (pastSpan + futureSpan + 2)
    , turnoffEvent      (nullptr)
{
    setProcessorType(PROCESSOR_TYPE_FILTER);
    thresholdVal = constantThresh;
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
    float sampleRate = in ? in->getSampleRate() : CoreServices::getGlobalSampleRate();
    EventChannel* chan = new EventChannel(EventChannel::TTL, 8, 1, sampleRate, this);
    chan->setName("Crossing detector output");
    chan->setDescription("Triggers whenever the input signal crosses a voltage threshold.");
    chan->setIdentifier("crossing.event");

    // metadata storing source data channel
    if (in)
    {
        MetaDataDescriptor sourceChanDesc(MetaDataDescriptor::UINT16, 3, "Source Channel",
            "Index at its source, Source processor ID and Sub Processor index of the channel that triggers this event", "source.channel.identifier.full");
        MetaDataValue sourceChanVal(sourceChanDesc);
        uint16 sourceInfo[3];
        sourceInfo[0] = in->getSourceIndex();
        sourceInfo[1] = in->getSourceNodeID();
        sourceInfo[2] = in->getSubProcessorIdx();
        sourceChanVal.setValue(static_cast<const uint16*>(sourceInfo));
        chan->addMetaData(sourceChanDesc, sourceChanVal);
    }

    // event-related metadata!
    eventMetaDataDescriptors.clearQuick();

    MetaDataDescriptor* crossingPointDesc = new MetaDataDescriptor(MetaDataDescriptor::INT64, 1, "Crossing Point",
        "Time when threshold was crossed", "crossing.point");
    chan->addEventMetaData(crossingPointDesc);
    eventMetaDataDescriptors.add(crossingPointDesc);

    MetaDataDescriptor* crossingLevelDesc = new MetaDataDescriptor(MetaDataDescriptor::FLOAT, 1, "Crossing level",
        "Voltage level at first sample after crossing", "crossing.level");
    chan->addEventMetaData(crossingLevelDesc);
    eventMetaDataDescriptors.add(crossingLevelDesc);

    MetaDataDescriptor* threshDesc = new MetaDataDescriptor(MetaDataDescriptor::FLOAT, 1, "Threshold",
        "Monitored voltage threshold", "crossing.threshold");
    chan->addEventMetaData(threshDesc);
    eventMetaDataDescriptors.add(threshDesc);

    MetaDataDescriptor* directionDesc = new MetaDataDescriptor(MetaDataDescriptor::UINT8, 1, "Direction",
        "Direction of crossing: 1 = rising, 0 = falling", "crossing.direction");
    chan->addEventMetaData(directionDesc);
    eventMetaDataDescriptors.add(directionDesc);

    eventChannelPtr = eventChannelArray.add(chan);
}

void CrossingDetector::process(AudioSampleBuffer& continuousBuffer)
{
    if (inputChannel < 0 || inputChannel >= continuousBuffer.getNumChannels())
    {
        jassertfalse;
        return;
    }

    int nSamples = getNumSamples(inputChannel);
    const float* rp = continuousBuffer.getReadPointer(inputChannel);
    juce::int64 startTs = getTimestamp(inputChannel);
    juce::int64 endTs = startTs + nSamples; // 1 past end

    // turn off event from previous buffer if necessary
    if (turnoffEvent != nullptr && turnoffEvent->getTimestamp() < endTs)
    {
        int turnoffOffset = static_cast<int>(turnoffEvent->getTimestamp() - startTs);
        if (turnoffOffset < 0)
        {
            // shouldn't happen; should be added during a previous buffer
            jassertfalse;
            turnoffEvent = nullptr;
        }
        else
        {
            addEvent(eventChannelPtr, turnoffEvent, turnoffOffset);
            turnoffEvent = nullptr;
        }
    }

    // store threshold for each sample of current buffer
    Array<float> currThresholds;
    currThresholds.resize(nSamples);

    // define lambdas to access history values more easily
    auto inputAt = [=](int index)
    {
        return index < 0 ? inputHistory[index] : rp[index];
    };

    auto thresholdAt = [&, this](int index)
    {
        return index < 0 ? thresholdHistory[index] : currThresholds[index];
    };

    // loop over current buffer and add events for newly detected crossings
    for (int i = 0; i < nSamples; ++i)
    {
        // state to keep constant during each iteration
        bool currPosOn = posOn;
        bool currNegOn = negOn;

        // get and save threshold for this sample
        switch (thresholdType)
        {
        case CONSTANT:
            currThresholds.set(i, constantThresh);
            break;

        case RANDOM:
            currThresholds.set(i, currRandomThresh);
            break;

        case CHANNEL:
            currThresholds.set(i, continuousBuffer.getSample(thresholdChannel, i));
            break;
        }

        // update pastCounter and futureCounter
        if (pastSpan > 0)
        {
            int indLeaving = i - (pastSpan + futureSpan + 2);
            if (inputAt(indLeaving) > thresholdAt(indLeaving))
            {
                pastCounter--;
            }

            int indEntering = i - (futureSpan + 2);
            if (inputAt(indEntering) > thresholdAt(indEntering))
            {
                pastCounter++;
            }
        }

        if (futureSpan > 0)
        {
            int indLeaving = i - futureSpan;
            if (inputAt(indLeaving) > thresholdAt(indLeaving))
            {
                futureCounter--;
            }

            int indEntering = i;
            if (inputAt(indEntering) > thresholdAt(indEntering))
            {
                futureCounter++;
            }
        }

        if (i < sampToReenable)
        {
            // can't trigger an event now
            continue;
        }

        int crossingOffset = i - futureSpan;

        float preVal = inputAt(crossingOffset - 1);
        float preThresh = thresholdAt(crossingOffset - 1);
        float postVal = inputAt(crossingOffset);
        float postThresh = thresholdAt(crossingOffset);

        // check whether to trigger an event
        if (currPosOn && shouldTrigger(true, preVal, postVal, preThresh, postThresh) ||
            currNegOn && shouldTrigger(false, preVal, postVal, preThresh, postThresh))
        {
            // add event
            triggerEvent(startTs, crossingOffset, nSamples, postThresh, postVal);
            
            // update sampToReenable
            sampToReenable = i + 1 + timeoutSamp;

            // if using random thresholds, set a new threshold
            if (thresholdType == RANDOM)
            {
                currRandomThresh = nextThresh();
                thresholdVal = currRandomThresh;
            }
        }
    }

    // update inputHistory and thresholdHistory
    inputHistory.enqueueArray(rp, nSamples);
    float* rpThresh = currThresholds.getRawDataPointer();
    thresholdHistory.enqueueArray(rpThresh, nSamples);

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

        case RANDOM:
            // get new random threshold
            currRandomThresh = nextThresh();
            thresholdVal = currRandomThresh;
            break;

        case CHANNEL:
            jassert(isCompatibleWithInput(thresholdChannel));
            thresholdVal = toChannelThreshString(thresholdChannel);
        }

        break;

    case MIN_RAND_THRESH:
        minRandomThresh = newValue;
        currRandomThresh = nextThresh();
        if (thresholdType == RANDOM)
        {
            thresholdVal = currRandomThresh;
        }
        break;

    case MAX_RAND_THRESH:
        maxRandomThresh = newValue;
        currRandomThresh = nextThresh();
        if (thresholdType == RANDOM)
        {
            thresholdVal = currRandomThresh;
        }
        break;

    case CONST_THRESH:
        constantThresh = newValue;
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
        if (newValue >= 0 && newValue < getNumInputs())
        {
            inputChannel = static_cast<int>(newValue);
            validSubProcFullID = getSubProcFullID(inputChannel);
        }
        else
        {
            validSubProcFullID = 0;
        }
        // make sure available threshold channels take into account new input channel
        static_cast<CrossingDetectorEditor*>(getEditor())->updateChannelThreshBox();
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
        eventDurationSamp = static_cast<int>(ceil(eventDuration / 1000.0f 
            * getDataChannel(inputChannel)->getSampleRate()));
        break;

    case TIMEOUT:
        timeout = static_cast<int>(newValue);
        timeoutSamp = static_cast<int>(floor(timeout / 1000.0f 
            * getDataChannel(inputChannel)->getSampleRate()));
        break;

    case PAST_SPAN:
        pastSpan = static_cast<int>(newValue);
        sampToReenable = pastSpan + futureSpan + 1;

        inputHistory.reset();
        inputHistory.resize(pastSpan + futureSpan + 2);
        thresholdHistory.reset();
        thresholdHistory.resize(pastSpan + futureSpan + 2);

        // counters must reflect current contents of inputHistory and thresholdHistory
        pastCounter = 0;
        futureCounter = 0;
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
        pastCounter = 0;
        futureCounter = 0;
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
    }
}

bool CrossingDetector::enable()
{
    // input channel is fixed once acquisition starts, so convert timeout and eventDuration
    float sampleRate = getDataChannel(inputChannel)->getSampleRate();
    eventDurationSamp = static_cast<int>(ceil(eventDuration * sampleRate / 1000.0f));
    timeoutSamp = static_cast<int>(floor(timeout * sampleRate / 1000.0f));
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

// ----- private methods ------

bool CrossingDetector::shouldTrigger(bool direction, float preVal, float postVal,
    float preThresh, float postThresh)
{
    jassert(pastCounter >= 0 && futureCounter >= 0);

    //check jumpLimit
    if (useJumpLimit && abs(postVal - preVal) >= jumpLimit)
    {
        return false;
    }

    //number of samples required before and after crossing threshold
    int pastSamplesNeeded = pastSpan ? static_cast<int>(ceil(pastSpan * pastStrict)) : 0;
    int futureSamplesNeeded = futureSpan ? static_cast<int>(ceil(futureSpan * futureStrict)) : 0;
    // if enough values cross threshold
    if (direction)
    {
        int pastZero = pastSpan - pastCounter;
        if (pastZero >= pastSamplesNeeded && futureCounter >= futureSamplesNeeded &&
            preVal <= preThresh && postVal > postThresh)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        int futureZero = futureSpan - futureCounter;
        if (pastCounter >= pastSamplesNeeded && futureZero >= futureSamplesNeeded &&
            preVal > preThresh && postVal <= postThresh)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

float CrossingDetector::nextThresh()
{
    float range = maxRandomThresh - minRandomThresh;
    return minRandomThresh + range * rng.nextFloat();
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

juce::uint32 CrossingDetector::getSubProcFullID(int chanNum) const
{
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
