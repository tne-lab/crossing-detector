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
    , threshold         (0.0f)
    , useRandomThresh   (false)
    , minThresh         (-180)
    , maxThresh         (180)
    , thresholdVal      (0.0)
    , posOn             (true)
    , negOn             (false)
    , inputChan         (0)
    , eventChan         (0)
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
    , turnoffEvent      (nullptr)

    //binary arrays to keep track of threshold crossing, jumpSize array to compare to jumpLimit
{
    pastBinary.resize(pastSpan + 1);
    pastBinary.clearQuick( );
    pastBinary.insertMultiple(0, 0, pastSpan + 1);
    futureBinary.resize(futureSpan + 1);
    futureBinary.clearQuick( );
    futureBinary.insertMultiple(0, 0, futureSpan + 1);
    setProcessorType(PROCESSOR_TYPE_FILTER);
    jumpSize.resize(pastSpan + futureSpan + 2);
    jumpSize.clearQuick();
    jumpSize.insertMultiple(0, 0, pastSpan + futureSpan + 2);
    
    thresholdHistory.resize(pastSpan + futureSpan + 2);
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
    const DataChannel* in = getDataChannel(inputChan);
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
    if (inputChan < 0 || inputChan >= continuousBuffer.getNumChannels())
    {
        jassertfalse;
        return;
    }

    int nSamples = getNumSamples(inputChan);
    const float* rp = continuousBuffer.getReadPointer(inputChan);
    juce::int64 startTs = getTimestamp(inputChan);
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

    // loop over current buffer and add events for newly detected crossings
    for (int i = 0; i < nSamples; ++i)
    {
        // state to keep constant during each iteration
        float currThresh;
        if (useRandomThresh)
        {
            currThresh = currRandomThresh;
        }
        else
        {
            currThresh = threshold;
        }
        bool currPosOn = posOn;
        bool currNegOn = negOn;

        //in binary arrays move previous values back and make space for new value
        //include counter for above threshold
        if(pastSpan >= 1 && pastBinary[0] == 1)
        {
            pastCounter--;
        }
        for (int j = 0; j < pastSpan; j++)
        {
            pastBinary.set(j, pastBinary[j + 1]);
        }
        pastBinary.set(pastSpan, futureBinary[0]);
        if(pastSpan >= 1 && pastBinary[pastSpan-1] == 1)
        {
            pastCounter++;
        }
        if(futureSpan >= 1 && futureBinary[1] == 1)
        {
            futureCounter--;
        }
        for (int j = 0; j < futureSpan; j++)
        {
            futureBinary.set(j, futureBinary[j + 1]);
        }
        //add new value to end of binary array
        if(rp[i] - currThresh > 0)
        {
            futureBinary.set(futureSpan, 1);
            if (futureSpan >= 1)
                futureCounter++;
        }
        else
        {
            futureBinary.set(futureSpan, 0);
        }
        //move back jumpSize and thresholdHistory values back to make space for new values
        for (int j = 0; j < pastSpan + futureSpan + 1; j++)
        {
            jumpSize.set(j, jumpSize[j + 1]);
            thresholdHistory.set(j, thresholdHistory[j + 1]);
        }
        //add new value to jumpSize array
        jumpSize.set(pastSpan + futureSpan + 1, rp[i]);
        thresholdHistory.set(pastSpan + futureSpan + 1, currThresh);

        if (i < sampToReenable)
            // can't trigger an event now
            continue;

        // check whether to trigger an event
        if (shouldTrigger(rp, nSamples, i, currThresh, currPosOn, currNegOn, pastSpan, futureSpan))
        {
            // add event
            int crossingOffset = i - futureSpan;
            float crossingThreshold = thresholdHistory[pastSpan + 1];
            float crossingLevel = jumpSize[pastSpan + 1];
            triggerEvent(startTs, crossingOffset, nSamples, crossingThreshold, crossingLevel);
            
            // update sampToReenable
            sampToReenable = i + 1 + timeoutSamp;

            // if using random thresholds, set a new threshold
            if (useRandomThresh)
            {
                currRandomThresh = nextThresh();
                thresholdVal = currRandomThresh;
            }
        }
    }

    // shift sampToReenable so it is relative to the next buffer
    sampToReenable = std::max(0, sampToReenable - nSamples);
}

// all new values should be validated before this function is called!
void CrossingDetector::setParameter(int parameterIndex, float newValue)
{
    switch (parameterIndex)
    {
    case pRandThresh:
        useRandomThresh = static_cast<bool>(newValue);
        // update threshold
        float newThresh;
        if (useRandomThresh)
        {
            newThresh = nextThresh();
            currRandomThresh = newThresh;
        }
        else
        {
            newThresh = threshold;
        }
        thresholdVal = newThresh;
        break;

    case pMinThresh:
        minThresh = newValue;
        currRandomThresh = nextThresh();
        if (useRandomThresh)
            thresholdVal = currRandomThresh;
        break;

    case pMaxThresh:
        maxThresh = newValue;
        currRandomThresh = nextThresh();
        if (useRandomThresh)
            thresholdVal = currRandomThresh;
        break;

    case pThreshold:
        threshold = newValue;
        break;

    case pPosOn:
        posOn = static_cast<bool>(newValue);
        break;

    case pNegOn:
        negOn = static_cast<bool>(newValue);
        break;

    case pInputChan:
        if (getNumInputs() > newValue)
            inputChan = static_cast<int>(newValue);
        break;

    case pEventChan:
        eventChan = static_cast<int>(newValue);
        break;

    case pEventDur:
        eventDuration = static_cast<int>(newValue);
        if (CoreServices::getAcquisitionStatus())
        {
            float sampleRate = getDataChannel(inputChan)->getSampleRate();
            eventDurationSamp = static_cast<int>(ceil(eventDuration * sampleRate / 1000.0f));
        }
        break;

    case pTimeout:
        timeout = static_cast<int>(newValue);
        if (CoreServices::getAcquisitionStatus())
        {
            float sampleRate = getDataChannel(inputChan)->getSampleRate();
            timeoutSamp = static_cast<int>(floor(timeout * sampleRate / 1000.0f));
        }
        break;

    case pPastSpan:
        pastSpan = static_cast<int>(newValue);
        sampToReenable = pastSpan + futureSpan + 1;
        pastBinary.resize(pastSpan + 1);
        pastBinary.clearQuick( );
        pastBinary.insertMultiple(0, 0, pastSpan + 1);
        pastCounter = 0; // must reflect current contents of pastBinary
        jumpSize.resize(pastSpan + futureSpan + 2);
        thresholdHistory.resize(pastSpan + futureSpan + 2);
        break;

    case pPastStrict:
        pastStrict = newValue;
        break;

    case pFutureSpan:
        futureSpan = static_cast<int>(newValue);
        sampToReenable = pastSpan + futureSpan + 1;
        futureBinary.resize(futureSpan + 1);
        futureBinary.clearQuick( );
        futureBinary.insertMultiple(0, 0, futureSpan + 1);
        futureCounter = 0; // must reflect current contents of futureBinary
        jumpSize.resize(pastSpan + futureSpan + 2);
        thresholdHistory.resize(pastSpan + futureSpan + 2);
        break;

    case pFutureStrict:
        futureStrict = newValue;
        break;

    case pUseJumpLimit:
        useJumpLimit = static_cast<bool>(newValue);
        break;

    case pJumpLimit:
        jumpLimit = newValue;
        break;
    }
}

bool CrossingDetector::enable()
{
    // input channel is fixed once acquisition starts, so convert timeout and eventDuration
    float sampleRate = getDataChannel(inputChan)->getSampleRate();
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

bool CrossingDetector::shouldTrigger(const float* rpCurr, int nSamples, int t0, float currThresh,
    bool currPosOn, bool currNegOn, int currPastSpan, int currFutureSpan)
{
    if (!currPosOn && !currNegOn)
        return false;

    if (currPosOn && currNegOn)
        return shouldTrigger(rpCurr, nSamples, t0, currThresh, true, false, currPastSpan, currFutureSpan) 
        || shouldTrigger(rpCurr, nSamples, t0, currThresh, false, true, currPastSpan, currFutureSpan);

    //check jumpLimit
    if (useJumpLimit && abs(jumpSize[pastSpan] - jumpSize[pastSpan + 1]) >= jumpLimit)
    {
        return false;
    }
    //number of samples required before and after crossing threshold
    int pastSamplesNeeded = pastSpan ? static_cast<int>(ceil(pastSpan * pastStrict)) : 0;
    int futureSamplesNeeded = futureSpan ? static_cast<int>(ceil(futureSpan * futureStrict)) : 0;
    // if enough values cross threshold
    if(currPosOn)
    {
        int pastZero = pastSpan - pastCounter;
        if(pastZero >= pastSamplesNeeded && futureCounter >= futureSamplesNeeded &&
            pastBinary[pastSpan] == 0 && futureBinary[0] == 1)
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
        if(pastCounter >= pastSamplesNeeded && futureZero >= futureSamplesNeeded &&
            pastBinary[pastSpan] == 1 && futureBinary[0] == 0)
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
    float range = maxThresh - minThresh;
    return minThresh + range * rng.nextFloat();
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
    int currEventChan = eventChan;
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
        // add event now
        addEvent(eventChannelPtr, eventOff, sampleNumOff);
    else
        // save for later
        turnoffEvent = eventOff;
}