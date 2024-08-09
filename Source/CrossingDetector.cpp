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
#include <climits>

/** ------------- Crossing Detector Stream Settings --------------- */

CrossingDetectorSettings::CrossingDetectorSettings() :
    inputChannel(0),
    eventChannel(0),
    indicatorChannel(-1),
    thresholdChannel(0),
    sampleRate(0.0f),
    averageNewSampWeight(0.0f),
    eventChannelPtr(nullptr),
    turnoffEvent(nullptr)
{
     // make the event-related metadata descriptors
    eventMetadataDescriptors.add(new MetadataDescriptor(MetadataDescriptor::INT64, 1, "Crossing Point",
        "Time when threshold was crossed", "crossing.point"));
    eventMetadataDescriptors.add(new MetadataDescriptor(MetadataDescriptor::FLOAT, 1, "Crossing level",
        "Voltage level at first sample after crossing", "crossing.level"));
    eventMetadataDescriptors.add(new MetadataDescriptor(MetadataDescriptor::FLOAT, 1, "Threshold",
        "Monitored voltage threshold", "crossing.threshold"));
    eventMetadataDescriptors.add(new MetadataDescriptor(MetadataDescriptor::UINT8, 1, "Direction",
        "Direction of crossing: 1 = rising, 0 = falling", "crossing.direction"));
    eventMetadataDescriptors.add(new MetadataDescriptor(MetadataDescriptor::DOUBLE, 1, "Learning rate",
        "Learning rate of adaptive algorithm", "crossing.learning_rate"));
}

void CrossingDetectorSettings::updateSampleRateDependentValues(
                                int eventDuration,
                                int timeout,
                                int bufferEndMask,
                                float averageDecaySeconds)
{
    eventDurationSamp = int(std::ceil(eventDuration * sampleRate / 1000.0f));
    timeoutSamp = int(std::floor(timeout * sampleRate / 1000.0f));
    bufferEndMaskSamp = int(std::ceil(bufferEndMask * sampleRate / 1000.0f));
    averageNewSampWeight = 1.0 / (averageDecaySeconds * sampleRate);
}

TTLEventPtr CrossingDetectorSettings::createEvent(juce::int64 bufferTs, int crossingOffset,
    int bufferLength, float threshold, float crossingLevel, bool eventState, double learningRate)
{
    // Construct metadata array
    // The order has to match the order the descriptors are stored in createEventChannels.
    MetadataValueArray mdArray;

    int mdInd = 0;
    MetadataValue* crossingPointVal = new MetadataValue(*eventMetadataDescriptors[mdInd++]);
    crossingPointVal->setValue(bufferTs + crossingOffset);
    mdArray.add(crossingPointVal);

    MetadataValue* crossingLevelVal = new MetadataValue(*eventMetadataDescriptors[mdInd++]);
    crossingLevelVal->setValue(crossingLevel);
    mdArray.add(crossingLevelVal);

    MetadataValue* threshVal = new MetadataValue(*eventMetadataDescriptors[mdInd++]);
    threshVal->setValue(threshold);
    mdArray.add(threshVal);

    MetadataValue* directionVal = new MetadataValue(*eventMetadataDescriptors[mdInd++]);
    directionVal->setValue(static_cast<juce::uint8>(crossingLevel > threshold));
    mdArray.add(directionVal);

    MetadataValue* learningRateVal = new MetadataValue(*eventMetadataDescriptors[mdInd++]);
    learningRateVal->setValue(learningRate);
    mdArray.add(learningRateVal);

    // Create event
    if(eventState)
    {
        int sampleNumOn = std::max(crossingOffset, 0);
        juce::int64 eventTsOn = bufferTs + sampleNumOn;
        TTLEventPtr eventOn = TTLEvent::createTTLEvent(eventChannelPtr, eventTsOn,
            eventChannel, true, mdArray);

        return  eventOn;
    }
    else
    {
        int sampleNumOff = std::max(crossingOffset, 0) + eventDurationSamp;
        juce::int64 eventTsOff = bufferTs + sampleNumOff;
        TTLEventPtr eventOff = TTLEvent::createTTLEvent(eventChannelPtr, eventTsOff,
            eventChannel, false, mdArray);

        return eventOff;
    }
}


/** ------------- Crossing Detector Processor --------------- */

CrossingDetector::CrossingDetector()
    : GenericProcessor      ("Crossing Detector")
    , thresholdType         (CONSTANT)
    , toggleThreshold       (false)
    , constantThresh        (0.0f)
    , averageDecaySeconds   (5.0f)
    , averageNeedsInit      (true)
    , runningSquaredAverage (0.0f)
    , selectedStreamId      (0)
    , indicatorTarget(180.0f)
    , useIndicatorRange(true)
    , startLearningRate(0.02)
    , minLearningRate(0.005)
    , decayRate(0.00003)
    , currLRDivisor(1.0)
    , adaptThreshPaused(false)
    , useAdaptThreshRange(true)
    , currLearningRate(startLearningRate)
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
    , jumpLimitElapsed      (0)
    , sampToReenable        (pastSpan + futureSpan + 1)
    , pastSamplesAbove      (0)
    , futureSamplesAbove    (0)
    , inputHistory          (pastSpan + futureSpan + 2)
    , thresholdHistory      (pastSpan + futureSpan + 2)
{
    setProcessorType(Plugin::Processor::FILTER);

    indicatorRange[0] = -180.0f;
    indicatorRange[1] = 180.0f;
    adaptThreshRange[0] = -180.0f;
    adaptThreshRange[1] = 180.0f;
    randomThreshRange[0] = -180.0f;
    randomThreshRange[1] = 180.0f;
    thresholdVal = constantThresh;

    addSelectedChannelsParameter(Parameter::STREAM_SCOPE, "Channel", "The input channel to analyze", 1);

    addIntParameter(Parameter::STREAM_SCOPE, "TTL_OUT", "Event output channel", 1, 1, 16);

    addBooleanParameter(Parameter::GLOBAL_SCOPE, "Rising", 
                        "Trigger events when past samples are below and future samples are above the threshold",
                        posOn);
    
    addBooleanParameter(Parameter::GLOBAL_SCOPE, "Falling", 
                        "Trigger events when past samples are above and future samples are below the threshold",
                        negOn);
    
    addIntParameter(Parameter::GLOBAL_SCOPE, "Timeout_ms", "Minimum length of time between consecutive events",
                    timeout, 0, 100000);

    addIntParameter(Parameter::GLOBAL_SCOPE, "threshold_type", "Type of Threshold to use", thresholdType, 0, 5);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "constant_threshold", "Constant threshold value",
                    constantThresh, -FLT_MAX, FLT_MAX, 0.1f);
    

    addFloatParameter(Parameter::GLOBAL_SCOPE, "avg_decay_seconds", "RMS Average Duration", averageDecaySeconds, 0, FLT_MAX, 0.01f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "min_random_threshold", "Minimum random threshold value",
                    randomThreshRange[0], -10000.0f, 10000.0f, 0.1f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "max_random_threshold", "Maximum random threshold value",
                    randomThreshRange[1], -10000.0f, 10000.0f, 0.1f);

    addIntParameter(Parameter::STREAM_SCOPE, "threshold_chan", "Threshold reference channel", 0, 0, 1000);

    addIntParameter(Parameter::GLOBAL_SCOPE, "past_span", "Number of past samples to look at at each timepoint (attention span)",
                    pastSpan, 0, 100000);

    addIntParameter(Parameter::GLOBAL_SCOPE, "future_span", "Number of future samples to look at at each timepoint (attention span)",
                    futureSpan, 0, 100000);
    
    addFloatParameter(Parameter::GLOBAL_SCOPE, "past_strict", "fraction of past span required to be above / below threshold",
                    pastStrict, 0.0f, 1.0f, 0.01f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "future_strict", "fraction of future span required to be above / below threshold",
                    futureStrict, 0.0f, 1.0f, 0.01f);
    
    addBooleanParameter(Parameter::GLOBAL_SCOPE, "use_jump_limit", 
                        "Enable/Disable phase jump filtering",
                        useJumpLimit);
    
    addFloatParameter(Parameter::GLOBAL_SCOPE, "jump_limit", "Maximum jump size",
                      jumpLimit, 0.0f, FLT_MAX, 0.1f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "jump_limit_sleep", "Sleep after artifact",
                      0.0f, 0.0f, FLT_MAX, 0.1f);

    addBooleanParameter(Parameter::GLOBAL_SCOPE, "use_buffer_end_mask", 
                        "Enable/disable buffer end sample voting",
                        negOn);

    addBooleanParameter(Parameter::GLOBAL_SCOPE, "toggle_threshold", 
                        "Enable/disable threshold",
                        toggleThreshold);

    addIntParameter(Parameter::GLOBAL_SCOPE, "buffer_end_mask", "Ignore crossings ocurring specified ms before the end of a buffer",
                    bufferEndMaskMs, 0, INT_MAX);

    addIntParameter(Parameter::GLOBAL_SCOPE, "event_duration", "Event Duration", eventDuration, 0, INT_MAX);
    
    // Adaptive threshold
    addIntParameter(Parameter::STREAM_SCOPE, "indicator_channel", "Index of monitored event channel", -1, -1, INT_MAX);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "indicator_target", "Target indicator", indicatorTarget, FLT_MIN, FLT_MAX, 0.1f);

    addBooleanParameter(Parameter::GLOBAL_SCOPE, "use_indicator_range", "Whether to use circular range", useIndicatorRange);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "indicator_range_start", "Start of indicator range", indicatorRange[0], FLT_MIN, FLT_MAX, 0.1f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "indicator_range_end", "Start of indicator range", indicatorRange[1], FLT_MIN, FLT_MAX, 0.1f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "start_learning_rate", "Initial learning rate", startLearningRate, FLT_MIN, FLT_MAX, 0.001f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "min_learning_rate", "Minimum learning rate", minLearningRate, FLT_MIN, FLT_MAX, 0.0001f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "decay_rate", "Decay rate", decayRate, FLT_MIN, FLT_MAX, 0.00001f);

    addBooleanParameter(Parameter::GLOBAL_SCOPE, "adapt_threshold_paused", "Whether the adaptive threshold is paused", adaptThreshPaused);

    addBooleanParameter(Parameter::GLOBAL_SCOPE, "use_adapt_threshold_range", "Whether to keep the threshold within the range", useAdaptThreshRange);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "adapt_threshold_range_start", "Start of adaptive threshold range", 
        adaptThreshRange[0], FLT_MIN, FLT_MAX, 0.1f);

    addFloatParameter(Parameter::GLOBAL_SCOPE, "adapt_threshold_range_end", "End of adaptive threshold range", adaptThreshRange[1], FLT_MIN, FLT_MAX, 0.1f);
}

CrossingDetector::~CrossingDetector() {}

AudioProcessorEditor* CrossingDetector::createEditor()
{
    editor = std::make_unique<CrossingDetectorEditor>(this);
    return editor.get();
}

void CrossingDetector::updateSettings()
{
    settings.update(getDataStreams());

    for(auto stream : getDataStreams())
    {
        settings[stream->getStreamId()]->sampleRate =  stream->getSampleRate();
        
        EventChannel* ttlChan;
        EventChannel::Settings ttlChanSettings{
            EventChannel::Type::TTL,
            "Crossing detector output",
            "Triggers whenever the input signal crosses a voltage threshold.",
            "crossing.event",
            getDataStream(stream->getStreamId())
        };

        ttlChan = new EventChannel(ttlChanSettings);

        // event-related metadata!
        for (auto desc : settings[stream->getStreamId()]->eventMetadataDescriptors)
        {
            ttlChan->addEventMetadata(desc);
        }

        eventChannels.add(ttlChan);
        eventChannels.getLast()->addProcessor(processorInfo.get());
        settings[stream->getStreamId()]->eventChannelPtr = eventChannels.getLast();
    }

    // Force trigger parameter value update
    parameterValueChanged(getParameter("Timeout_ms"));
    parameterValueChanged(getParameter("threshold_type"));
    parameterValueChanged(getParameter("constant_threshold"));
    parameterValueChanged(getParameter("min_random_threshold"));
    parameterValueChanged(getParameter("max_random_threshold"));
    parameterValueChanged(getParameter("future_span"));
    parameterValueChanged(getParameter("past_span"));
    parameterValueChanged(getParameter("past_strict"));
    parameterValueChanged(getParameter("future_strict"));
    parameterValueChanged(getParameter("use_jump_limit"));
    parameterValueChanged(getParameter("jump_limit"));
    parameterValueChanged(getParameter("jump_limit_sleep"));
    parameterValueChanged(getParameter("buffer_end_mask"));
    parameterValueChanged(getParameter("event_duration"));
    parameterValueChanged(getParameter("avg_decay_seconds"));

    parameterValueChanged(getParameter("indicator_target"));
    parameterValueChanged(getParameter("use_indicator_range"));
    parameterValueChanged(getParameter("indicator_range_start"));
    parameterValueChanged(getParameter("indicator_range_end"));
    parameterValueChanged(getParameter("start_learning_rate"));
    parameterValueChanged(getParameter("min_learning_rate"));
    parameterValueChanged(getParameter("decay_rate"));
    parameterValueChanged(getParameter("adapt_threshold_paused"));
    parameterValueChanged(getParameter("use_adapt_threshold_range"));
    parameterValueChanged(getParameter("adapt_threshold_range_start"));
    parameterValueChanged(getParameter("adapt_threshold_range_end"));
}

void CrossingDetector::process(AudioSampleBuffer& continuousBuffer)
{
    // loop through the streams
    for (auto stream : getDataStreams())
    {

        if ((*stream)["enable_stream"] && stream->getStreamId() == selectedStreamId)
        {
            CrossingDetectorSettings* settingsModule = settings[stream->getStreamId()];
            auto streamId = stream->getStreamId();
            if (settings[streamId]->indicatorChannel > -1)
            {
                checkForEvents();
            }

            if (settingsModule->inputChannel < 0
                || settingsModule->inputChannel >= continuousBuffer.getNumChannels()
                || !settingsModule->eventChannelPtr)
            {
                jassertfalse;
                return;
            }

            int nSamples = getNumSamplesInBlock(stream->getStreamId());
            int globalChanIndex = stream->getContinuousChannels()[settingsModule->inputChannel]->getGlobalIndex();
            const float* const rp = continuousBuffer.getReadPointer(globalChanIndex);
            juce::int64 startTs = getFirstSampleNumberForBlock(stream->getStreamId());

            // turn off event from previous buffer if necessary
            int turnoffOffset = settingsModule->turnoffEvent ? 
                                jmax(0, (int)(settingsModule->turnoffEvent->getSampleNumber() - startTs)) : -1;
            if (turnoffOffset >= 0 && turnoffOffset < nSamples)
            {
                addEvent(settingsModule->turnoffEvent, turnoffOffset);
                settingsModule->turnoffEvent = nullptr;
            }

            const ThresholdType currThreshType = thresholdType;

            // store threshold for each sample of current buffer
            if (currThresholds.size() < nSamples)
            {
                currThresholds.resize(nSamples);
            }
            float* const pThresh = currThresholds.getRawDataPointer();

            int threshChanIndex = stream->getContinuousChannels()[settingsModule->thresholdChannel]->getGlobalIndex();
            const float* const rpThreshChan = currThreshType == CHANNEL
                ? continuousBuffer.getReadPointer(threshChanIndex)
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

            if (averageNeedsInit)
            {
                averageNeedsInit = false;
                runningSquaredAverage = inputAt(0) * inputAt(0);
            }

            // loop over current buffer and add events for newly detected crossings
            for (int i = 0; i < nSamples; ++i)
            {
                // state to keep constant during each iteration
                bool currPosOn = posOn;
                bool currNegOn = negOn;

                // Update the running average whether or not we're using it.
                // Bog standard first-order exponential smoothing of the squared amplitude.
                runningSquaredAverage *= (1.0 - settingsModule->averageNewSampWeight);
                runningSquaredAverage += settingsModule->averageNewSampWeight * inputAt(i) * inputAt(i);

                // get and save threshold for this sample
                switch (currThreshType)
                {
                case CONSTANT:
                case ADAPTIVE:
                    pThresh[i] = constantThresh;
                    break;
                case AVERAGE:
                    pThresh[i] = constantThresh * sqrt(runningSquaredAverage);
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
                    (useBufferEndMask && nSamples - indCross > settingsModule->bufferEndMaskSamp))
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

                    double eventLearningRate = thresholdType == ADAPTIVE ? currLearningRate : 0;
                    // create and add ON event
                    TTLEventPtr onEvent = settingsModule->
                        createEvent(startTs, indCross, nSamples, postThresh, postVal, true, eventLearningRate);
                    addEvent(onEvent, std::max(indCross, 0));

                    // create OFF event
                    int sampleNumOff = std::max(indCross, 0) + settingsModule->eventDurationSamp;
                    TTLEventPtr offEvent = settingsModule->
                        createEvent(startTs, indCross, nSamples, postThresh, postVal, false, eventLearningRate);
                    
                    // Add or schedule turning-off event
                    // We don't care whether there are other turning-offs scheduled to occur either in
                    // this buffer or later. The abilities to change event duration during acquisition and for
                    // events to be longer than the timeout period create a lot of possibilities and edge cases,
                    // but overwriting turnoffEvent unconditionally guarantees that this and all previously
                    // turned-on events will be turned off by this "turning-off" if they're not already off.
                    if (sampleNumOff <= nSamples)
                    {
                        // add off event now
                        addEvent(offEvent, sampleNumOff);
                    }
                    else
                    {
                        // save for later
                        settingsModule->turnoffEvent = offEvent;
                    }
                    
                    // update sampToReenable
                    sampToReenable = indCross + 1 + settingsModule->timeoutSamp;

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

            // threshold toggle

            int toggleChannelNum  = -1;
            if (toggleThreshold) 
            {
                toggleChannelNum = settingsModule->inputChannel;
            }
            if (toggleChannelNum >= 0)
            {
                float *wpThresh = continuousBuffer.getWritePointer(toggleChannelNum);
                if (wpThresh != nullptr)
                {
                    for (int i = 0; i < nSamples; ++i)
                        wpThresh[i] -= pThresh[i];
                } 
            }
        }
    }
}


void CrossingDetector::handleTTLEvent(TTLEventPtr event)
{
    const EventChannel* eventChanInfo = event->getChannelInfo();
    const EventChannel* indChanInfo = getEventChannel(settings[selectedStreamId]->indicatorChannel);

    if (eventChanInfo == indChanInfo && thresholdType == ADAPTIVE && !adaptThreshPaused)
    {
        int metadataCount = event->getMetadataValueCount();

        if (metadataCount > 0)
        {

            for (int mIndex = 0; mIndex < event->getMetadataValueCount(); mIndex++)
            {
                const MetadataValue* metadataValue = event->getMetadataValue(0);

                if (metadataValue->getDataType() == MetadataDescriptor::DOUBLE)
                {
                    double eventValue;
                    metadataValue->getValue(&eventValue);

                    float eventErr = errorFromTarget(eventValue);

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

                    break;
                }
            }
        }
    }
}

void CrossingDetector::parameterValueChanged(Parameter* param)
{
    LOGD("[Crossing Detector] Parameter value changed: ", param->getName());
    if (param->getName().equalsIgnoreCase("threshold_type"))
    {
        thresholdType = static_cast<ThresholdType>((int)param->getValue());

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
                thresholdVal = toChannelThreshString(settings[selectedStreamId]->thresholdChannel);
                break;

            case AVERAGE:
                thresholdVal = constantThresh;
                break;
        }
    }
    else if (param->getName().equalsIgnoreCase("constant_threshold"))
    {
        constantThresh = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("min_random_threshold"))
    {
        randomThreshRange[0] = (float)param->getValue();
        currRandomThresh = nextRandomThresh();
        if (thresholdType == RANDOM)
        {
            thresholdVal = currRandomThresh;
        }
    }
    else if (param->getName().equalsIgnoreCase("max_random_threshold"))
    {
        randomThreshRange[1] = (float)param->getValue();
        currRandomThresh = nextRandomThresh();
        if (thresholdType == RANDOM)
        {
            thresholdVal = currRandomThresh;
        }
    }
    else if (param->getName().equalsIgnoreCase("threshold_chan"))
    {
        settings[param->getStreamId()]->thresholdChannel = (int)param->getValue();
        if (thresholdType == CHANNEL)
        {
            thresholdVal = toChannelThreshString(settings[param->getStreamId()]->thresholdChannel);
        }
    }
    else if (param->getName().equalsIgnoreCase("Channel"))
    {
        Array<var>* array = param->getValue().getArray();
        
        if (array->size() > 0)
            settings[param->getStreamId()]->inputChannel = int(array->getReference(0));
        else
            settings[param->getStreamId()]->inputChannel = -1;

        if(selectedStreamId != param->getStreamId())
            setSelectedStream(param->getStreamId());

        // make sure available threshold channels take into account new input channel
        static_cast<CrossingDetectorEditor*>(getEditor())->updateVisualizer();

        // // update signal chain, since the event channel metadata has to get updated.
         CoreServices::updateSignalChain(getEditor());
    }
    else if (param->getName().equalsIgnoreCase("TTL_OUT"))
    {
        settings[param->getStreamId()]->eventChannel = (int)param->getValue() - 1;
    }
    else if (param->getName().equalsIgnoreCase("Rising"))
    {
        posOn = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("Falling"))
    {
        negOn = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("event_duration"))
    {
        eventDuration = (int)param->getValue();
        for (auto stream : getDataStreams())
        {
            settings[stream->getStreamId()]->updateSampleRateDependentValues(eventDuration, timeout, bufferEndMaskMs, averageDecaySeconds);
        }
    }
    else if (param->getName().equalsIgnoreCase("Timeout_ms"))
    {
        timeout = (int)param->getValue();
        for (auto stream : getDataStreams())
        {
            settings[stream->getStreamId()]->updateSampleRateDependentValues(eventDuration, timeout, bufferEndMaskMs, averageDecaySeconds);
        }
    }
    else if (param->getName().equalsIgnoreCase("past_span"))
    {
        pastSpan = (int)param->getValue();
        sampToReenable = pastSpan + futureSpan + 1;

        inputHistory.reset();
        inputHistory.resize(pastSpan + futureSpan + 2);
        thresholdHistory.reset();
        thresholdHistory.resize(pastSpan + futureSpan + 2);

        // counters must reflect current contents of inputHistory and thresholdHistory
        pastSamplesAbove = 0;
        futureSamplesAbove = 0;
    }
    else if (param->getName().equalsIgnoreCase("future_span"))
    {
        futureSpan = (int)param->getValue();
        sampToReenable = pastSpan + futureSpan + 1;

        inputHistory.reset();
        inputHistory.resize(pastSpan + futureSpan + 2);
        thresholdHistory.reset();
        thresholdHistory.resize(pastSpan + futureSpan + 2);

        // counters must reflect current contents of inputHistory and thresholdHistory
        pastSamplesAbove = 0;
        futureSamplesAbove = 0;
    }
    else if (param->getName().equalsIgnoreCase("past_strict"))
    {
        pastStrict = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("future_strict"))
    {
        futureStrict = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("use_jump_limit"))
    {
        useJumpLimit = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("jump_limit"))
    {
        jumpLimit = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("jump_limit_sleep"))
    {
        jumpLimitSleep = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("use_buffer_end_mask"))
    {
        useBufferEndMask = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("toggle_threshold"))
    {
        toggleThreshold = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("buffer_end_mask"))
    {
        bufferEndMaskMs = (int)param->getValue();
        for (auto stream : getDataStreams())
        {
            settings[stream->getStreamId()]->updateSampleRateDependentValues(eventDuration, timeout, bufferEndMaskMs, averageDecaySeconds);
        }
    }
    else if (param->getName().equalsIgnoreCase("indicator_channel"))
    {
        auto streamId = param->getStreamId();
        int value = param->getValue();
        auto streamSettings = settings[streamId];

        if (streamId == 0 || streamSettings == nullptr)
        {
            return;
        }

        if (value > -1 && value < getTotalEventChannels())
        {
            settings[streamId]->indicatorChannel = value;
        }
        else
        {
            
            settings[streamId]->indicatorChannel = -1;
        }
    }
    else if (param->getName().equalsIgnoreCase("indicator_target"))
    {
        indicatorTarget = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("use_indicator_range"))
    {
        useIndicatorRange = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("indicator_range_start"))
    {
        indicatorRange[0] = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("indicator_range_end"))
    {
        indicatorRange[1] = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("adapt_threshold_paused"))
    {
        adaptThreshPaused = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("start_learning_rate"))
    {
        startLearningRate = (double)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("min_learning_rate"))
    {
        minLearningRate = (double)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("decay_rate"))
    {
        decayRate = (double)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("use_adapt_threshold_range"))
    {
        useAdaptThreshRange = (bool)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("adapt_threshold_range_start"))
    {
        adaptThreshRange[0] = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("adapt_threshold_range_end"))
    {
        adaptThreshRange[1] = (float)param->getValue();
    }
}


bool CrossingDetector::startAcquisition()
{
    jumpLimitElapsed = jumpLimitSleep * getDataStream(selectedStreamId)->getSampleRate();

    for(auto stream : getDataStreams())
    {
        settings[stream->getStreamId()]->
            updateSampleRateDependentValues(eventDuration, timeout, bufferEndMaskMs, averageDecaySeconds);
    }

    return isEnabled;
}

bool CrossingDetector::stopAcquisition()
{
    // set this to pastSpan so that we don't trigger on old data when we start again.
    sampToReenable = pastSpan + futureSpan + 1;    
    // cancel any pending turning-off per stream
    for(auto stream : getDataStreams())
    {
        settings[stream->getStreamId()]->turnoffEvent = nullptr;
    }
    
    return true;
}

void CrossingDetector::setSelectedStream(juce::uint16 streamId)
{
    selectedStreamId = streamId;
}

// ----- private functions ------


float CrossingDetector::nextRandomThresh()
{
    float range = randomThreshRange[1] - randomThreshRange[0];
    return randomThreshRange[0] + range * rng.nextFloat();
}

bool CrossingDetector::isCompatibleWithInput(int chanNum)
{
    if (settings[selectedStreamId]->inputChannel == chanNum 
        && getDataStream(selectedStreamId)->getContinuousChannels()[chanNum] != nullptr)
    {
        return false;
    }

    return true;
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
        jumpLimitElapsed = 0;
        return false;
    }

    if (jumpLimitElapsed <= (jumpLimitSleep * getDataStream(selectedStreamId)->getSampleRate()))
    {
        jumpLimitElapsed++;
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
