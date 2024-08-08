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

/** Holds settings for one stream's crossing detector */
class CrossingDetectorSettings
{
    friend class CrossingDetectorCanvas;
public:
    /** Constructor -- sets default values*/
    CrossingDetectorSettings();

    /** Destructor*/
    ~CrossingDetectorSettings() { }

    /** Converts parameters specified in ms to samples, and updates the corresponding member variables. */
    void updateSampleRateDependentValues(int eventDuration, int timeout, int bufferEndMask, float averageDecaySeconds);

    /* Crate a "turning-on" or "turning-off" event for a crossing.
     *  - bufferTs:       Timestamp of start of current buffer
     *  - crossingOffset: Difference betweeen time of actual crossing and bufferTs
     *  - bufferLength:   Number of samples in current buffer
     *  - threshold:      Threshold at the time of the crossing
     *  - crossingLevel:  Level of signal at the first sample after the crossing
     *  - learningRate:   Adaptive algorithm's learning rate, 0 if threshold is not adapative
     */
    TTLEventPtr createEvent(juce::int64 bufferTs, int crossingOffset, int bufferLength,
        float threshold, float crossingLevel, bool eventState, double learningRate);

    /** Parameters */

    int inputChannel; //Index of the inut channel
    int eventChannel;
    
    // Receive custom TTL events. Generally from phase calculator indicating current phase
    int indicatorChannel;

    // if using channel threshold:
    int thresholdChannel;
    
    float sampleRate;
    int eventDurationSamp;
    int timeoutSamp;
    int bufferEndMaskSamp;
    float averageNewSampWeight;

    // used to send crossing events
    EventChannel* eventChannelPtr;
    MetadataDescriptorArray eventMetadataDescriptors;
    TTLEventPtr turnoffEvent; // holds a turnoff event that must be added in a later buffer
};


enum ThresholdType { CONSTANT = 0, RANDOM, CHANNEL, NUM_THRESHOLDS, AVERAGE, ADAPTIVE };

class CrossingDetector : public GenericProcessor
{
public:
    CrossingDetector();
    ~CrossingDetector();

    bool hasEditor() const { return true; }
    AudioProcessorEditor* createEditor() override;

    void updateSettings() override;

    void process(AudioSampleBuffer& continuousBuffer) override;

    /** Called when a parameter is updated*/
    void parameterValueChanged(Parameter* param) override;

    bool startAcquisition() override;
    bool stopAcquisition() override;

    void handleTTLEvent(TTLEventPtr event) override;

    /** Get the current selected stream */
    juce::uint16 getSelectedStream() { return selectedStreamId; }

    /** Set the current selected stream */
    void setSelectedStream(juce::uint16 streamId);

    /* Returns true if the given chanNum corresponds to an input
     * and that channel is not equal to the inputChannel.
     */
    bool isCompatibleWithInput(int chanNum);

    static float toEquivalentInRange(float x, const float* range);

    // toEquivalentInRange with range = indicatorRange
    float toIndicatorInRange(float x) const;

    // toEquivalentInRange with range = thresholdRange
    float toThresholdInRange(float x) const;

    void restartAdaptiveThreshold();

    Value thresholdVal; // underlying value of the threshold label
    
private:

    // ---------------------------- PRIVATE FUNCTIONS ----------------------

    /********** random threshold ***********/

    // Select a new random threshold using minThresh, maxThresh, and rng.
    float nextRandomThresh();
 
    /********** channel threshold ***********/

    // Returns a string to display in the threshold box when using a threshold channel
    static String toChannelThreshString(int chanNum);

    /*********  triggering ************/

    /* Whether there should be a trigger in the given direction (true = rising, float = falling),
     * given the current pastCounter and futureCounter and the passed values and thresholds
     * surrounding the point where a crossing may be.
     */
    bool shouldTrigger(bool direction, float preVal, float postVal, float preThresh, float postThresh);


    /* Calculates the error of x from the indicatorTarget, taking the indicatorRange
     * into account if enabled. That is, if useIndicatorRange is false, just calculates
     * x - indicatorTarget; if useIndicatorRange is true, returns either this difference
     * or the distance via wrapping around the circle, whichever has smaller absolute value.
     */
    float errorFromTarget(float x) const;

    // ------ PARAMETERS ------------

    StreamSettings<CrossingDetectorSettings> settings;
    ThresholdType thresholdType;

    // used for the threshold toggle 
    bool toggleThreshold;

    // if using constant threshold:
    float constantThresh;

    // if using RMS average
    float runningSquaredAverage;

    // if using random thresholds:
    float randomThreshRange[2];
    float currRandomThresh;

    // if using adaptive threshold:
    float indicatorTarget;
    bool useIndicatorRange;
    float indicatorRange[2];
    double startLearningRate;
    double minLearningRate;
    double decayRate;
    bool adaptThreshPaused;
    bool useAdaptThreshRange;
    float adaptThreshRange[2];

    bool posOn;
    bool negOn;

    int eventDuration; // in milliseconds
    int timeout; // milliseconds after an event onset when no more events are allowed.

    // if using multiple-of-average threshold:
    float averageDecaySeconds;

    bool useBufferEndMask;
    int bufferEndMaskMs;

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
    float jumpLimitSleep;
    int jumpLimitElapsed;

    // ------ INTERNALS -----------

    // the next time at which the detector should be reenabled after a timeout period, measured in
    // samples past the start of the current processing buffer. Less than -numNext if there is no scheduled reenable (i.e. the detector is enabled).
    int sampToReenable;

     // counters for delay keeping track of voting samples
    int pastSamplesAbove;
    int futureSamplesAbove;

    // arrays to implement past/future voting
    CircularArray<float> inputHistory;
    CircularArray<float> thresholdHistory;

    Array<float> currThresholds;

    


    /* If using adaptive threshold, learning rate evolves by this formula (LR = learning rate, MLR = min learning rate):
     * LR_{t} = (LR_{t-1} - MLR) / divisor_{t} + MLR
     * divisor_{0} = 1
     * divisor_{t} = divisor_{t-1} + decay
     */
    double currLearningRate;
    double currMinLearningRate;
    double currLRDivisor;  // what the LR was last divided by

    // whether RMS average value needs to be initialized
    bool averageNeedsInit;
    
    Random rng; // for random thresholds

    // Selected stream's ID
    juce::uint16 selectedStreamId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossingDetector);
};

#endif // CROSSING_DETECTOR_H_INCLUDED
