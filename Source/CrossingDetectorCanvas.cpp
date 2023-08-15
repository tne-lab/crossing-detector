/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2022 Open Ephys

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

#include "CrossingDetectorCanvas.h"
#include <string>  // stoi, stof
#include <climits> // INT_MAX
#include <cfloat>  // FLT_MAX

/*************** canvas (extra settings) *******************/

CrossingDetectorCanvas::CrossingDetectorCanvas(GenericProcessor* p)
{
    processor = static_cast<CrossingDetector*>(p);
    editor = static_cast<CrossingDetectorEditor*>(processor->getEditor());
    initializeOptionsPanel();
    viewport = new Viewport();
    viewport->setViewedComponent(optionsPanel, false);
    viewport->setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);
}

CrossingDetectorCanvas::~CrossingDetectorCanvas() {}

void CrossingDetectorCanvas::refreshState() {}

void CrossingDetectorCanvas::refresh() {}

void CrossingDetectorCanvas::paint(Graphics& g)
{
    ColourGradient editorBg = editor->getBackgroundGradient();
    g.fillAll(editorBg.getColourAtPosition(0.5)); // roughly matches editor background (without gradient)
}

void CrossingDetectorCanvas::resized()
{
    viewport->setBounds(0, 0, getWidth(), getHeight());
}

void CrossingDetectorCanvas::initializeOptionsPanel()
{
    optionsPanel = new Component("CD Options Panel");
    // initial bounds, to be expanded
    juce::Rectangle<int> opBounds(0, 0, 1, 1);
    const int C_TEXT_HT = 25;
    const int LEFT_EDGE = 30;
    const int TAB_WIDTH = 25;

    juce::Rectangle<int> bounds;
    int xPos = LEFT_EDGE;
    int yPos = 15;

    optionsPanelTitle = new Label("CDOptionsTitle", "Crossing Detector Additional Settings");
    optionsPanelTitle->setBounds(bounds = { xPos, yPos, 400, 50 });
    optionsPanelTitle->setFont(Font("Fira Sans", "Bold", 20.0f));
    optionsPanel->addAndMakeVisible(optionsPanelTitle);
    opBounds = opBounds.getUnion(bounds);

    Font subtitleFont("Fira Sans", "Bold", 16.0f);

    /** ############## THRESHOLD TYPE ############## */

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
    constantThreshButton->setBounds(bounds = { xPos, yPos, 160, C_TEXT_HT });
    constantThreshButton->setToggleState((int)processor->getParameter("threshold_type")->getValue() == ThresholdType::CONSTANT,
        dontSendNotification);
    constantThreshButton->setTooltip("Use a constant threshold (set on the main editor panel in the signal chain)");
    constantThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(constantThreshButton);
    opBounds = opBounds.getUnion(bounds);

    constantThreshValue = createEditable("Threshold", processor->thresholdVal.getValue(), "Threshold voltage",
        { xPos += 160, yPos, 50, C_TEXT_HT });
    constantThreshValue->setEnabled(constantThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(constantThreshValue);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({ constantThreshButton, constantThreshValue });


    /* -------- Multiple of RMS average threshold --------- */

    xPos = LEFT_EDGE + TAB_WIDTH;
    yPos += 45;

    averageThreshButton = new ToggleButton("Multiple of RMS average over");
    averageThreshButton->setLookAndFeel(&rbLookAndFeel);
    averageThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    averageThreshButton->setBounds(bounds = { xPos, yPos, 200, C_TEXT_HT });
    averageThreshButton->setToggleState((int)processor->getParameter("threshold_type")->getValue() == ThresholdType::AVERAGE,
        dontSendNotification);
    averageThreshButton->setTooltip("Use the RMS average amplitude multiplied by a constant (set on the main editor panel in the signal chain)");
    averageThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(averageThreshButton);
    opBounds = opBounds.getUnion(bounds);

    averageTimeEditable = createEditable("AvgTimeE", String((float)processor->getParameter("avg_decay_seconds")->getValue()),
        "Average smoothing window", bounds = { xPos + 210, yPos, 50, C_TEXT_HT });
    averageTimeEditable->setEnabled(averageThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(averageTimeEditable);
    opBounds = opBounds.getUnion(bounds);

    averageTimeLabel = new Label("AvgTimeL", "seconds");
    averageTimeLabel->setBounds(bounds = { xPos + 270, yPos, 50, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(averageTimeLabel);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({
        averageThreshButton,
        averageTimeLabel,
        averageTimeEditable
        });

    /* --------- Adaptive threshold -------- */

    yPos += 40;

    adaptiveThreshButton = new ToggleButton("Optimize correlated indicator from event channel:");
    adaptiveThreshButton->setLookAndFeel(&rbLookAndFeel);
    adaptiveThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    adaptiveThreshButton->setBounds(bounds = { xPos, yPos, 340, C_TEXT_HT });
    adaptiveThreshButton->setToggleState((int)processor->getParameter("threshold_type")->getValue() == ThresholdType::ADAPTIVE,
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
    indicatorChanBox->setTooltip("TTL event channel carrying indicator values");
    indicatorChanBox->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(indicatorChanBox);
    opBounds = opBounds.getUnion(bounds);

    xPos += TAB_WIDTH;
    yPos += 30;

    targetLabel = new Label("targetL", "Target indicator value:");
    targetLabel->setBounds(bounds = { xPos, yPos, 150, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(targetLabel);
    opBounds = opBounds.getUnion(bounds);

    lastTargetEditableString = String((float)processor->getParameter("indicator_target")->getValue());
    targetEditable = createEditable("indicator target", lastTargetEditableString,
        "", bounds = { xPos += 150, yPos, 80, C_TEXT_HT });
    targetEditable->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(targetEditable);
    opBounds = opBounds.getUnion(bounds);

    indicatorRangeButton = new ToggleButton("within circular range from");
    indicatorRangeButton->setBounds(bounds = { xPos += 85, yPos, 190, C_TEXT_HT });
    indicatorRangeButton->setEnabled(adaptiveThreshButton->getToggleState());
    indicatorRangeButton->setToggleState((bool)processor->getParameter("use_indicator_range")->getValue(), dontSendNotification);
    indicatorRangeButton->setTooltip(String("Treat the range of indicator values as circular and minimize the circular ") +
        "distance from the target, with the minimum and maximum of the range considered equal.");
    indicatorRangeButton->addListener(this);
    optionsPanel->addAndMakeVisible(indicatorRangeButton);
    opBounds = opBounds.getUnion(bounds);

    bool enableRangeControls = adaptiveThreshButton->getToggleState() && indicatorRangeButton->getToggleState();

    lastIndicatorRangeMinString = String((float)processor->getParameter("indicator_range_start")->getValue());
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

    lastIndicatorRangeMaxString = String(((float)processor->getParameter("indicator_range_end")->getValue()));
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

    learningRateEditable = createEditable("learningRateE", String((float)processor->getParameter("start_learning_rate")->getValue()),
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

    minLearningRateEditable = createEditable("minLearningRateE", String((float)processor->getParameter("min_learning_rate")->getValue()),
        String("Learning rate to approach in the limit if decay rate is nonzero (updated on restart)"),
        bounds = { xPos += 95, yPos, 60, C_TEXT_HT });
    minLearningRateEditable->setEnabled(adaptiveThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(minLearningRateEditable);
    opBounds = opBounds.getUnion(bounds);

    decayRateLabel = new Label("decayRateL", "with decay rate");
    decayRateLabel->setBounds(bounds = { xPos += 60, yPos, 100, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(decayRateLabel);
    opBounds = opBounds.getUnion(bounds);

    decayRateEditable = createEditable("decayRateE", String((float)processor->getParameter("decay_rate")->getValue()),
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
    pauseButton->setToggleState((bool)processor->getParameter("adapt_threshold_paused")->getValue(), dontSendNotification);
    optionsPanel->addAndMakeVisible(pauseButton);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + 2 * TAB_WIDTH;
    yPos += 30;

    threshRangeButton = new ToggleButton("Keep threshold within circular range from");
    threshRangeButton->setBounds(bounds = { xPos, yPos, 290, C_TEXT_HT });
    threshRangeButton->setEnabled(adaptiveThreshButton->getToggleState());
    threshRangeButton->setToggleState((bool)processor->getParameter("use_adapt_threshold_range")->getValue(), dontSendNotification);
    threshRangeButton->setTooltip(String("Treat the range of threshold values as circular, such that ") +
        "a positive adjustment over the range maximum will wrap to the minimum and vice versa.");
    threshRangeButton->addListener(this);
    optionsPanel->addAndMakeVisible(threshRangeButton);
    opBounds = opBounds.getUnion(bounds);

    enableRangeControls = adaptiveThreshButton->getToggleState() && threshRangeButton->getToggleState();

    lastThreshRangeMinString = processor->getParameter("adapt_threshold_range_start")->getValueAsString();
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

    lastThreshRangeMaxString = processor->getParameter("adapt_threshold_range_end")->getValueAsString();;
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
    randomizeButton->setBounds(bounds = { xPos, yPos, 325, C_TEXT_HT });
    randomizeButton->setToggleState((int)processor->getParameter("threshold_type")->getValue() == ThresholdType::RANDOM,
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

    minThreshEditable = createEditable("MinThreshE", String((float)processor->getParameter("min_random_threshold")->getValue()),
        "Minimum threshold voltage", bounds = { xPos += 80, yPos, 50, C_TEXT_HT });
    minThreshEditable->setEnabled(randomizeButton->getToggleState());
    optionsPanel->addAndMakeVisible(minThreshEditable);
    opBounds = opBounds.getUnion(bounds);

    maxThreshLabel = new Label("MaxThreshL", "Maximum:");
    maxThreshLabel->setBounds(bounds = { xPos += 60, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(maxThreshLabel);
    opBounds = opBounds.getUnion(bounds);

    maxThreshEditable = createEditable("MaxThreshE", String((float)processor->getParameter("max_random_threshold")->getValue()),
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
    channelThreshButton->setToggleState((int)processor->getParameter("threshold_type")->getValue() == ThresholdType::CHANNEL,
        dontSendNotification);
    channelThreshButton->setEnabled(false); // only enabled when channelThreshBox is populated
    channelThreshButton->setTooltip("At each sample, compare the level of the input channel with a given threshold channel");
    channelThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(channelThreshButton);
    opBounds = opBounds.getUnion(bounds);

    channelThreshBox = new ComboBox("channelSelection");
    channelThreshBox->setBounds(bounds = { xPos + 210, yPos, 50, C_TEXT_HT });
    channelThreshBox->addListener(this);
    channelThreshBox->setTooltip(
        "Only channels from the same stream as the input (but not the input itself) "
        "can be selected.");
    channelThreshBox->setEnabled(channelThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(channelThreshBox);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({ channelThreshButton, channelThreshBox });

    /** ############## EVENT CRITERIA ############## */

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
    limitButton->setBounds(bounds = { xPos, yPos, 420, C_TEXT_HT });
    limitButton->setToggleState((bool)processor->getParameter("use_jump_limit")->getValue(), dontSendNotification);
    limitButton->addListener(this);
    optionsPanel->addAndMakeVisible(limitButton);
    opBounds = opBounds.getUnion(bounds);

    limitLabel = new Label("LimitL", "Maximum jump size:");
    limitLabel->setBounds(bounds = { xPos += TAB_WIDTH, yPos += 30, 140, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(limitLabel);
    opBounds = opBounds.getUnion(bounds);

    limitEditable = createEditable("LimitE", String((float)processor->getParameter("jump_limit")->getValue()), "",
        bounds = { xPos += 150, yPos, 50, C_TEXT_HT });
    limitEditable->setEnabled(limitButton->getToggleState());
    optionsPanel->addAndMakeVisible(limitEditable);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + TAB_WIDTH;
    limitSleepLabel = new Label("LimitSL", "Sleep after artifact:");
    limitSleepLabel->setBounds(bounds = { xPos += TAB_WIDTH, yPos += 30, 140, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(limitSleepLabel);
    opBounds = opBounds.getUnion(bounds);

    limitSleepEditable = createEditable("LimitSE", String((float)processor->getParameter("jump_limit_sleep")->getValue()), "",
        bounds = { xPos += 150, yPos, 50, C_TEXT_HT });
    limitSleepEditable->setEnabled(limitButton->getToggleState());
    optionsPanel->addAndMakeVisible(limitSleepEditable);
    opBounds = opBounds.getUnion(bounds);

    criteriaGroupSet->addGroup({ limitButton, limitLabel, limitEditable, limitSleepLabel, limitSleepEditable });

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

    pastPctEditable = createEditable("PastPctE", String(100 * (float)processor->getParameter("past_strict")->getValue()), "",
        bounds = { xPos += 75, yPos, 35, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastPctEditable);
    opBounds = opBounds.getUnion(bounds);

    pastPctLabel = new Label("PastPctL", "% of the");
    pastPctLabel->setBounds(bounds = { xPos += 35, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastPctLabel);
    opBounds = opBounds.getUnion(bounds);

    pastSpanEditable = createEditable("PastSpanE", String((int)processor->getParameter("past_span")->getValue()), "",
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

    futurePctEditable = createEditable("FuturePctE", String(100 * (float)processor->getParameter("future_strict")->getValue()), "",
        bounds = { xPos += 75, yPos, 35, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futurePctEditable);
    opBounds = opBounds.getUnion(bounds);

    futurePctLabel = new Label("FuturePctL", "% of the");
    futurePctLabel->setBounds(bounds = { xPos += 35, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futurePctLabel);
    opBounds = opBounds.getUnion(bounds);

    futureSpanEditable = createEditable("FutureSpanE", String((int)processor->getParameter("future_span")->getValue()), "",
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


    /* --------------- Buffer end mask ----------------- */
    xPos = LEFT_EDGE + TAB_WIDTH;
    yPos += 40;

    static const String bufferMaskTT =
        "Each time a new buffer of samples is received, the samples closer to the start have "
        "been waiting to be processed for longer than those at the end, but an event triggered "
        "from any of them will be handled by the rest of the chain at the same time. This adds "
        "some variance to the latency between data and reaction in a closed-loop scenario. Enable "
        "this option to just ignore any crossings before a threshold measured from the end of the buffer.";

    bufferMaskButton = new ToggleButton("Ignore crossings ocurring >");
    bufferMaskButton->setBounds(bounds = { xPos, yPos, 225, C_TEXT_HT });
    bufferMaskButton->setToggleState((bool)processor->getParameter("use_buffer_end_mask")->getValue(), dontSendNotification);
    bufferMaskButton->addListener(this);
    bufferMaskButton->setTooltip(bufferMaskTT);
    optionsPanel->addAndMakeVisible(bufferMaskButton);
    opBounds = opBounds.getUnion(bounds);

    bufferMaskEditable = createEditable("BufMaskE", String((int)processor->getParameter("buffer_end_mask")->getValue()),
        bufferMaskTT, bounds = { xPos += 225, yPos, 40, C_TEXT_HT });
    bufferMaskEditable->setEnabled(bufferMaskButton->getToggleState());
    optionsPanel->addAndMakeVisible(bufferMaskEditable);
    opBounds = opBounds.getUnion(bounds);

    bufferMaskLabel = new Label("BufMaskL", "ms before the end of a buffer.");
    bufferMaskLabel->setBounds(bounds = { xPos += 45, yPos, 250, C_TEXT_HT });
    bufferMaskLabel->setTooltip(bufferMaskTT);
    optionsPanel->addAndMakeVisible(bufferMaskLabel);
    opBounds = opBounds.getUnion(bounds);

    criteriaGroupSet->addGroup({ bufferMaskButton, bufferMaskEditable, bufferMaskLabel });

    /** ############## OUTPUT OPTIONS ############## */

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
    durationLabel->setBounds(bounds = { xPos, yPos, 115, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durationLabel);
    opBounds = opBounds.getUnion(bounds);

    durationEditable = createEditable("DurE", String((int)processor->getParameter("event_duration")->getValue()), "",
        bounds = { xPos += 120, yPos, 40, C_TEXT_HT });
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


void CrossingDetectorCanvas::comboBoxChanged(ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == channelThreshBox)
    {
        DataStream* currStream = processor->getDataStream(processor->getSelectedStream());
        currStream->getParameter("threshold_chan")->setNextValue(channelThreshBox->getSelectedId() - 1);
    }
    else if (comboBoxThatHasChanged == indicatorChanBox)
    {
        uint16 selStreamId = processor->getSelectedStream();
        if (selStreamId == 0) return;
        DataStream* currStream = processor->getDataStream(processor->getSelectedStream());
        LOGD("Indicator box changed, setting value to:", indicatorChanBox->getSelectedId() - 1);
        currStream->getParameter("indicator_channel")->setNextValue(indicatorChanBox->getSelectedId() - 1);
    }
    else if (comboBoxThatHasChanged == indicatorRangeMinBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastIndicatorRangeMinString,
            "indicator_range_start");
        if (std::isfinite(newVal))
        {
            String newText = comboBoxThatHasChanged->getText();
            if (newVal > (float) processor->getParameter("indicator_range_start")->getValue())
            {
                // push max value up to match
                indicatorRangeMaxBox->setText(newText, sendNotificationSync);
            }
            if (newVal > (float) processor->getParameter("indicator_target")->getValue())
            {
                // push target value up to match
                targetEditable->setText(newText, sendNotification);
            }
        }
    }
    else if (comboBoxThatHasChanged == indicatorRangeMaxBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastIndicatorRangeMaxString,
            "indicator_range_end");
        if (std::isfinite(newVal))
        {
            String newText = comboBoxThatHasChanged->getText();
            if (newVal < (float) processor->getParameter("indicator_range_start")->getValue())
            {
                // push min value down to match
                indicatorRangeMinBox->setText(newText, sendNotificationSync);
            }
            if (newVal < (float)processor->getParameter("indicator_target")->getValue())
            {
                // push target down to match
                targetEditable->setText(newText, sendNotification);
            }
        }
    }
    else if (comboBoxThatHasChanged == threshRangeMinBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastThreshRangeMinString,
            "adapt_threshold_range_start");
        if (std::isfinite(newVal))
        {
            if (newVal > (float)processor->getParameter("adapt_threshold_range_end")->getValue())
            {
                // push max value up to match
                threshRangeMaxBox->setText(comboBoxThatHasChanged->getText(), sendNotificationSync);
            }
        }
    }
    else if (comboBoxThatHasChanged == threshRangeMaxBox)
    {
        float newVal = updateExpressionComponent(comboBoxThatHasChanged, lastThreshRangeMaxString,
            "adapt_threshold_range_end");
        if (std::isfinite(newVal))
        {
            if (newVal < (float)processor->getParameter("adapt_threshold_range_start")->getValue())
            {
                // push min value down to match
                threshRangeMinBox->setText(comboBoxThatHasChanged->getText(), sendNotificationSync);
            }
        }
    }
}

void CrossingDetectorCanvas::labelTextChanged(Label* labelThatHasChanged)
{
    if (labelThatHasChanged == averageTimeEditable)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("avg_decay_seconds")->getValue();
        if (updateFloatLabel(labelThatHasChanged, 0, FLT_MAX, 5, &newVal))
        {
            processor->getParameter("avg_decay_seconds")->setNextValue(newVal);
        }
    }

    // Sample voting editable labels
    else if (labelThatHasChanged == pastPctEditable)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("past_strict")->getValue();
        if (updateFloatLabel(labelThatHasChanged, 0, 100, 100 * prevVal, &newVal))
        {
            processor->getParameter("past_strict")->setNextValue(newVal / 100);
        }
    }
    else if (labelThatHasChanged == pastSpanEditable)
    {
        int newVal;
        int prevVal = (int)processor->getParameter("past_span")->getValue();
        if (updateIntLabel(labelThatHasChanged, 0, INT_MAX, prevVal, &newVal))
        {
            processor->getParameter("past_strict")->setNextValue(newVal);
        }
    }
    else if (labelThatHasChanged == futurePctEditable)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("future_strict")->getValue();
        if (updateFloatLabel(labelThatHasChanged, 0, 100, 100 * prevVal, &newVal))
        {
            processor->getParameter("future_strict")->setNextValue(newVal / 100);
        }
    }
    else if (labelThatHasChanged == futureSpanEditable)
    {
        int newVal;
        int prevVal = (int)processor->getParameter("future_span")->getValue();
        if (updateIntLabel(labelThatHasChanged, 0, INT_MAX, prevVal, &newVal))
        {
            processor->getParameter("future_span")->setNextValue(newVal);
        }
    }

    // Random threshold editable labels
    else if (labelThatHasChanged == minThreshEditable)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("min_random_threshold")->getValue();
        if (updateFloatLabel(labelThatHasChanged,
            -FLT_MAX, FLT_MAX, prevVal, &newVal))
        {
            processor->getParameter("min_random_threshold")->setNextValue(newVal);
            if (newVal > (float)processor->getParameter("max_random_threshold")->getValue())
            {
                // push the max thresh up to match
                maxThreshEditable->setText(String(newVal), sendNotificationAsync);
            }
        }
    }
    else if (labelThatHasChanged == maxThreshEditable)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("max_random_threshold")->getValue();
        if (updateFloatLabel(labelThatHasChanged,
            -FLT_MAX, FLT_MAX, prevVal, &newVal))
        {
            processor->getParameter("max_random_threshold")->setNextValue(newVal);
            if (newVal < (float)processor->getParameter("min_random_threshold")->getValue())
            {
                // push the min down to match
                minThreshEditable->setText(String(newVal), sendNotificationAsync);
            }
        }
    }

    // Event criteria editable labels
    else if (labelThatHasChanged == limitEditable)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("jump_limit")->getValue();
        if (updateFloatLabel(labelThatHasChanged, 0, FLT_MAX, prevVal, &newVal))
        {
            processor->getParameter("jump_limit")->setNextValue(newVal);
        }
    }
    else if (labelThatHasChanged == limitSleepEditable)
    {
		float newVal;
        float prevVal = (float)processor->getParameter("jump_limit_sleep")->getValue();
		if (updateFloatLabel(labelThatHasChanged, 0, FLT_MAX, prevVal, &newVal))
        {
            processor->getParameter("jump_limit_sleep")->setNextValue(newVal);
        }
    }
    else if (labelThatHasChanged == bufferMaskEditable)
    {
        int newVal;
        int prevVal = (int)processor->getParameter("buffer_end_mask")->getValue();
        if (updateIntLabel(labelThatHasChanged, 0, INT_MAX, prevVal, &newVal))
        {
            processor->getParameter("buffer_end_mask")->setNextValue(newVal);
        }
    }

    // Output options editable labels
    else if (labelThatHasChanged == durationEditable)
    {
        int newVal;
        int prevVal = (int)processor->getParameter("event_duration")->getValue();
        if (updateIntLabel(labelThatHasChanged, 0, INT_MAX, prevVal, &newVal))
        {
            processor->getParameter("event_duration")->setNextValue(newVal);
        }
    }
    // Adaptive threshold editable labels
    else if (labelThatHasChanged == targetEditable)
    {
        float newVal = updateExpressionComponent(labelThatHasChanged, lastTargetEditableString, "indicator_target");
        if (std::isfinite(newVal) && (bool)processor->getParameter("use_indicator_range")->getValue())
        {
            // enforce indicator range
            float valInRange = processor->toIndicatorInRange(newVal);
            if (valInRange != newVal)
            {
                lastTargetEditableString = String(valInRange);
                targetEditable->setText(lastTargetEditableString, dontSendNotification);
                targetEditable->setTooltip("");
                processor->getParameter("indicator_target")->setNextValue(valInRange);
            }
        }
    }
    else if (labelThatHasChanged == learningRateEditable)
    {
        float newVal;
        if (updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
            (float)processor->getParameter("start_learning_rate")->getValue(), &newVal))
        {
            processor->getParameter("start_learning_rate")->setNextValue(newVal);
        }
    }
    else if (labelThatHasChanged == minLearningRateEditable)
    {
        float newVal;
        if (updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
            (float)processor->getParameter("min_learning_rate")->getValue(), &newVal))
        {
            processor->getParameter("min_learning_rate")->setNextValue(newVal);
        }
    }
    else if (labelThatHasChanged == decayRateEditable)
    {
        float newVal;
        if (updateFloatLabel(labelThatHasChanged, 0, FLT_MAX,
            (float)processor->getParameter("decay_rate")->getValue(), &newVal))
        {
            processor->getParameter("decay_rate")->setNextValue(newVal);
        }
    }
}

void CrossingDetectorCanvas::buttonClicked(Button* button)
{
    // Buttons for event criteria
    if (button == limitButton)
    {
        bool limitOn = button->getToggleState();
        limitEditable->setEnabled(limitOn);
        limitSleepEditable->setEnabled(limitOn);
        processor->getParameter("use_jump_limit")->setNextValue(limitOn);
    }
    else if (button == bufferMaskButton)
    {
        bool bufMaskOn = button->getToggleState();
        bufferMaskEditable->setEnabled(bufMaskOn);
        processor->getParameter("use_buffer_end_mask")->setNextValue(bufMaskOn);
    }

    // Threshold radio buttons
    else if (button == constantThreshButton)
    {
        bool on = button->getToggleState();
        if (on)
        {
            editor->setThresholdLabelEnabled(true);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::CONSTANT);
        }
    }
    else if (button == averageThreshButton)
    {
        bool on = button->getToggleState();
        if (on)
        {
            averageTimeEditable->setEnabled(true);
            editor->setThresholdLabelEnabled(true);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::AVERAGE);
        }
    }
    else if (button == randomizeButton)
    {
        bool on = button->getToggleState();
        minThreshEditable->setEnabled(on);
        maxThreshEditable->setEnabled(on);
        if (on)
        {
            editor->setThresholdLabelEnabled(false);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::RANDOM);
        }
    }
    else if (button == channelThreshButton)
    {
        bool on = button->getToggleState();
        channelThreshBox->setEnabled(on);
        if (on)
        {
            editor->setThresholdLabelEnabled(false);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::CHANNEL);
        }
    }
    // Adaptiave threshold
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
            editor->setThresholdLabelEnabled(true);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::ADAPTIVE);
        }
    }
    else if (button == indicatorRangeButton)
    {
        bool wrapOn = button->getToggleState();
        if (wrapOn)
        {
            float oldTarget = processor->getParameter("indicator_target")->getValue();
            float newTarget = processor->toIndicatorInRange(oldTarget);

            if (newTarget != oldTarget)
            {
                targetEditable->setText(String(newTarget), dontSendNotification);

            }
        }

        if (adaptiveThreshButton->getToggleState())
        {
            indicatorRangeMinBox->setEnabled(wrapOn);
            indicatorRangeMaxBox->setEnabled(wrapOn);
        }
        processor->getParameter("use_indicator_range")->setNextValue(wrapOn);
    }
    else if (button == restartButton)
    {
        processor->restartAdaptiveThreshold();
    }
    else if (button == pauseButton)
    {
        processor->getParameter("adapt_threshold_paused")->setNextValue(button->getToggleState());
    }
    else if (button == threshRangeButton)
    {
        bool wrapOn = button->getToggleState();
        if (wrapOn && adaptiveThreshButton->getToggleState())
        {
            // enforce range on threshold
            float oldThreshold = processor->getParameter("constant_threshold")->getValue();
            float newThreshold = processor->toThresholdInRange(oldThreshold);
            if (newThreshold != oldThreshold)
            {
                //constantThreshValue->setText(String(newThreshold), sendNotification);
            }
        }
        if (adaptiveThreshButton->getToggleState())
        {
            threshRangeMinBox->setEnabled(wrapOn);
            threshRangeMaxBox->setEnabled(wrapOn);
        }
        processor->getParameter("use_adapt_threshold_range")->setNextValue(static_cast<float>(wrapOn));
    }
}

void CrossingDetectorCanvas::update()
{

    // update channel threshold combo box
    int numChans = 0;
    
    uint16 selStreamId = processor->getSelectedStream();
    if(selStreamId != 0)
        numChans = processor->getDataStream(selStreamId)->getChannelCount();
    
    int currThreshId = channelThreshBox->getSelectedId();
    channelThreshBox->clear(dontSendNotification);

    for (int chan = 1; chan <= numChans; ++chan)
    {
        if(processor->isCompatibleWithInput(chan - 1))
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
            constantThreshButton->setToggleState(true, sendNotificationSync);
        }
        else if (!channelThreshBoxEmpty)
        {
            // default to first entry
            channelThreshBox->setSelectedItemIndex(0, sendNotificationSync);
        }
    }

    // update adaptive event channel combo box
    int numEventChans = processor->getTotalEventChannels();
    indicatorChanBox->clear(dontSendNotification);

    for (int chan = 1; chan <= numEventChans; ++chan)
    {
        // again, purposely using 1-based ids
        const EventChannel* chanInfo = processor->getEventChannel(chan - 1);
        String name = chanInfo->getName();
        if (!CrossingDetectorCanvas::isValidIndicatorChan(chanInfo))
        {
            continue;
        }
        const String& chanName = chanInfo->getName();
        indicatorChanBox->addItem(chanName, chan);
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

    // channel threshold should be selectable iff there are any choices
    channelThreshButton->setEnabled(!channelThreshBoxEmpty);
}

float CrossingDetectorCanvas::evalWithPiScope(const String& text, bool* simple)
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
float CrossingDetectorCanvas::updateExpressionComponent(T* component, String& lastText, String paramToChange)
{
    String newText = component->getText();
    bool simple;
    float newVal = evalWithPiScope(newText, &simple);
    if (std::isfinite(newVal))
    {
        processor->getParameter(paramToChange)->setNextValue(newVal);
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

/**************** private ******************/

bool CrossingDetectorCanvas::isValidIndicatorChan(const EventChannel* eventInfo)
{
    EventChannel::Type type = eventInfo->getType();
    String name = eventInfo->getName();
    auto binaryType = eventInfo->getBinaryDataType();
    bool isBinary = binaryType >= EventChannel::BINARY_BASE_VALUE && type < EventChannel::INVALID;
    int length = eventInfo->getLength();
    bool isNonempty = eventInfo->getLength() > 0;

    if (!isNonempty || type != EventChannel::TTL)
    {
        return false;
    }

    int metadataCount = eventInfo->getMetadataCount();
    if (metadataCount > 0)
    {
        for (int i = 0; i < metadataCount; i++)
        {
            const MetadataValue* val = eventInfo->getMetadataValue(i);

            
            const MetadataDescriptor* desc = eventInfo->getMetadataDescriptor(i);
            if (desc->getIdentifier().contains("crossing.indicator"))
            {
                return true;
            }
        }
    }

    return false;
}

Label* CrossingDetectorCanvas::createEditable(const String& name, const String& initialValue,
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

Label* CrossingDetectorCanvas::createLabel(const String& name, const String& text,
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
bool CrossingDetectorCanvas::updateIntLabel(Label* label, int min, int max, int defaultValue, int* out)
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
bool CrossingDetectorCanvas::updateFloatLabel(Label* label, float min, float max,
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
    const Point<float> cornerSize(CORNER_SIZE, CORNER_SIZE);
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
        juce::Point<int> positionFromItsParent = component->getPosition();
        juce::Point<int> localPosition = getLocalPoint(componentParent, positionFromItsParent);

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