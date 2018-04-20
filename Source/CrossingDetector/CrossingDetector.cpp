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
    : GenericProcessor("Crossing Detector")
    , threshold(0.0f)
    , useRandomThresh(false)
    , minThresh(-180)
    , maxThresh(180)
    , thresholdVal(0.0)
    , posOn(true)
    , negOn(false)
    , inputChan(0)
    , eventChan(0)
    , eventDuration(5)
    , timeout(1000)
    , pastStrict(1.0f)
    , pastSpan(0)
    , futureStrict(1.0f)
    , futureSpan(0)
    , useJumpLimit(false)
    , jumpLimit(5.0f)
    , sampsToShutoff(-1)
    , sampsToReenable(pastSpan + futureSpan + 1)
    , shutoffChan(-1)
    , pastCounter(0)
    , futureCounter(0)

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
    
    MetaDataDescriptor* eventLevelDesc = new MetaDataDescriptor(MetaDataDescriptor::FLOAT, 1, "Event level",
                                                                "Actual voltage level at sample where event occurred", "crossing.eventLevel");
    chan->addEventMetaData(eventLevelDesc);
    eventMetaDataDescriptors.add(eventLevelDesc);
    
    MetaDataDescriptor* threshDesc = new MetaDataDescriptor(MetaDataDescriptor::FLOAT, 1, "Threshold",
                                                            "Monitored voltage threshold", "crossing.threshold");
    chan->addEventMetaData(threshDesc);
    eventMetaDataDescriptors.add(threshDesc);
    
    MetaDataDescriptor* posOnDesc = new MetaDataDescriptor(MetaDataDescriptor::UINT8, 1, "Ascending on",
                                                           "Equals 1 if an event is triggered for ascending crossings", "crossing.positive");
    chan->addEventMetaData(posOnDesc);
    eventMetaDataDescriptors.add(posOnDesc);
    
    MetaDataDescriptor* negOnDesc = new MetaDataDescriptor(MetaDataDescriptor::UINT8, 1, "Descending on",
                                                           "Equals 1 if an event is triggered for descending crossings", "crossing.negative");
    chan->addEventMetaData(negOnDesc);
    eventMetaDataDescriptors.add(negOnDesc);
    
    MetaDataDescriptor* crossingPoint = new MetaDataDescriptor(MetaDataDescriptor::INT64, 1, "Crossing Point",
                                                             "Time when threshold was crossed", "crossing.point");
    chan->addEventMetaData(crossingPoint);
    eventMetaDataDescriptors.add(crossingPoint);    
    
    eventChannelPtr = eventChannelArray.add(chan);
}

void CrossingDetector::process(AudioSampleBuffer& continuousBuffer)
{
    // state to keep constant during each call
    int currChan = inputChan;
    int currPastSpan = pastSpan;
    int currFutureSpan = futureSpan;
    
    if (currChan < 0 || currChan >= continuousBuffer.getNumChannels()) // (shouldn't really happen)
        return;
    
    int nSamples = getNumSamples(currChan);
    const float* rp = continuousBuffer.getReadPointer(currChan);
    
    // loop has two functions: detect crossings and turn on events for the end of the previous buffer and most of the current buffer,
    // or if an event is currently on, turn it off if it has been on for long enough.
    for (int i = 0; i <= nSamples; i++)
    {
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
        if(pastBinary[0] == 1)
        {
            pastCounter--;
        }
        for (int j = 0; j < pastSpan; j++)
        {
            pastBinary.set(j, pastBinary[j + 1]);
        }
        pastBinary.set(pastSpan, futureBinary[0]);
        if(pastBinary[pastSpan-1] == 1)
        {
            pastCounter++;
        }
        if(futureBinary[1] == 1)
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
            futureCounter++;
        }
        else 
        {
            futureBinary.set(futureSpan, 0);
        }
        //move back jumpSize values back to make space for new value
        for (int j = 0; j < pastSpan + futureSpan + 1; j++)
        {
            jumpSize.set(j, jumpSize[j + 1]);
        }
        //add new value to jumpSize array
        jumpSize.set(pastSpan + futureSpan + 1, rp[i]);
        

        // if enabled, check whether to trigger an event
        bool turnOn = (i >= sampsToReenable && i < nSamples && shouldTrigger(rp, nSamples, i, currThresh, currPosOn, currNegOn, currPastSpan, currFutureSpan));
        
        // if not triggering, check whether event should be shut off (operates on [0, nSamples) )
        bool turnOff = (!turnOn && sampsToShutoff >= 0 && i >= sampsToShutoff);
        
        if (turnOn || turnOff)
        {
            // actual sample when event fires (start of current buffer if turning on and the crossing was before this buffer.)
            int eventTime = turnOn ? std::max(i - currFutureSpan, 0) : sampsToShutoff;
            int64 timestamp = getTimestamp(currChan) + eventTime;
            int64 crossingTimestamp = getTimestamp(currChan) + (i - currFutureSpan);

            // construct the event's metadata array
            // The order of metadata has to match the order they are stored in createEventChannels.
            MetaDataValueArray mdArray;
            
            int mdInd = 0;
            MetaDataValue* eventLevelVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
            eventLevelVal->setValue(rp[eventTime]);
            mdArray.add(eventLevelVal);
            
            MetaDataValue* threshVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
            threshVal->setValue(currThresh);
            mdArray.add(threshVal);
            
            MetaDataValue* posOnVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
            posOnVal->setValue(static_cast<uint8>(posOn));
            mdArray.add(posOnVal);
            
            MetaDataValue* negOnVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
            negOnVal->setValue(static_cast<uint8>(negOn));
            mdArray.add(negOnVal);
            
            MetaDataValue* crossingPointVal = new MetaDataValue(*eventMetaDataDescriptors[mdInd++]);
            crossingPointVal->setValue(crossingTimestamp);
            mdArray.add(crossingPointVal);

            if (turnOn)
            {
                // add event
                uint8 ttlData = 1 << eventChan;
                TTLEventPtr event = TTLEvent::createTTLEvent(eventChannelPtr, timestamp, &ttlData,
                                                             sizeof(uint8), mdArray, eventChan);
                addEvent(eventChannelPtr, event, eventTime);
                
                // if using random thresholds, set a new threshold
                if (useRandomThresh)
                {
                    currRandomThresh = nextThresh();
                    thresholdVal = currRandomThresh;
                }
                
                // schedule event turning off and timeout period ending
                float sampleRate = getDataChannel(currChan)->getSampleRate();
                int eventDurationSamps = static_cast<int>(ceil(eventDuration * sampleRate / 1000.0f));
                int timeoutSamps = static_cast<int>(floor(timeout * sampleRate / 1000.0f));
                sampsToShutoff = eventTime + eventDurationSamps;
                sampsToReenable = eventTime + timeoutSamps;
            }
            else
            {
                // add (turning-off) event
                uint8 ttlData = 0;
                int realEventChan = (shutoffChan != -1 ? shutoffChan : eventChan);
                TTLEventPtr event = TTLEvent::createTTLEvent(eventChannelPtr, timestamp,
                                                             &ttlData, sizeof(uint8), mdArray, realEventChan);
                addEvent(eventChannelPtr, event, eventTime);
                
                // reset shutoffChan (now eventChan has been changed)
                shutoffChan = -1;
            }
        }
    }
    
    if (sampsToShutoff < nSamples)
        // no scheduled shutoff, so keep it at -1
        sampsToShutoff = -1;
    else
        // shift so it is relative to the next buffer
        sampsToShutoff -= nSamples;
    
    if (sampsToReenable < nSamples)
        // already reenabled
        sampsToReenable = -1;
    else
        // shift so it is relative to the next buffer
        sampsToReenable -= nSamples;
    
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
            // if we're in the middle of an event, keep track of the old channel until it's done.
            if (sampsToShutoff > -1)
                shutoffChan = eventChan;
            eventChan = static_cast<int>(newValue);
            break;
            
        case pEventDur:
            eventDuration = static_cast<int>(newValue);
            break;
            
        case pTimeout:
            timeout = static_cast<int>(newValue);
            break;
            
        case pPastSpan:
            pastSpan = static_cast<int>(newValue);
            sampsToReenable = pastSpan + futureSpan + 1;
            pastBinary.resize(pastSpan + 1); 
            pastBinary.clearQuick( );
            pastBinary.insertMultiple(0, 0, pastSpan + 1);
            break;
            
        case pPastStrict:
            pastStrict = newValue;
            break;
            
        case pFutureSpan:
            futureSpan = static_cast<int>(newValue);
            sampsToReenable = pastSpan + futureSpan + 1;
            futureBinary.resize(futureSpan + 1); 
            futureBinary.clearQuick( );
            futureBinary.insertMultiple(0, 0, futureSpan + 1);
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

bool CrossingDetector::disable()
{
    // set this to pastSpan so that we don't trigger on old data when we start again.
    sampsToReenable = pastSpan + futureSpan + 1;
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
    if(posOn)
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
