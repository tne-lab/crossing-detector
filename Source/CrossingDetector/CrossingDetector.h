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

#ifndef CROSSING_DETECTOR_H_INCLUDED
#define CROSSING_DETECTOR_H_INCLUDED

#ifdef _WIN32
#include <Windows.h>
#endif

#include <ProcessorHeaders.h>
#include "CircularArray.h"

/*
 * The crossing detector plugin is designed to read in one continuous channel c, and generate events on one events channel
 * when c crosses a certain value. There are various parameters to tweak this basic functionality, including:
 *  - whether to listen for crosses with a positive or negative slope, or either
 *  - how strictly to filter transient level changes, by adjusting the required number and percent of past and future samples to be above/below the threshold
 *  - the duration of the generated event
 *  - the minimum time to wait between events ("timeout")
 *  - whether to use a constant threshold, draw one randomly from a range for each event, or read thresholds from an input channel
 *
 * All ontinuous signals pass through unchanged, so multiple CrossingDetectors can be
 * chained together in order to operate on more than one channel.
 *
 * @see GenericProcessor
 */

class CrossingDetector : public GenericProcessor
{
    friend class CrossingDetectorEditor;

public:
    CrossingDetector();
    ~CrossingDetector();

    bool hasEditor() const { return true; }
    AudioProcessorEditor* createEditor() override;

    void createEventChannels() override;

    void process(AudioSampleBuffer& continuousBuffer) override;

    void setParameter(int parameterIndex, float newValue) override;

    bool enable() override;
    bool disable() override;

private:
    enum ThresholdType { CONSTANT, RANDOM, CHANNEL };

    enum Parameter
    {
        THRESH_TYPE,
        MIN_RAND_THRESH,
        MAX_RAND_THRESH,
        CONST_THRESH,
        THRESH_CHAN,
        INPUT_CHAN,
        EVENT_CHAN,
        POS_ON,
        NEG_ON,
        EVENT_DUR,
        TIMEOUT,
        PAST_SPAN,
        PAST_STRICT,
        FUTURE_SPAN,
        FUTURE_STRICT,
        USE_JUMP_LIMIT,
        JUMP_LIMIT,
    };

    // -----utility funcs--------

    /* Whether there should be a trigger in the given direction (true = rising, float = falling),
     * given the current pastCounter and futureCounter and the passed values and thresholds
     * surrounding the point where a crossing may be.
     */
    bool shouldTrigger(bool direction, float preVal, float postVal, float preThresh, float postThresh);

    // Select a new random threshold using minThresh, maxThresh, and rng.
    float nextThresh();
 
    /* Add "turning-on" and "turning-off" event for a crossing.
     *  - bufferTs:       Timestamp of start of current buffer
     *  - crossingOffset: Difference betweeen time of actual crossing and bufferTs
     *  - bufferLength:   Number of samples in current buffer
     *  - threshold:      Threshold at the time of the crossing
     *  - crossingLevel:  Level of signal at the first sample after the crossing
     */
    void triggerEvent(juce::int64 bufferTs, int crossingOffset, int bufferLength,
        float threshold, float crossingLevel);

    // Retrieves the full source subprocessor ID of the given channel.
    juce::uint32 getSubProcFullID(int chanNum) const;

    /* Returns true if the given chanNum corresponds to an input
    * and that channel has the same source subprocessor as the
    * selected inputChannel, but is not equal to the inputChannel.
    */
    bool isCompatibleWithInput(int chanNum) const;

    // Returns a string to display in the threshold box when using a threshold channel
    static String toChannelThreshString(int chanNum);

    // ------parameters------------

    ThresholdType thresholdType;

    // if using constant threshold:
    float constantThresh;

    // if using random thresholds:
    float minRandomThresh;
    float maxRandomThresh;
    float currRandomThresh;

    // if using channel threshold:
    int thresholdChannel;

    int inputChannel;
    int eventChannel;
    bool posOn;
    bool negOn;

    int eventDuration; // in milliseconds
    int eventDurationSamp;
    int timeout; // milliseconds after an event onset when no more events are allowed.
    int timeoutSamp;

    /* Number of *additional* past and future samples to look at at each timepoint (attention span)
     * If futureSpan samples are not available to look ahead from a timepoint, the test is delayed until enough samples are available.
     * If it succeeds, the event occurs on the first sample in the buffer when the test occurs, but the "crossing point"
     * metadata field will contain the timepoint of the actual crossing.
     */
    int pastSpan;
    int futureSpan;

    // fraction of spans required to be above / below threshold
    float pastStrict;
    float futureStrict;

    // maximum absolute difference between x[k] and x[k-1] to trigger an event on x[k]
    bool useJumpLimit;
    float jumpLimit;

    // ------internals-----------

    // the next time at which the detector should be reenabled after a timeout period, measured in
    // samples past the start of the current processing buffer. Less than -numNext if there is no scheduled reenable (i.e. the detector is enabled).
    int sampToReenable;

     // counters for delay keeping track of voting samples
    int pastCounter;
    int futureCounter;

    // arrays to implement past/future voting
    CircularArray<float> inputHistory;
    CircularArray<float> thresholdHistory;

    EventChannel* eventChannelPtr;
    MetaDataDescriptorArray eventMetaDataDescriptors;
    TTLEventPtr turnoffEvent; // holds a turnoff event that must be added in a later buffer

    Value thresholdVal; // underlying value of the threshold label

    Random rng; // for random thresholds

    // full subprocessor ID of input channel (or 0 if none selected)
    juce::uint32 validSubProcFullID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossingDetector);
};

#endif // CROSSING_DETECTOR_H_INCLUDED
