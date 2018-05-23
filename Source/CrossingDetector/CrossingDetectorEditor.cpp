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
    constantThreshButton->setTooltip("Use a constant threshold (set on the main editor window)");
    constantThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(constantThreshButton);
    opBounds = opBounds.getUnion(bounds);

    /* --------- Random threshold ---------- */

    yPos += 35;

    randomizeButton = new ToggleButton("Drawn from uniform distribution");
    randomizeButton->setLookAndFeel(&rbLookAndFeel);
    randomizeButton->setRadioGroupId(threshRadioId, dontSendNotification);
    randomizeButton->setBounds(bounds = { xPos, yPos, 250, C_TEXT_HT });
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

    minThreshEditable = createEditable("MinThreshE", String(processor->minRandomThresh),
        "Minimum threshold voltage", bounds = { xPos += 80, yPos, 50, C_TEXT_HT });
    minThreshEditable->setEnabled(randomizeButton->getToggleState());
    optionsPanel->addAndMakeVisible(minThreshEditable);
    opBounds = opBounds.getUnion(bounds);

    maxThreshLabel = new Label("MaxThreshL", "Maximum:");
    maxThreshLabel->setBounds(bounds = { xPos += 60, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(maxThreshLabel);
    opBounds = opBounds.getUnion(bounds);

    maxThreshEditable = createEditable("MaxThreshE", String(processor->maxRandomThresh),
        "Maximum threshold voltage", bounds = { xPos += 80, yPos, 50, C_TEXT_HT });
    maxThreshEditable->setEnabled(randomizeButton->getToggleState());
    optionsPanel->addAndMakeVisible(maxThreshEditable);
    opBounds = opBounds.getUnion(bounds);
    
    /* ------------ Channel threshold ---------- */

    xPos = LEFT_EDGE + TAB_WIDTH;
    yPos += 35;

    channelThreshButton = new ToggleButton("Continuous channel #:");
    channelThreshButton->setLookAndFeel(&rbLookAndFeel);
    channelThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    channelThreshButton->setBounds(bounds = { xPos, yPos, 170, C_TEXT_HT });
    channelThreshButton->setToggleState(processor->thresholdType == CrossingDetector::CHANNEL,
        dontSendNotification);
    channelThreshButton->setEnabled(false); // only enabled when channelThreshBox is populated
    channelThreshButton->setTooltip("At each sample, compare the level of the input channel with a given threshold channel");
    channelThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(channelThreshButton);
    opBounds = opBounds.getUnion(bounds);

    channelThreshBox = new ComboBox("channelSelection");
    channelThreshBox->setBounds(bounds = { xPos + 180, yPos, 50, C_TEXT_HT });
    channelThreshBox->addListener(this);
    channelThreshBox->setTooltip(
        "Only channels from the same source subprocessor as the input (but not the input itself) "
        "can be selected.");
    channelThreshBox->setEnabled(channelThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(channelThreshBox);
    opBounds = opBounds.getUnion(bounds);

    /* ~~~~~~~~~~ Criteria section  ~~~~~~~~~~~~ */

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
    pastPctLabel->setBounds(bounds = { xPos += 40, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastPctLabel);
    opBounds = opBounds.getUnion(bounds);

    pastSpanEditable = createEditable("PastSpanE", String(processor->pastSpan), "",
        bounds = { xPos += 70, yPos, 45, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(pastSpanEditable);
    opBounds = opBounds.getUnion(bounds);

    pastSpanLabel = new Label("PastSpanL", "samples immediately preceding X[k-1]...");
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
    futurePctLabel->setBounds(bounds = { xPos += 40, yPos, 70, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futurePctLabel);
    opBounds = opBounds.getUnion(bounds);

    futureSpanEditable = createEditable("FutureSpanE", String(processor->futureSpan), "",
        bounds = { xPos += 70, yPos, 45, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futureSpanEditable);
    opBounds = opBounds.getUnion(bounds);

    futureSpanLabel = new Label("FutureSpanL", "samples immediately following X[k]...");
    futureSpanLabel->setBounds(bounds = { xPos += 50, yPos, 260, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(futureSpanLabel);
    opBounds = opBounds.getUnion(bounds);

    xPos = LEFT_EDGE + 2 * TAB_WIDTH;
    yPos += 30;

    votingFooter = new Label("VotingFootL", "...to be on the correct side of the threshold.");
    votingFooter->setBounds(bounds = { xPos, yPos, 350, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(votingFooter);
    opBounds = opBounds.getUnion(bounds);

    /* ~~~~~~~~~~~~~~~ Output section ~~~~~~~~~~~~ */

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

    durLabel = new Label("DurL", "Event duration:");
    durLabel->setBounds(bounds = { xPos, yPos, 105, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durLabel);
    opBounds = opBounds.getUnion(bounds);

    durationEditable = createEditable("DurE", String(processor->eventDuration), "",
        bounds = { xPos += 110, yPos, 40, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durationEditable);
    opBounds = opBounds.getUnion(bounds);

    durUnitLabel = new Label("DurUnitL", "ms");
    durUnitLabel->setBounds(bounds = { xPos += 45, yPos, 30, C_TEXT_HT });
    optionsPanel->addAndMakeVisible(durUnitLabel);
    opBounds = opBounds.getUnion(bounds);

    optionsPanel->setBounds(opBounds);
}

CrossingDetectorEditor::~CrossingDetectorEditor() {}

void CrossingDetectorEditor::comboBoxChanged(ComboBox* comboBoxThatHasChanged)
{
    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());
    if (comboBoxThatHasChanged == inputBox)
    {
        getProcessor()->setParameter(CrossingDetector::INPUT_CHAN,
            static_cast<float>(inputBox->getSelectedId() - 1));
    }
    else if (comboBoxThatHasChanged == outputBox)
    {
        getProcessor()->setParameter(CrossingDetector::EVENT_CHAN,
            static_cast<float>(outputBox->getSelectedId() - 1));
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
    else if (labelThatHasChanged == thresholdEditable && processor->thresholdType == CrossingDetector::CONSTANT)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX, processor->constantThresh, &newVal);

        if (success)
        {
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
            -FLT_MAX, processor->maxRandomThresh, processor->minRandomThresh, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::MIN_RAND_THRESH, newVal);
        }
    }
    else if (labelThatHasChanged == maxThreshEditable)
    {
        float newVal;
        bool success = updateFloatLabel(labelThatHasChanged,
            processor->minRandomThresh, FLT_MAX, processor->maxRandomThresh, &newVal);

        if (success)
        {
            processor->setParameter(CrossingDetector::MAX_RAND_THRESH, newVal);
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
}

void CrossingDetectorEditor::buttonEvent(Button* button)
{
    GenericProcessor* processor = getProcessor();
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

    // threshold radio buttons
    else if (button == constantThreshButton)
    {
        bool on = button->getToggleState();
        thresholdEditable->setEnabled(on);
        if (on)
        {
            processor->setParameter(CrossingDetector::THRESH_TYPE,
                static_cast<float>(CrossingDetector::CONSTANT));
        }
    }
    else if (button == randomizeButton)
    {
        bool on = button->getToggleState();
        minThreshEditable->setEnabled(on);
        maxThreshEditable->setEnabled(on);
        if (on)
        {
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
    inputBox->clear(dontSendNotification);

    for (int chan = 1; chan <= numInputs; ++chan)
    {
        // using 1-based ids since 0 is reserved for "nothing selected"
        inputBox->addItem(String(chan), chan);
        if (currInputId == chan)
        {
            inputBox->setSelectedId(chan, sendNotificationAsync);
        }
    }

    if (inputBox->getSelectedId() == 0)
    {
        // set id to -1 instead of 0 if empty to force a notification, given that it's been cleared
        inputBox->setSelectedId((numInputs == 0) ? -1 : 1, sendNotificationAsync);
    }
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
        inputBox->setSelectedId(xmlNode->getIntAttribute("inputChanId", inputBox->getSelectedId()), sendNotificationSync);
        outputBox->setSelectedId(xmlNode->getIntAttribute("outputChanId", outputBox->getSelectedId()), sendNotificationSync);

        // rising/falling
        risingButton->setToggleState(xmlNode->getBoolAttribute("bRising", risingButton->getToggleState()), sendNotificationSync);
        fallingButton->setToggleState(xmlNode->getBoolAttribute("bFalling", fallingButton->getToggleState()), sendNotificationSync);

        // threshold (order is important here!)
        thresholdEditable->setText(xmlNode->getStringAttribute("threshold", thresholdEditable->getText()), sendNotificationSync);
        minThreshEditable->setText(xmlNode->getStringAttribute("minThresh", minThreshEditable->getText()), sendNotificationSync);
        maxThreshEditable->setText(xmlNode->getStringAttribute("maxThresh", maxThreshEditable->getText()), sendNotificationSync);
        channelThreshBox->setSelectedId(xmlNode->getIntAttribute("thresholdChanId", channelThreshBox->getSelectedId()), sendNotificationSync);
        
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
