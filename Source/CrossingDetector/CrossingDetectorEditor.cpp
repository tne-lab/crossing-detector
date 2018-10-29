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

#include "CrossingDetectorEditor.h"
#include "CrossingDetector.h"
#include <string>  // stoi, stof
#include <climits> // INT_MAX
#include <cfloat>  // FLT_MAX

CrossingDetectorEditor::CrossingDetectorEditor(GenericProcessor* parentNode, bool useDefaultParameterEditors)
    : VisualizerEditor(parentNode, 205, useDefaultParameterEditors)
{
    tabText = "Crossing Detector";
    CrossingDetector* processor = static_cast<CrossingDetector*>(parentNode);

    const int TEXT_HT = 18;
    /* ------------- Top row (channels) ------------- */
    int xPos = 12;
    int yPos = 36;

    inputLabel = createLabel("InputChanL", "In:", { xPos, yPos, 30, TEXT_HT });
    addAndMakeVisible(inputLabel);

    inputBox = new ComboBox("Input channel");
    inputBox->setTooltip("Continuous channel to analyze");
    inputBox->setBounds(xPos += 33, yPos, 40, TEXT_HT);
    inputBox->addListener(this);
    addAndMakeVisible(inputBox);

    outputLabel = createLabel("OutL", "Out:", { xPos += 50, yPos, 40, TEXT_HT });
    addAndMakeVisible(outputLabel);

    outputBox = new ComboBox("Out event channel");
    for (int chan = 1; chan <= 8; chan++)
        outputBox->addItem(String(chan), chan);
    outputBox->setSelectedId(processor->eventChannel + 1);
    outputBox->setBounds(xPos += 45, yPos, 40, TEXT_HT);
    outputBox->setTooltip("Output event channel");
    outputBox->addListener(this);
    addAndMakeVisible(outputBox);

    /* ------------ Middle row (conditions) -------------- */
    xPos = 20;
    const int Y_MID = yPos + 48;
    const int Y_GAP = 2;
    const int Y_POS_UPPER = Y_MID - (TEXT_HT + Y_GAP / 2);
    const int Y_POS_LOWER = Y_MID + Y_GAP / 2;

    risingButton = new UtilityButton("RISING", Font("Default", 10, Font::plain));
    risingButton->addListener(this);
    risingButton->setBounds(xPos, Y_POS_UPPER, 60, TEXT_HT);
    risingButton->setClickingTogglesState(true);
    bool enable = processor->posOn;
    risingButton->setToggleState(enable, dontSendNotification);
    risingButton->setTooltip("Trigger events when past samples are below and future samples are above the threshold");
    addAndMakeVisible(risingButton);

    fallingButton = new UtilityButton("FALLING", Font("Default", 10, Font::plain));
    fallingButton->addListener(this);
    fallingButton->setBounds(xPos, Y_POS_LOWER, 60, TEXT_HT);
    fallingButton->setClickingTogglesState(true);
    enable = processor->negOn;
    fallingButton->setToggleState(enable, dontSendNotification);
    fallingButton->setTooltip("Trigger events when past samples are above and future samples are below the threshold");
    addAndMakeVisible(fallingButton);

    acrossLabel = createLabel("AcrossL", "threshold:", { xPos += 70, Y_POS_UPPER - 3, 100, TEXT_HT });
    addAndMakeVisible(acrossLabel);

    thresholdEditable = createEditable("Threshold", "", "Threshold voltage",
    { xPos + 5, Y_POS_LOWER - 3, 80, TEXT_HT });
    thresholdEditable->setEnabled(processor->thresholdType == CrossingDetector::CONSTANT);
    // allow processor to override the threshold text
    thresholdEditable->getTextValue().referTo(processor->thresholdVal);
    addAndMakeVisible(thresholdEditable);

    /* --------- Bottom row (timeout) ------------- */
    xPos = 30;
    yPos = Y_MID + 24;

    timeoutLabel = createLabel("TimeoutL", "Timeout:", { xPos, yPos, 64, TEXT_HT });
    addAndMakeVisible(timeoutLabel);

    timeoutEditable = createEditable("Timeout", String(processor->timeout),
        "Minimum length of time between consecutive events", { xPos += 67, yPos, 50, TEXT_HT });
    addAndMakeVisible(timeoutEditable);

    timeoutUnitLabel = createLabel("TimeoutUnitL", "ms", { xPos += 53, yPos, 30, TEXT_HT });
    addAndMakeVisible(timeoutUnitLabel);

    /************** Canvas elements *****************/

    optionsPanel = new Component("CD Options Panel");
    // initial bounds, to be expanded
    juce::Rectangle<int> opBounds(0, 0, 1, 1);
    const int C_TEXT_HT = 25;
    const int LEFT_EDGE = 30;
    const int TAB_WIDTH = 25;

    juce::Rectangle<int> bounds;
    xPos = LEFT_EDGE;
    yPos = 15;

    optionsPanelTitle = new Label("CDOptionsTitle", "Crossing Detector Additional Settings");
    optionsPanelTitle->setBounds(bounds = { xPos, yPos, 400, 50 });
    optionsPanelTitle->setFont(Font(20, Font::bold));
    optionsPanel->addAndMakeVisible(optionsPanelTitle);
    opBounds = opBounds.getUnion(bounds);

    Font subtitleFont(16, Font::bold);

    /* ~~~~~~~~ Threshold type ~~~~~~~~ */

    thresholdGroupSet = new VerticalGroupSet("Threshold controls");
    optionsPanel->addAndMakeVisible(thresholdGroupSet, 0);

    xPos = LEFT_EDGE;
    yPos += 45;

    thresholdTitle = new Label("ThresholdTitle", "Threshold type");
    thresholdTitle->setBounds(bounds = { xPos, yPos, 200, 50 });
    thresholdTitle->setFont(subtitleFont);
    optionsPanel->addAndMakeVisible(thresholdTitle);
    opBounds = opBounds.getUnion(bounds);

    /* -------- Constant threshold --------- */

    xPos += TAB_WIDTH;
    yPos += 45;

    constantThreshButton = new ToggleButton("Constant");
    constantThreshButton->setLookAndFeel(&rbLookAndFeel);
    constantThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    constantThreshButton->setBounds(bounds = { xPos, yPos, 150, C_TEXT_HT });
    constantThreshButton->setToggleState(processor->thresholdType == CrossingDetector::CONSTANT,
        dontSendNotification);
    constantThreshButton->setTooltip("Use a constant threshold (set on the main editor panel in the signal chain)");
    constantThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(constantThreshButton);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({ constantThreshButton });

    /* --------- Adaptive threshold -------- */

    yPos += 40;

    adaptiveThreshButton = new ToggleButton("Optimize correlated inidicator from event channel:");
    adaptiveThreshButton->setLookAndFeel(&rbLookAndFeel);
    adaptiveThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    adaptiveThreshButton->setBounds(bounds = { xPos, yPos, 340, C_TEXT_HT });
    adaptiveThreshButton->setToggleState(processor->thresholdType == CrossingDetector::ADAPTIVE,
        dontSendNotification);
    adaptiveThreshButton->setTooltip(String("Continually adjust the threshold to minimize the error between indicator values sent ") +
        "over the selected channel and the selected target. Assumes that the threshold and the indicator values are correlated " +
        "(but not necessarily linearly), and uses the direction and magnitude of error to calculate adjustments to the threshold.");
    adaptiveThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(adaptiveThreshButton);
    opBounds = opBounds.getUnion(bounds);
    
    indicatorChanBox = new ComboBox("indicatorChanBox");
    indicatorChanBox->setBounds(bounds = { xPos + 340, yPos, 300, C_TEXT_HT });
    indicatorChanBox->addListener(this);
    indicatorChanBox->setTooltip("Binary event channel carrying indicator values");
    indicatorChanBox->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(indicatorChanBox);
    opBounds = opBounds.getUnion(bounds);

    xPos += TAB_WIDTH;
    yPos += 30;

    targetLabel = new Label("targetL", "Target indicator value:");
    targetLabel->setBounds(bounds = { xPos, yPos, 150, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(targetLabel);
    opBounds = opBounds.getUnion(bounds);

    lastTargetEditableString = String(processor->indicatorTarget);
    targetEditable = createEditable("indicator target", lastTargetEditableString,
        "", bounds = { xPos += 150, yPos, 80, C_TEXT_HT });
    targetEditable->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(targetEditable);
    opBounds = opBounds.getUnion(bounds);

    indicatorRangeButton = new ToggleButton("within circular range from");
    indicatorRangeButton->setBounds(bounds = { xPos += 85, yPos, 190, C_TEXT_HT });
    indicatorRangeButton->setEnabled(adaptiveThreshButton->getToggleState());
    indicatorRangeButton->setToggleState(processor->useIndicatorRange, dontSendNotification);
    indicatorRangeButton->setTooltip(String("Treat the range of indicator values as circular and minimize the circular ") +
        "distance from the target, with the minimum and maximum of the range considered equal.");
    indicatorRangeButton->addListener(this);
    optionsPanel->addAndMakeVisible(indicatorRangeButton);
    opBounds = opBounds.getUnion(bounds);

    bool enableRangeControls = adaptiveThreshButton->getToggleState() && indicatorRangeButton->getToggleState();

    lastIndicatorRangeMinString = String(processor->indicatorRange[0]);
    indicatorRangeMinBox = new ComboBox("minimum indicator value");
    indicatorRangeMinBox->setEditableText(true);
    indicatorRangeMinBox->addItem("-180", 1);
    indicatorRangeMinBox->addItem("-pi", 2);
    indicatorRangeMinBox->addItem("0", 3);
    indicatorRangeMinBox->setText(lastIndicatorRangeMinString, dontSendNotification);
    indicatorRangeMinBox->setBounds(bounds = { xPos += 190, yPos, 100, C_TEXT_HT });
    indicatorRangeMinBox->addListener(this);
    indicatorRangeMinBox->setEnabled(enableRangeControls);
    optionsPanel->addAndMakeVisible(indicatorRangeMinBox);
    opBounds = opBounds.getUnion(bounds);

    indicatorRangeTo = new Label("indicatorRangeToL", "to");
    indicatorRangeTo->setBounds(bounds = { xPos += 103, yPos, 25, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(indicatorRangeTo);
    opBounds = opBounds.getUnion(bounds);

    lastIndicatorRangeMaxString = String(processor->indicatorRange[1]);
    indicatorRangeMaxBox = new ComboBox("maximum indicator value");
    indicatorRangeMaxBox->setEditableText(true);
    indicatorRangeMaxBox->addItem("180", 1);
    indicatorRangeMaxBox->addItem("pi", 2);
    indicatorRangeMaxBox->addItem("360", 3);
    indicatorRangeMaxBox->addItem("2*pi", 4);
    indicatorRangeMaxBox->setText(lastIndicatorRangeMaxString, dontSendNotification);
    indicatorRangeMaxBox->setBounds(bounds = { xPos += 28, yPos, 100, C_TEXT_HT });
    indicatorRangeMaxBox->addListener(this);
    indicatorRangeMaxBox->setEnabled(enableRangeControls);
    optionsPanel->addAndMakeVisible(indicatorRangeMaxBox);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + 2 * TAB_WIDTH;
    yPos += 30;

    learningRateLabel = new Label("learningRateL", "Start learning rate at");
    learningRateLabel->setBounds(bounds = { xPos, yPos, 145, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(learningRateLabel);
    opBounds = opBounds.getUnion(bounds);

    learningRateEditable = createEditable("learningRateE", String(processor->startLearningRate),
        String("Initial amount by which the indicator error is multiplied to obtain a correction factor, which ") +
        "is subtracted from the threshold. Use a negative learning rate if the indicator is negatively correlated " +
        "with the threshold. If the decay rate is 0, the learning rate stays constant.",
        bounds = { xPos += 145, yPos, 60, C_TEXT_HT });
    learningRateEditable->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(learningRateEditable);
    opBounds = opBounds.getUnion(bounds);

    minLearningRateLabel = new Label("minLearningRateL", "and approach");
    minLearningRateLabel->setBounds(bounds = { xPos += 60, yPos, 95, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(minLearningRateLabel);
    opBounds = opBounds.getUnion(bounds);

    minLearningRateEditable = createEditable("minLearningRateE", String(processor->minLearningRate),
        String("Learning rate to approach in the limit if decay rate is nonzero (updated on restart)"),
        bounds = { xPos += 95, yPos, 60, C_TEXT_HT });
    minLearningRateEditable->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(minLearningRateEditable);
    opBounds = opBounds.getUnion(bounds);

    decayRateLabel = new Label("decayRateL", "with decay rate");
    decayRateLabel->setBounds(bounds = { xPos += 60, yPos, 100, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(decayRateLabel);
    opBounds = opBounds.getUnion(bounds);

    decayRateEditable = createEditable("decayRateE", String(processor->decayRate),
        String("Determines whether the learning rate decreases over time and how quickly. Each time ") +
        "an event is received, the learning rate is divided by (1 + d*t), where d is the decay and t " +
        "is the number of events since the last reset or acquisition start.",
        bounds = { xPos += 100, yPos, 60, C_TEXT_HT });
    decayRateEditable->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(decayRateEditable);
    opBounds = opBounds.getUnion(bounds);

    restartButton = new UtilityButton("RESTART", Font(20));
    restartButton->addListener(this);
    restartButton->setBounds(bounds = { xPos += 65, yPos, 55, C_TEXT_HT });
    restartButton->setEnabled(adaptiveThreshButton->getToggleState());
    restartButton->setTooltip(String("Set the learning rate to the start value and resetart decaying toward the minimum ") +
        "value (if decay rate is nonzero). A restart also happens when acquisition stops and restarts.");
    optionsPanel->addAndMakeVisible(restartButton);
    opBounds = opBounds.getUnion(bounds);

    pauseButton = new UtilityButton("PAUSE", Font(20));
    pauseButton->addListener(this);
    pauseButton->setBounds(bounds = { xPos += 60, yPos, 50, C_TEXT_HT });
    pauseButton->setEnabled(adaptiveThreshButton->getToggleState());
    pauseButton->setTooltip(String("While active, indicator events are ignored."));
    pauseButton->setClickingTogglesState(true);
    pauseButton->setToggleState(processor->adaptThreshPaused, dontSendNotification);
    optionsPanel->addAndMakeVisible(pauseButton);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + 2 * TAB_WIDTH;
    yPos += 30;

    threshRangeButton = new ToggleButton("Keep threshold within circular range from");
    threshRangeButton->setBounds(bounds = { xPos, yPos, 290, C_TEXT_HT });
    threshRangeButton->setEnabled(adaptiveThreshButton->getToggleState());
    threshRangeButton->setToggleState(processor->useAdaptThreshRange, dontSendNotification);
    threshRangeButton->setTooltip(String("Treat the range of threshold values as circular, such that ") +
        "a positive adjustment over the range maximum will wrap to the minimum and vice versa.");
    threshRangeButton->addListener(this);
    optionsPanel->addAndMakeVisible(threshRangeButton);
    opBounds = opBounds.getUnion(bounds);

    enableRangeControls = adaptiveThreshButton->getToggleState() && threshRangeButton->getToggleState();

    lastThreshRangeMinString = String(processor->adaptThreshRange[0]);
    threshRangeMinBox = new ComboBox("minimum threshold");
    threshRangeMinBox->setEditableText(true);
    threshRangeMinBox->addItem("-180", 1);
    threshRangeMinBox->addItem("-pi", 2);
    threshRangeMinBox->addItem("0", 3);
    threshRangeMinBox->setText(lastThreshRangeMinString, dontSendNotification);
    threshRangeMinBox->setBounds(bounds = { xPos += 290, yPos, 100, C_TEXT_HT });
    threshRangeMinBox->addListener(this);
    threshRangeMinBox->setEnabled(enableRangeControls);
    optionsPanel->addAndMakeVisible(threshRangeMinBox);
    opBounds = opBounds.getUnion(bounds);

    threshRangeTo = new Label("threshRangeToL", "to");
    threshRangeTo->setBounds(bounds = { xPos += 103, yPos, 25, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(threshRangeTo);
    opBounds = opBounds.getUnion(bounds);

    lastThreshRangeMaxString = String(processor->adaptThreshRange[1]);
    threshRangeMaxBox = new ComboBox("maximum threshold");
    threshRangeMaxBox->setEditableText(true);
    threshRangeMaxBox->addItem("180", 1);
    threshRangeMaxBox->addItem("pi", 2);
    threshRangeMaxBox->addItem("360", 3);
    threshRangeMaxBox->addItem("2*pi", 4);
    threshRangeMaxBox->setText(lastThreshRangeMaxString, dontSendNotification);
    threshRangeMaxBox->setBounds(bounds = { xPos += 28, yPos, 100, C_TEXT_HT });
    threshRangeMaxBox->addListener(this);
    threshRangeMaxBox->setEnabled(enableRangeControls);
    optionsPanel->addAndMakeVisible(threshRangeMaxBox);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({
        adaptiveThreshButton, indicatorChanBox,
        targetLabel, targetEditable,
        indicatorRangeButton, indicatorRangeMinBox, indicatorRangeTo, indicatorRangeMaxBox,
        learningRateLabel, learningRateEditable,
        decayRateLabel, decayRateEditable,
        restartButton, pauseButton,
        threshRangeButton, threshRangeMinBox, threshRangeTo, threshRangeMaxBox
    });

    /* --------- Random threshold ---------- */

    xPos = LEFT_EDGE + TAB_WIDTH;
    yPos += 40;

    randomizeButton = new ToggleButton("Draw randomly from uniform distribution");
    randomizeButton->setLookAndFeel(&rbLookAndFeel);
    randomizeButton->setRadioGroupId(threshRadioId, dontSendNotification);
    randomizeButton->setBounds(bounds = { xPos, yPos, 300, C_TEXT_HT });
    randomizeButton->setToggleState(processor->thresholdType == CrossingDetector::RANDOM,
        dontSendNotification);
    randomizeButton->setTooltip("After each event, choose a new threshold sampled uniformly at random from the given range");
    randomizeButton->addListener(this);
    optionsPanel->addAndMakeVisible(randomizeButton);
    opBounds = opBounds.getUnion(bounds);

    xPos += TAB_WIDTH;
    yPos += 30;

    minThreshLabel = new Label("MinThreshL", "Minimum:");
    minThreshLabel->setBounds(bounds = { xPos, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(minThreshLabel);
    opBounds = opBounds.getUnion(bounds);

    minThreshEditable = createEditable("MinThreshE", String(processor->randomThreshRange[0]),
        "Minimum threshold voltage", bounds = { xPos += 80, yPos, 50, C_TEXT_HT });
    minThreshEditable->setEnabled(randomizeButton->getToggleState());
    optionsPanel->addAndMakeVisible(minThreshEditable);
    opBounds = opBounds.getUnion(bounds);

    maxThreshLabel = new Label("MaxThreshL", "Maximum:");
    maxThreshLabel->setBounds(bounds = { xPos += 60, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(maxThreshLabel);
    opBounds = opBounds.getUnion(bounds);

    maxThreshEditable = createEditable("MaxThreshE", String(processor->randomThreshRange[1]),
        "Maximum threshold voltage", bounds = { xPos += 80, yPos, 50, C_TEXT_HT });
    maxThreshEditable->setEnabled(randomizeButton->getToggleState());
    optionsPanel->addAndMakeVisible(maxThreshEditable);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({
        randomizeButton,
        minThreshLabel,
        minThreshEditable,
        maxThreshLabel,
        maxThreshEditable
    });
    
    /* ------------ Channel threshold ---------- */

    xPos = LEFT_EDGE + TAB_WIDTH;
    yPos += 40;

    channelThreshButton = new ToggleButton("Use continuous channel #:");
    channelThreshButton->setLookAndFeel(&rbLookAndFeel);
    channelThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    channelThreshButton->setBounds(bounds = { xPos, yPos, 200, C_TEXT_HT });
    channelThreshButton->setToggleState(processor->thresholdType == CrossingDetector::CHANNEL,
        dontSendNotification);
    channelThreshButton->setEnabled(false); // only enabled when channelThreshBox is populated
    channelThreshButton->setTooltip("At each sample, compare the level of the input channel with a given threshold channel");
    channelThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(channelThreshButton);
    opBounds = opBounds.getUnion(bounds);

    channelThreshBox = new ComboBox("channelSelection");
    channelThreshBox->setBounds(bounds = { xPos + 200, yPos, 50, C_TEXT_HT });
    channelThreshBox->addListener(this);
    channelThreshBox->setTooltip(
        "Only channels from the same source subprocessor as the input (but not the input itself) "
        "can be selected.");
    channelThreshBox->setEnabled(channelThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(channelThreshBox);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({ channelThreshButton, channelThreshBox });

    /* ~~~~~~~~~~ Criteria section  ~~~~~~~~~~~~ */

    criteriaGroupSet = new VerticalGroupSet("Event criteria controls");
    optionsPanel->addAndMakeVisible(criteriaGroupSet, 0);

    xPos = LEFT_EDGE;
    yPos += 40;

    criteriaTitle = new Label("criteriaTitle", "Event criteria");
    criteriaTitle->setBounds(bounds = { xPos, yPos, 200, 50 });
    criteriaTitle->setFont(subtitleFont);
    optionsPanel->addAndMakeVisible(criteriaTitle);
    opBounds = opBounds.getUnion(bounds);

    /* --------------- Jump limiting ------------------ */

    xPos += TAB_WIDTH;
    yPos += 45;

    limitButton = new ToggleButton("Limit jump size across threshold (|X[k] - X[k-1]|)");
    limitButton->setBounds(bounds = { xPos, yPos, 340, C_TEXT_HT });
    limitButton->setToggleState(processor->useJumpLimit, dontSendNotification);
    limitButton->addListener(this);
    optionsPanel->addAndMakeVisible(limitButton);
    opBounds = opBounds.getUnion(bounds);

    limitLabel = new Label("LimitL", "Maximum jump size:");
    limitLabel->setBounds(bounds = { xPos += TAB_WIDTH, yPos += 30, 140, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(limitLabel);
    opBounds = opBounds.getUnion(bounds);

    limitEditable = createEditable("LimitE", String(processor->jumpLimit), "",
        bounds = { xPos += 150, yPos, 50, C_TEXT_HT });
    limitEditable->setEnabled(processor->useJumpLimit);
    optionsPanel->addAndMakeVisible(limitEditable);
    opBounds = opBounds.getUnion(bounds);

    criteriaGroupSet->addGroup({ limitButton, limitLabel, limitEditable });

    /* --------------- Sample voting ------------------ */
    xPos = LEFT_EDGE + TAB_WIDTH;
    yPos += 40;

    votingHeader = new Label("VotingHeadL", "Sample voting:");
    votingHeader->setBounds(bounds = { xPos, yPos, 120, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(votingHeader);
    opBounds = opBounds.getUnion(bounds);

    xPos += TAB_WIDTH;
    yPos += 30;

    pastStrictLabel = new Label("PastStrictL", "Require");
    pastStrictLabel->setBounds(bounds = { xPos, yPos, 65, C_TEXT_HT });
    pastStrictLabel->setJustificationType(Justification::centredRight);
    optionsPanel->addAndMakeVisible(pastStrictLabel);
    opBounds = opBounds.getUnion(bounds);

    pastPctEditable = createEditable("PastPctE", String(100 * processor->pastStrict), "",
        bounds = { xPos += 75, yPos, 35, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastPctEditable);
    opBounds = opBounds.getUnion(bounds);

    pastPctLabel = new Label("PastPctL", "% of the");
    pastPctLabel->setBounds(bounds = { xPos += 35, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastPctLabel);
    opBounds = opBounds.getUnion(bounds);

    pastSpanEditable = createEditable("PastSpanE", String(processor->pastSpan), "",
        bounds = { xPos += 70, yPos, 45, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastSpanEditable);
    opBounds = opBounds.getUnion(bounds);

    pastSpanLabel = new Label("PastSpanL", "samples immediately before X[k-1]...");
    pastSpanLabel->setBounds(bounds = { xPos += 50, yPos, 260, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastSpanLabel);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + 2 * TAB_WIDTH;
    yPos += 30;

    futureStrictLabel = new Label("FutureStrictL", "...and");
    futureStrictLabel->setBounds(bounds = { xPos, yPos, 65, C_TEXT_HT });
    futureStrictLabel->setJustificationType(Justification::centredRight);
    optionsPanel->addAndMakeVisible(futureStrictLabel);
    opBounds = opBounds.getUnion(bounds);

    futurePctEditable = createEditable("FuturePctE", String(100 * processor->futureStrict), "",
        bounds = { xPos += 75, yPos, 35, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futurePctEditable);
    opBounds = opBounds.getUnion(bounds);

    futurePctLabel = new Label("FuturePctL", "% of the");
    futurePctLabel->setBounds(bounds = { xPos += 35, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futurePctLabel);
    opBounds = opBounds.getUnion(bounds);

    futureSpanEditable = createEditable("FutureSpanE", String(processor->futureSpan), "",
        bounds = { xPos += 70, yPos, 45, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futureSpanEditable);
    opBounds = opBounds.getUnion(bounds);

    futureSpanLabel = new Label("FutureSpanL", "samples immediately after X[k]...");
    futureSpanLabel->setBounds(bounds = { xPos += 50, yPos, 260, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futureSpanLabel);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + 2 * TAB_WIDTH;
    yPos += 30;

    votingFooter = new Label("VotingFootL", "...to be on the correct side of the threshold.");
    votingFooter->setBounds(bounds = { xPos, yPos, 350, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(votingFooter);
    opBounds = opBounds.getUnion(bounds);

    criteriaGroupSet->addGroup({
        votingHeader,
        pastStrictLabel,   pastPctEditable,   pastPctLabel,   pastSpanEditable,   pastSpanLabel,
        futureStrictLabel, futurePctEditable, futurePctLabel, futureSpanEditable, futureSpanLabel,
        votingFooter
    });

    /* ~~~~~~~~~~~~~~~ Output section ~~~~~~~~~~~~ */

    outputGroupSet = new VerticalGroupSet("Output controls");
    optionsPanel->addAndMakeVisible(outputGroupSet, 0);

    xPos = LEFT_EDGE;
    yPos += 40;

    outputTitle = new Label("outputTitle", "Output options");
    outputTitle->setBounds(bounds = { xPos, yPos, 150, 50 });
    outputTitle->setFont(subtitleFont);
    optionsPanel->addAndMakeVisible(outputTitle);
    opBounds = opBounds.getUnion(bounds);

    /* ------------------ Event duration --------------- */

    xPos += TAB_WIDTH;
    yPos += 45;

    durationLabel = new Label("DurL", "Event duration:");
    durationLabel->setBounds(bounds = { xPos, yPos, 105, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durationLabel);
    opBounds = opBounds.getUnion(bounds);

    durationEditable = createEditable("DurE", String(processor->eventDuration), "",
        bounds = { xPos += 110, yPos, 40, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durationEditable);
    opBounds = opBounds.getUnion(bounds);

    durationUnit = new Label("DurUnitL", "ms");
    durationUnit->setBounds(bounds = { xPos += 45, yPos, 30, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durationUnit);
    opBounds = opBounds.getUnion(bounds);

    outputGroupSet->addGroup({ durationLabel, durationEditable, durationUnit });

    // some extra padding
    opBounds.setBottom(opBounds.getBottom() + 10);
    opBounds.setRight(opBounds.getRight() + 10);

    optionsPanel->setBounds(opBounds);
    thresholdGroupSet->setBounds(opBounds);
    criteriaGroupSet->setBounds(opBounds);
    outputGroupSet->setBounds(opBounds);
}

CrossingDetectorEditor::~CrossingDetectorEditor() {}

void CrossingDetectorEditor::comboBoxChanged(ComboBox* comboBoxThatHasChanged)
{
    auto processor = static_cast<CrossingDetector*>(getProcessor());
    if (comboBoxThatHasChanged == inputBox)
    {
        processor->setParameter(CrossingDetector::INPUT_CHAN,
            static_cast<float>(inputBox->getSelectedId() - 1));
    }

    else if (comboBoxThatHasChanged == outputBox)
    {
        processor->setParameter(CrossingDetector::EVENT_CHAN,
            static_cast<float>(outputBox->getSelectedId() - 1));
    }

    else if (comboBoxThatHasChanged == indicatorChanBox)
    {
        processor->setParameter(CrossingDetector::INDICATOR_CHAN,
            static_cast<float>(indicatorChanBox->getSelectedId() - 1));
    }

    else if (comboBoxThatHasChanged == indicatorRangeMinBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastIndicatorRangeMinString,
            CrossingDetector::MIN_INDICATOR);
        if (std::isfinite(newVal))
        {
            String newText = comboBoxThatHasChanged->getText();
            if (newVal > processor->indicatorRange[1])
            {
                // push max value up to match
                indicatorRangeMaxBox->setText(newText, sendNotificationSync);
            }
            if (newVal > processor->indicatorTarget)
            {
                // push target value up to match
                targetEditable->setText(newText, sendNotification);
            }
        }
    }

    else if (comboBoxThatHasChanged == indicatorRangeMaxBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastIndicatorRangeMaxString,
            CrossingDetector::MAX_INDICATOR);
        if (std::isfinite(newVal))
        {
            String newText = comboBoxThatHasChanged->getText();
            if (newVal < processor->indicatorRange[0])
            {
                // push min value down to match
                indicatorRangeMinBox->setText(newText, sendNotificationSync);
            }
            if (newVal < processor->indicatorTarget)
            {
                // push target down to match
                targetEditable->setText(newText, sendNotification);
            }
        }
    }

    else if (comboBoxThatHasChanged == threshRangeMinBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastThreshRangeMinString,
            CrossingDetector::MIN_ADAPTED_THRESH);
        if (std::isfinite(newVal))
        {
            if (newVal > processor->adaptThreshRange[1])
            {
                // push max value up to match
                threshRangeMaxBox->setText(comboBoxThatHasChanged->getText(), sendNotificationSync);
            }
            if (adaptiveThreshButton->getToggleState() && newVal > processor->constantThresh)
            {
                // push threshold up to match
                thresholdEditable->setText(String(newVal), sendNotification);
            }
        }
    }

    else if (comboBoxThatHasChanged == threshRangeMaxBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastThreshRangeMaxString,
            CrossingDetector::MAX_ADAPTED_THRESH);
        if (std::isfinite(newVal))
        {
            if (newVal < processor->adaptThreshRange[0])
            {
                // push min value down to match
                threshRangeMinBox->setText(comboBoxThatHasChanged->getText(), sendNotificationSync);
            }
            if (adaptiveThreshButton->getToggleState() && newVal < processor->constantThresh)
            {
                // push threshold down to match
                thresholdEditable->setText(String(newVal), sendNotification);
            }
        }
    }

    else if (comboBoxThatHasChanged == channelThreshBox)
    {
        processor->setParameter(CrossingDetector::THRESH_CHAN,
            static_cast<float>(channelThreshBox->getSelectedId() - 1));
    }

}

void CrossingDetectorEditor::labelTextChanged(Label* labelThatHasChanged)
{
    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());

    if (labelThatHasChanged == durationEditable)
    {
        int newVal;
        bool success = updateIntLabel(labelThatHasChanged, 0, INT_MAX, processor->eventDuration, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::EVENT_DUR, static_cast<float>(newVal));
        }
    }
    else if (labelThatHasChanged == timeoutEditable)
    {
        int newVal;
        bool success = updateIntLabel(labelThatHasChanged, 0, INT_MAX, processor->timeout, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::TIMEOUT, static_cast<float>(newVal));
        }
    }
    else if (labelThatHasChanged == thresholdEditable &&
        (processor->thresholdType == CrossingDetector::CONSTANT || processor->thresholdType == CrossingDetector::ADAPTIVE))
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
            processor->constantThresh, &newVal);

        if (success)
        {
            if (processor->thresholdType == CrossingDetector::ADAPTIVE && processor->useAdaptThreshRange)
            {
                // enforce threshold range
                float valInRange = processor->toThresholdInRange(newVal);
                if (valInRange != newVal)
                {
                    labelThatHasChanged->setText(String(valInRange), dontSendNotification);
                    newVal = valInRange;
                }
            }
            processor->setParameter(CrossingDetector::CONST_THRESH, newVal);
        }
    }
    else if (labelThatHasChanged == pastPctEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, 0, 100, 100 * processor->pastStrict, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::PAST_STRICT, newVal / 100);
        }
    }
    else if (labelThatHasChanged == pastSpanEditable)
    {
        int newVal;
        bool success = updateIntLabel(labelThatHasChanged, 0, INT_MAX, processor->pastSpan, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::PAST_SPAN, static_cast<float>(newVal));
        }
    }
    else if (labelThatHasChanged == futurePctEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, 0, 100, 100 * processor->futureStrict, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::FUTURE_STRICT, newVal / 100);
        }
    }
    else if (labelThatHasChanged == futureSpanEditable)
    {
        int newVal;
        bool success = updateIntLabel(labelThatHasChanged, 0, INT_MAX, processor->futureSpan, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::FUTURE_SPAN, static_cast<float>(newVal));
        }
    }
    else if (labelThatHasChanged == minThreshEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged,
            -FLT_MAX, FLT_MAX, processor->randomThreshRange[0], &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::MIN_RAND_THRESH, newVal);
            if (newVal > processor->randomThreshRange[1])
            {
                // push the max thresh up to match
                maxThreshEditable->setText(String(newVal), sendNotificationAsync);
            }
        }
    }
    else if (labelThatHasChanged == maxThreshEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged,
            -FLT_MAX, FLT_MAX, processor->randomThreshRange[1], &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::MAX_RAND_THRESH, newVal);
            if (newVal < processor->randomThreshRange[0])
            {
                // push the min down to match
                minThreshEditable->setText(String(newVal), sendNotificationAsync);
            }
        }
    }
    else if (labelThatHasChanged == limitEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, 0, FLT_MAX, processor->jumpLimit, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::JUMP_LIMIT, newVal);
        }
    }
    else if (labelThatHasChanged == targetEditable)
    {
        float newVal = updateExpressionComponent(labelThatHasChanged, lastTargetEditableString,
            CrossingDetector::INDICATOR_TARGET);
        if (std::isfinite(newVal) && processor->useIndicatorRange)
        {
            // enforce indicator range
            float valInRange = processor->toIndicatorInRange(newVal);
            if (valInRange != newVal)
            {
                lastTargetEditableString = String(valInRange);
                targetEditable->setText(lastTargetEditableString, dontSendNotification);
                targetEditable->setTooltip("");
                processor->setParameter(CrossingDetector::INDICATOR_TARGET, valInRange);
            }
        }
    }
    else if (labelThatHasChanged == learningRateEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
            processor->startLearningRate, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::START_LEARNING_RATE, newVal);
        }
    }
    else if (labelThatHasChanged == minLearningRateEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
            processor->minLearningRate, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::MIN_LEARNING_RATE, newVal);
        }
    }
    else if (labelThatHasChanged == decayRateEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, 0, FLT_MAX,
            processor->decayRate, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::DECAY_RATE, newVal);
        }
    }
}

void CrossingDetectorEditor::buttonEvent(Button* button)
{
    auto processor = static_cast<CrossingDetector*>(getProcessor());
    if (button == risingButton)
    {
        processor->setParameter(CrossingDetector::POS_ON, static_cast<float>(button->getToggleState()));
    }
    else if (button == fallingButton)
    {
        processor->setParameter(CrossingDetector::NEG_ON, static_cast<float>(button->getToggleState()));
    }
    else if (button == limitButton)
    {
        bool limitOn = button->getToggleState();
        limitEditable->setEnabled(limitOn);
        processor->setParameter(CrossingDetector::USE_JUMP_LIMIT, static_cast<float>(limitOn));
    }
    else if (button == indicatorRangeButton)
    {
        bool wrapOn = button->getToggleState();
        if (wrapOn)
        {
            // enforce range on target
            float oldTarget = processor->indicatorTarget;
            float newTarget = processor->toIndicatorInRange(oldTarget);
            if (newTarget != oldTarget)
            {
                targetEditable->setText(String(newTarget), sendNotification);
            }
        }
        if (adaptiveThreshButton->getToggleState())
        {
            indicatorRangeMinBox->setEnabled(wrapOn);
            indicatorRangeMaxBox->setEnabled(wrapOn);
        }
        processor->setParameter(CrossingDetector::USE_INDICATOR_RANGE, static_cast<float>(wrapOn));
    }
    else if (button == restartButton)
    {
        processor->restartAdaptiveThreshold();
    }
    else if (button == pauseButton)
    {
        processor->setParameter(CrossingDetector::ADAPT_PAUSED, static_cast<float>(button->getToggleState()));
    }
    else if (button == threshRangeButton)
    {
        bool wrapOn = button->getToggleState();
        if (wrapOn && adaptiveThreshButton->getToggleState())
        {
            // enforce range on threshold
            float oldThreshold = processor->constantThresh;
            float newThreshold = processor->toThresholdInRange(oldThreshold);
            if (newThreshold != oldThreshold)
            {
                thresholdEditable->setText(String(newThreshold), sendNotification);
            }
        }
        if (adaptiveThreshButton->getToggleState())
        {
            threshRangeMinBox->setEnabled(wrapOn);
            threshRangeMaxBox->setEnabled(wrapOn);
        }
        processor->setParameter(CrossingDetector::USE_THRESH_RANGE, static_cast<float>(wrapOn));
    }

    // threshold radio buttons
    else if (button == constantThreshButton)
    {
        bool on = button->getToggleState();
        if (on)
        {
            thresholdEditable->setEnabled(true);
            processor->setParameter(CrossingDetector::THRESH_TYPE,
                static_cast<float>(CrossingDetector::CONSTANT));
        }
    }
    else if (button == adaptiveThreshButton)
    {
        bool on = button->getToggleState();
        indicatorChanBox->setEnabled(on);
        targetEditable->setEnabled(on);
        indicatorRangeButton->setEnabled(on);
        if (indicatorRangeButton->getToggleState())
        {
            indicatorRangeMinBox->setEnabled(on);
            indicatorRangeMaxBox->setEnabled(on);
        }
        learningRateEditable->setEnabled(on);
        minLearningRateEditable->setEnabled(on);
        decayRateEditable->setEnabled(on);
        restartButton->setEnabled(on);
        pauseButton->setEnabled(on);
        threshRangeButton->setEnabled(on);
        if (threshRangeButton->getToggleState())
        {
            // explicitly turn the button on in order to trigger side effects
            threshRangeButton->setToggleState(false, dontSendNotification);
            threshRangeButton->setToggleState(true, sendNotification);
        }

        if (on)
        {
            thresholdEditable->setEnabled(true);
            processor->setParameter(CrossingDetector::THRESH_TYPE,
                static_cast<float>(CrossingDetector::ADAPTIVE));
        }
    }
    else if (button == randomizeButton)
    {
        bool on = button->getToggleState();
        minThreshEditable->setEnabled(on);
        maxThreshEditable->setEnabled(on);
        if (on)
        {
            thresholdEditable->setEnabled(false);
            processor->setParameter(CrossingDetector::THRESH_TYPE,
                static_cast<float>(CrossingDetector::RANDOM));
        }
    }
    else if (button == channelThreshButton)
    {
        bool on = button->getToggleState();
        channelThreshBox->setEnabled(on);
        if (on)
        {
            thresholdEditable->setEnabled(false);
            processor->setParameter(CrossingDetector::THRESH_TYPE,
                static_cast<float>(CrossingDetector::CHANNEL));
        }
    }
}

void CrossingDetectorEditor::updateSettings()
{
    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());

    // update input combo box
    int numInputs = processor->getNumInputs();
    int currInputId = inputBox->getSelectedId();

    if (numInputs == 0)
    {
        if (currInputId != 0)
        {
            inputBox->clear(sendNotificationSync);
        }
        // else do nothing - box is already empty.
    }
    else
    {
        inputBox->clear(dontSendNotification);
        for (int chan = 1; chan <= numInputs; ++chan)
        {
            // using 1-based ids since 0 is reserved for "nothing selected"
            inputBox->addItem(String(chan), chan);
            if (currInputId == chan)
            {
                inputBox->setSelectedId(chan, dontSendNotification);
            }
        }

        if (inputBox->getSelectedId() == 0)
        {
            // default to first channel
            inputBox->setSelectedId(1, sendNotificationSync);
        }
    }

    // update adaptive event channel combo box
    int numEventChans = processor->getTotalEventChannels();
    String lastChanName = processor->indicatorChanName;
    bool wasEmpty = processor->indicatorChan == -1;
    indicatorChanBox->clear(dontSendNotification);

    for (int chan = 1; chan <= numEventChans; ++chan)
    {
        // again, purposely using 1-based ids
        const EventChannel* chanInfo = processor->getEventChannel(chan - 1);
        if (!CrossingDetector::isValidIndicatorChan(chanInfo))
        {
            continue;
        }
        const String& chanName = chanInfo->getName();
        indicatorChanBox->addItem(chanName, chan);
        if (!wasEmpty && chanName == lastChanName)
        {
            indicatorChanBox->setSelectedId(chan, sendNotificationSync);
        }
    }
    if (indicatorChanBox->getSelectedId() == 0)
    {
        if (numEventChans > 0)
        {
            // set to first item by default
            indicatorChanBox->setSelectedItemIndex(0, sendNotificationSync);
        }
        else
        {
            // set id to -1 instead of 0 if empty to force a notification, given that it's been cleared
            indicatorChanBox->setSelectedId(-1, sendNotificationSync);
        }
    }

    // update channel threshold combo box
    updateChannelThreshBox();
}

void CrossingDetectorEditor::updateChannelThreshBox()
{
    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());

    int numInputs = processor->getNumInputs();
    int currThreshId = channelThreshBox->getSelectedId();
    channelThreshBox->clear(dontSendNotification);

    for (int chan = 1; chan <= numInputs; ++chan)
    {
        if (processor->isCompatibleWithInput(chan - 1))
        {
            channelThreshBox->addItem(String(chan), chan);
            if (currThreshId == chan)
            {
                channelThreshBox->setSelectedId(chan, dontSendNotification);
            }
        }
    }

    bool channelThreshBoxEmpty = channelThreshBox->getNumItems() == 0;

    if (channelThreshBox->getSelectedId() == 0)
    {
        if (channelThreshBoxEmpty && channelThreshButton->getToggleState())
        {
            // default to constant threshold
            constantThreshButton->setToggleState(true, sendNotificationAsync);
        }
        else if (!channelThreshBoxEmpty)
        {
            // default to first entry
            channelThreshBox->setSelectedItemIndex(0, sendNotificationAsync);
        }
    }

    // channel threshold should be selectable iff there are any choices
    channelThreshButton->setEnabled(!channelThreshBoxEmpty);
}

void CrossingDetectorEditor::startAcquisition()
{
    inputBox->setEnabled(false);
    pastSpanEditable->getText(false);
    futureSpanEditable->getText(false);
}

void CrossingDetectorEditor::stopAcquisition()
{
    inputBox->setEnabled(true);
    pastSpanEditable->getText(true);
    futureSpanEditable->getText(true);
}

Visualizer* CrossingDetectorEditor::createNewCanvas()
{
    canvas = new CrossingDetectorCanvas(getProcessor());
    return canvas;
}

Component* CrossingDetectorEditor::getOptionsPanel()
{
    return optionsPanel.get();
}

void CrossingDetectorEditor::saveCustomParameters(XmlElement* xml)
{
    VisualizerEditor::saveCustomParameters(xml);

    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());

    xml->setAttribute("Type", "CrossingDetectorEditor");
    XmlElement* paramValues = xml->createNewChildElement("VALUES");

    // channels
    paramValues->setAttribute("inputChanId", inputBox->getSelectedId());
    paramValues->setAttribute("outputChanId", outputBox->getSelectedId());

    // rising/falling
    paramValues->setAttribute("bRising", risingButton->getToggleState());
    paramValues->setAttribute("bFalling", fallingButton->getToggleState());

    // threshold
    paramValues->setAttribute("thresholdType", processor->thresholdType);
    paramValues->setAttribute("threshold", processor->constantThresh);
    paramValues->setAttribute("indicatorChanName", processor->indicatorChanName);
    paramValues->setAttribute("indicatorTarget", targetEditable->getText());
    paramValues->setAttribute("useIndicatorRange", indicatorRangeButton->getToggleState());
    paramValues->setAttribute("indicatorRangeMin", indicatorRangeMinBox->getText());
    paramValues->setAttribute("indicatorRangeMax", indicatorRangeMaxBox->getText());
    paramValues->setAttribute("learningRate", learningRateEditable->getText());
    paramValues->setAttribute("minLearningRate", minLearningRateEditable->getText());
    paramValues->setAttribute("decayRate", decayRateEditable->getText());
    paramValues->setAttribute("useAdaptThreshRange", threshRangeButton->getToggleState());
    paramValues->setAttribute("adaptThreshRangeMin", threshRangeMinBox->getText());
    paramValues->setAttribute("adaptThreshRangeMax", threshRangeMaxBox->getText());
    paramValues->setAttribute("minThresh", minThreshEditable->getText());
    paramValues->setAttribute("maxThresh", maxThreshEditable->getText());
    paramValues->setAttribute("thresholdChanId", channelThreshBox->getSelectedId());

    // voting
    paramValues->setAttribute("pastPctExclusive", pastPctEditable->getText());
    paramValues->setAttribute("pastSpanExclusive", pastSpanEditable->getText());
    paramValues->setAttribute("futurePctExclusive", futurePctEditable->getText());
    paramValues->setAttribute("futureSpanExclusive", futureSpanEditable->getText());

    // jump limit
    paramValues->setAttribute("bJumpLimit", limitButton->getToggleState());
    paramValues->setAttribute("jumpLimit", limitEditable->getText());

    // timing
    paramValues->setAttribute("durationMS", durationEditable->getText());
    paramValues->setAttribute("timeoutMS", timeoutEditable->getText());
}

void CrossingDetectorEditor::loadCustomParameters(XmlElement* xml)
{
    VisualizerEditor::loadCustomParameters(xml);

    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());

    forEachXmlChildElementWithTagName(*xml, xmlNode, "VALUES")
    {
        // channels
		int inputChanId = xmlNode->getIntAttribute("inputChanId", inputBox->getSelectedId());
		if (inputBox->indexOfItemId(inputChanId) >= 0) // guard against different # of channels
		{
			inputBox->setSelectedId(inputChanId, sendNotificationSync);
		}
        outputBox->setSelectedId(xmlNode->getIntAttribute("outputChanId", outputBox->getSelectedId()), sendNotificationSync);

        // rising/falling
        risingButton->setToggleState(xmlNode->getBoolAttribute("bRising", risingButton->getToggleState()), sendNotificationSync);
        fallingButton->setToggleState(xmlNode->getBoolAttribute("bFalling", fallingButton->getToggleState()), sendNotificationSync);

        // threshold (order is important here!)
        // set indicator chan name directly so that the right one gets selected in future updates
        processor->indicatorChanName = xmlNode->getStringAttribute("indicatorChanName", processor->indicatorChanName);
        indicatorRangeButton->setToggleState(xmlNode->getBoolAttribute("useIndicatorRange", indicatorRangeButton->getToggleState()), sendNotificationSync);
        indicatorRangeMinBox->setText(xmlNode->getStringAttribute("indicatorRangeMin", indicatorRangeMinBox->getText()), sendNotificationSync);
        indicatorRangeMaxBox->setText(xmlNode->getStringAttribute("indicatorRangeMax", indicatorRangeMaxBox->getText()), sendNotificationSync);
        targetEditable->setText(xmlNode->getStringAttribute("indicatorTarget", targetEditable->getText()), sendNotificationSync);
        learningRateEditable->setText(xmlNode->getStringAttribute("learningRate", learningRateEditable->getText()), sendNotificationSync);
        minLearningRateEditable->setText(xmlNode->getStringAttribute("minLearningRate", minLearningRateEditable->getText()), sendNotificationSync);
        decayRateEditable->setText(xmlNode->getStringAttribute("decayRate", decayRateEditable->getText()), sendNotificationSync);
        threshRangeButton->setToggleState(xmlNode->getBoolAttribute("useAdaptThreshRange", threshRangeButton->getToggleState()), sendNotificationSync);
        threshRangeMinBox->setText(xmlNode->getStringAttribute("adaptThreshRangeMin", threshRangeMinBox->getText()), sendNotificationSync);
        threshRangeMaxBox->setText(xmlNode->getStringAttribute("adaptThreshRangeMax", threshRangeMaxBox->getText()), sendNotificationSync);
        thresholdEditable->setText(xmlNode->getStringAttribute("threshold", thresholdEditable->getText()), sendNotificationSync);
        minThreshEditable->setText(xmlNode->getStringAttribute("minThresh", minThreshEditable->getText()), sendNotificationSync);
        maxThreshEditable->setText(xmlNode->getStringAttribute("maxThresh", maxThreshEditable->getText()), sendNotificationSync);
		
		int thresholdChanId = xmlNode->getIntAttribute("thresholdChanId", channelThreshBox->getSelectedId());
		if (channelThreshBox->indexOfItemId(thresholdChanId) >= 0) // guard against different # of channels
		{
			channelThreshBox->setSelectedId(thresholdChanId, sendNotificationSync);
		}
        
        int thresholdType;
        if (xmlNode->hasAttribute("bRandThresh"))
        {
            // (for backwards compatability)
            thresholdType = xmlNode->getBoolAttribute("bRandThresh") ? CrossingDetector::RANDOM : CrossingDetector::CONSTANT;
        }
        else
        {
            thresholdType = xmlNode->getIntAttribute("thresholdType", processor->thresholdType);
        }

        switch (thresholdType)
        {
        case CrossingDetector::CONSTANT:
            constantThreshButton->setToggleState(true, sendNotificationSync);
            break;

        case CrossingDetector::ADAPTIVE:
            adaptiveThreshButton->setToggleState(true, sendNotificationSync);
            break;

        case CrossingDetector::RANDOM:
            randomizeButton->setToggleState(true, sendNotificationSync);
            break;

        case CrossingDetector::CHANNEL:
            channelThreshButton->setToggleState(true, sendNotificationSync);
            break;
        }

        // voting
        pastPctEditable->setText(xmlNode->getStringAttribute("pastPctExclusive", pastPctEditable->getText()), sendNotificationSync);
        pastSpanEditable->setText(xmlNode->getStringAttribute("pastSpanExclusive", pastSpanEditable->getText()), sendNotificationSync);
        futurePctEditable->setText(xmlNode->getStringAttribute("futurePctExclusive", futurePctEditable->getText()), sendNotificationSync);
        futureSpanEditable->setText(xmlNode->getStringAttribute("futureSpanExclusive", futureSpanEditable->getText()), sendNotificationSync);

        // jump limit
        limitButton->setToggleState(xmlNode->getBoolAttribute("bJumpLimit", limitButton->getToggleState()), sendNotificationSync);
        limitEditable->setText(xmlNode->getStringAttribute("jumpLimit", limitEditable->getText()), sendNotificationSync);

        // timing
        durationEditable->setText(xmlNode->getStringAttribute("durationMS", durationEditable->getText()), sendNotificationSync);
        timeoutEditable->setText(xmlNode->getStringAttribute("timeoutMS", timeoutEditable->getText()), sendNotificationSync);

        // backwards compatibility
        // old duration/timeout in samples, convert to ms.
        if (xmlNode->hasAttribute("duration") || xmlNode->hasAttribute("timeout"))
        {
            // use default sample rate of 30000 if no channels present
            int sampleRate = processor->getTotalDataChannels() > 0 ? processor->getDataChannel(0)->getSampleRate() : 30000;
            try
            {
                int durationSamps = std::stoi(xmlNode->getStringAttribute("duration").toRawUTF8());
                int durationMs = jmax((durationSamps * 1000) / sampleRate, 1);
                durationEditable->setText(String(durationMs), sendNotificationAsync);
            }
            catch (const std::logic_error&) { /* give up */ }
            try
            {
                int timeoutSamps = std::stoi(xmlNode->getStringAttribute("timeout").toRawUTF8());
                int timeoutMs = (timeoutSamps * 1000) / sampleRate;
                timeoutEditable->setText(String(timeoutMs), sendNotificationAsync);
            }
            catch (const std::logic_error&) { /* give up */ }
        }

        // old sample voting - convert to settings "exclusive" of X[k-1] and X[k]
        if (xmlNode->hasAttribute("pastPct") || xmlNode->hasAttribute("futurePct"))
        {
            // past
            try
            {
                int pastSpanOld = std::stoi(xmlNode->getStringAttribute("pastSpan").toRawUTF8());
                float pastPctOld = std::stof(xmlNode->getStringAttribute("pastPct").toRawUTF8()) / 100;
                float pastNumOld = ceil(pastSpanOld * pastPctOld);
                if (pastNumOld < 1)
                {
                    CoreServices::sendStatusMessage("Warning: old past span/strictness behavior not supported; using defaults");
                }
                else
                {
                    int pastSpanNew = pastSpanOld - 1;
                    pastSpanEditable->setText(String(pastSpanNew), sendNotificationAsync);
                    float pastNumNew = pastNumOld - 1;
                    float pastPctNew = pastSpanNew > 0 ? pastNumNew / pastSpanNew : 1.0f;
                    pastPctEditable->setText(String(pastPctNew * 100), sendNotificationAsync);
                }
            }
            catch (const std::logic_error&) { /* give up */ }

            // future
            try
            {
                int futureSpanOld = std::stoi(xmlNode->getStringAttribute("futureSpan").toRawUTF8());
                float futurePctOld = std::stof(xmlNode->getStringAttribute("futurePct").toRawUTF8()) / 100;
                float futureNumOld = ceil(futureSpanOld * futurePctOld);
                if (futureNumOld < 1)
                {
                    CoreServices::sendStatusMessage("Warning: old future span/strictness behavior not supported; using defaults");
                }
                else
                {
                    int futureSpanNew = futureSpanOld - 1;
                    futureSpanEditable->setText(String(futureSpanNew), sendNotificationAsync);
                    float futureNumNew = futureNumOld - 1;
                    float futurePctNew = futureSpanNew > 0 ? futureNumNew / futureSpanNew : 1.0f;
                    futurePctEditable->setText(String(futurePctNew * 100), sendNotificationAsync);
                }
            }
            catch (const std::logic_error&) { /* give up */ }
        }
    }
}

/**************** private ******************/

float CrossingDetectorEditor::evalWithPiScope(const String& text, bool* simple)
{
    String parseError;
    Expression expr(text, parseError);
    if (parseError.isEmpty())
    {
        if (simple != nullptr)
        {
            *simple = expr.getType() == Expression::constantType;
        }
        return static_cast<float>(expr.evaluate(PiScope()));
    }
    return NAN;
}

template<typename T>
float CrossingDetectorEditor::updateExpressionComponent(T* component, String& lastText, int paramToChange)
{
    String newText = component->getText();
    bool simple;
    float newVal = evalWithPiScope(newText, &simple);
    if (std::isfinite(newVal))
    {
        getProcessor()->setParameter(paramToChange, newVal);
        lastText = newText;

        if (simple)
        {
            component->setTooltip("");
        }
        else
        {
            component->setTooltip("= " + String(newVal));
        }
    }
    else
    {
        // evaluation error
        CoreServices::sendStatusMessage("Invalid expression for " + component->getName());
        component->setText(lastText, dontSendNotification);
    }

    return newVal;
}

Label* CrossingDetectorEditor::createEditable(const String& name, const String& initialValue,
    const String& tooltip, juce::Rectangle<int> bounds)
{
    Label* editable = new Label(name, initialValue);
    editable->setEditable(true);
    editable->addListener(this);
    editable->setBounds(bounds);
    editable->setColour(Label::backgroundColourId, Colours::grey);
    editable->setColour(Label::textColourId, Colours::white);
    if (tooltip.length() > 0)
    {
        editable->setTooltip(tooltip);
    }
    return editable;
}

Label* CrossingDetectorEditor::createLabel(const String& name, const String& text,
    juce::Rectangle<int> bounds)
{
    Label* label = new Label(name, text);
    label->setBounds(bounds);
    label->setFont(Font("Small Text", 12, Font::plain));
    label->setColour(Label::textColourId, Colours::darkgrey);
    return label;
}

/* Attempts to parse the current text of a label as an int between min and max inclusive.
*  If successful, sets "*out" and the label text to this value and and returns true.
*  Otherwise, sets the label text to defaultValue and returns false.
*/
bool CrossingDetectorEditor::updateIntLabel(Label* label, int min, int max, int defaultValue, int* out)
{
    const String& in = label->getText();
    int parsedInt;
    try
    {
        parsedInt = std::stoi(in.toRawUTF8());
    }
    catch (const std::logic_error&)
    {
        label->setText(String(defaultValue), dontSendNotification);
        return false;
    }

    *out = jmax(min, jmin(max, parsedInt));

    label->setText(String(*out), dontSendNotification);
    return true;
}

// Like updateIntLabel, but for floats
bool CrossingDetectorEditor::updateFloatLabel(Label* label, float min, float max,
    float defaultValue, float* out)
{
    const String& in = label->getText();
    float parsedFloat;
    try
    {
        parsedFloat = std::stof(in.toRawUTF8());
    }
    catch (const std::logic_error&)
    {
        label->setText(String(defaultValue), dontSendNotification);
        return false;
    }

    *out = jmax(min, jmin(max, parsedFloat));

    label->setText(String(*out), dontSendNotification);
    return true;
}

/*************** canvas (extra settings) *******************/

CrossingDetectorCanvas::CrossingDetectorCanvas(GenericProcessor* n)
    : processor(n)
{
    editor = static_cast<CrossingDetectorEditor*>(processor->editor.get());
    viewport = new Viewport();
    Component* optionsPanel = editor->getOptionsPanel();
    viewport->setViewedComponent(optionsPanel, false);
    viewport->setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);
}

CrossingDetectorCanvas::~CrossingDetectorCanvas() {}

void CrossingDetectorCanvas::refreshState() {}
void CrossingDetectorCanvas::update() {}
void CrossingDetectorCanvas::refresh() {}
void CrossingDetectorCanvas::beginAnimation() {}
void CrossingDetectorCanvas::endAnimation() {}
void CrossingDetectorCanvas::setParameter(int, float) {}
void CrossingDetectorCanvas::setParameter(int, int, int, float) {}

void CrossingDetectorCanvas::paint(Graphics& g)
{
    ColourGradient editorBg = editor->getBackgroundGradient();
    g.fillAll(editorBg.getColourAtPosition(0.5)); // roughly matches editor background (without gradient)
}

void CrossingDetectorCanvas::resized()
{
    viewport->setBounds(0, 0, getWidth(), getHeight());
}


/************** RadioButtonLookAndFeel ***********/

void RadioButtonLookAndFeel::drawTickBox(Graphics& g, Component& component,
    float x, float y, float w, float h, const bool ticked, const bool isEnabled,
    const bool isMouseOverButton, const bool isButtonDown)
{
    // call base function with ticked = false
    LookAndFeel_V2::drawTickBox(g, component, x, y, w, h, false, isEnabled, isMouseOverButton, isButtonDown);

    if (ticked)
    {
        // draw black circle
        const float boxSize = w * 0.7f;
        const juce::Rectangle<float> glassSphereBounds(x, y + (h - boxSize) * 0.5f, boxSize, boxSize);
        const float tickSize = boxSize * 0.55f;

        g.setColour(component.findColour(isEnabled ? ToggleButton::tickColourId
            : ToggleButton::tickDisabledColourId));
        g.fillEllipse(glassSphereBounds.withSizeKeepingCentre(tickSize, tickSize));
    }
}

/************ VerticalGroupSet ****************/

VerticalGroupSet::VerticalGroupSet(Colour backgroundColor)
    : Component ()
    , bgColor   (backgroundColor)
    , leftBound (INT_MAX)
    , rightBound(INT_MIN)
{}

VerticalGroupSet::VerticalGroupSet(const String& componentName, Colour backgroundColor)
    : Component (componentName)
    , bgColor   (backgroundColor)
    , leftBound (INT_MAX)
    , rightBound(INT_MIN)
{}

VerticalGroupSet::~VerticalGroupSet() {}

void VerticalGroupSet::addGroup(std::initializer_list<Component*> components)
{
    if (!getParentComponent())
    {
        jassertfalse;
        return;
    }

    DrawableRectangle* thisGroup;
    groups.add(thisGroup = new DrawableRectangle);
    addChildComponent(thisGroup);
    const RelativePoint cornerSize(CORNER_SIZE, CORNER_SIZE);
    thisGroup->setCornerSize(cornerSize);
    thisGroup->setFill(bgColor);

    int topBound = INT_MAX;
    int bottomBound = INT_MIN;
    for (auto component : components)
    {
        Component* componentParent = component->getParentComponent();
        if (!componentParent)
        {
            jassertfalse;
            return;
        }
        int width = component->getWidth();
        int height = component->getHeight();
        Point<int> positionFromItsParent = component->getPosition();
        Point<int> localPosition = getLocalPoint(componentParent, positionFromItsParent);

        // update bounds
        leftBound = jmin(leftBound, localPosition.x - PADDING);
        rightBound = jmax(rightBound, localPosition.x + width + PADDING);
        topBound = jmin(topBound, localPosition.y - PADDING);
        bottomBound = jmax(bottomBound, localPosition.y + height + PADDING);
    }

    // set new background's bounds
    auto bounds = juce::Rectangle<float>::leftTopRightBottom(leftBound, topBound, rightBound, bottomBound);
    thisGroup->setRectangle(bounds);
    thisGroup->setVisible(true);

    // update all other children
    for (DrawableRectangle* group : groups)
    {
        if (group == thisGroup) { continue; }

        topBound = group->getPosition().y;
        bottomBound = topBound + group->getHeight();
        bounds = juce::Rectangle<float>::leftTopRightBottom(leftBound, topBound, rightBound, bottomBound);
        group->setRectangle(bounds);
    }
}