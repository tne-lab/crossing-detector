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
    enum ThresholdType { CONSTANT, RANDOM, CHANNEL, ADAPTIVE };

    enum Parameter
    {
        THRESH_TYPE,
        CONST_THRESH,
        INDICATOR_CHAN,
        INDICATOR_TARGET,
        USE_INDICATOR_RANGE,
        MIN_INDICATOR,
        MAX_INDICATOR,
        START_LEARNING_RATE,
        MIN_LEARNING_RATE,
        DECAY_RATE,
        ADAPT_PAUSED,
        USE_THRESH_RANGE,
        MIN_ADAPTED_THRESH,
        MAX_ADAPTED_THRESH,
        MIN_RAND_THRESH,
        MAX_RAND_THRESH,
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

    // ---------------------------- PRIVATE FUNCTIONS ----------------------

    /*********** adaptive threshold *************/

    /* Use events created by the phase calculator to adapt threshold, if the threshold
     * mode is adaptive.
     */
    void handleEvent(const EventChannel* eventInfo, const MidiMessage& event,
        int samplePosition = 0) override;

    // Restart the learning rate decaying process (updating start and min learning rates to match UI)
    void restartAdaptiveThreshold();

    /* Calculates the error of x from the indicatorTarget, taking the indicatorRange
     * into account if enabled. That is, if useIndicatorRange is false, just calculates
     * x - indicatorTarget; if useIndicatorRange is true, returns either this difference
     * or the distance via wrapping around the circle, whichever has smaller absolute value.
     */
    float errorFromTarget(float x) const;

    /* Calculates the equivalent value of the given float within the given circular range (2-element array).
     * (e.g. if range[0] == 0, returns the positive float equivalent of x % range[1])
     */
    static float toEquivalentInRange(float x, const float* range);

    // toEquivalentInRange with range = indicatorRange
    float toIndicatorInRange(float x) const;

    // toEquivalentInRange with range = thresholdRange
    float toThresholdInRange(float x) const;

    /* Convert the first element of a binary event to a float, regardless of the type
     * (assumes eventPtr is not null)
     */
    static float floatFromBinaryEvent(BinaryEventPtr& eventPtr);

    // Returns whether the given event chan can be used to train an adaptive threshold.
    static bool isValidIndicatorChan(const EventChannel* eventInfo);

    /********** random threshold ***********/

    // Select a new random threshold using minThresh, maxThresh, and rng.
    float nextRandomThresh();
 
    /********** channel threshold ***********/

    // Retrieves the full source subprocessor ID of the given channel.
    juce::uint32 getSubProcFullID(int chanNum) const;

    /* Returns true if the given chanNum corresponds to an input
     * and that channel has the same source subprocessor as the
     * selected inputChannel, but is not equal to the inputChannel.
     */
    bool isCompatibleWithInput(int chanNum) const;

    // Returns a string to display in the threshold box when using a threshold channel
    static String toChannelThreshString(int chanNum);

    /*********  triggering ************/

    /* Whether there should be a trigger in the given direction (true = rising, float = falling),
     * given the current pastCounter and futureCounter and the passed values and thresholds
     * surrounding the point where a crossing may be.
     */
    bool shouldTrigger(bool direction, float preVal, float postVal, float preThresh, float postThresh);

    /* Add "turning-on" and "turning-off" event for a crossing.
     *  - bufferTs:       Timestamp of start of current buffer
     *  - crossingOffset: Difference betweeen time of actual crossing and bufferTs
     *  - bufferLength:   Number of samples in current buffer
     *  - threshold:      Threshold at the time of the crossing
     *  - crossingLevel:  Level of signal at the first sample after the crossing
     */
    void triggerEvent(juce::int64 bufferTs, int crossingOffset, int bufferLength,
        float threshold, float crossingLevel);

    // ------ PARAMETERS ------------

    ThresholdType thresholdType;

    // if using constant threshold:
    float constantThresh;

    // if using adaptive threshold:
    int indicatorChan; // index of the monitored event channel
    float indicatorTarget;
    bool useIndicatorRange;
    float indicatorRange[2];
    double startLearningRate;
    double minLearningRate;
    double decayRate;
    bool adaptThreshPaused;
    bool useAdaptThreshRange;
    float adaptThreshRange[2];

    // if using random thresholds:
    float randomThreshRange[2];
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

    // ------ INTERNALS -----------

    // the next time at which the detector should be reenabled after a timeout period, measured in
    // samples past the start of the current processing buffer. Less than -numNext if there is no scheduled reenable (i.e. the detector is enabled).
    int sampToReenable;

     // counters for delay keeping track of voting samples
    int pastCounter;
    int futureCounter;

    // arrays to implement past/future voting
    CircularArray<float> inputHistory;
    CircularArray<float> thresholdHistory;

    Array<float> currThresholds;

    EventChannel* eventChannelPtr;
    MetaDataDescriptorArray eventMetaDataDescriptors;
    TTLEventPtr turnoffEvent; // holds a turnoff event that must be added in a later buffer

    Value thresholdVal; // underlying value of the threshold label

    /* If using adaptive threshold, learning rate evolves by this formula (LR = learning rate, MLR = min learning rate):
     * LR_{t} = (LR_{t-1} - MLR) / divisor_{t} + MLR
     * divisor_{0} = 1
     * divisor_{t} = divisor_{t-1} + decay
     */
    double currLearningRate;
    double currMinLearningRate;
    double currLRDivisor;  // what the LR was last divided by
    
    String indicatorChanName; // save so that we can try to find a matching channel when updating

    Random rng; // for random thresholds

    // full subprocessor ID of input channel (or 0 if none selected)
    juce::uint32 validSubProcFullID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossingDetector);
};

#endif // CROSSING_DETECTOR_H_INCLUDED
