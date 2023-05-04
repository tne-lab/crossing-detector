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

    constantThreshButton = new ToggleButton("Constant Threshold: ");
    constantThreshButton->setLookAndFeel(&rbLookAndFeel);
    constantThreshButton->setRadioGroupId(threshRadioId, dontSendNotification);
    constantThreshButton->setBounds(bounds = { xPos, yPos, 160, C_TEXT_HT });
    constantThreshButton->setToggleState((int)processor->getParameter("threshold_type")->getValue() == ThresholdType::CONSTANT,
        dontSendNotification);
    constantThreshButton->setTooltip("Use a constant threshold (set on the main editor panel in the signal chain)");
    constantThreshButton->addListener(this);
    optionsPanel->addAndMakeVisible(constantThreshButton);
    opBounds = opBounds.getUnion(bounds);

    constantThreshValue = createEditable("ConstantThresholdValue", String((float)processor->getParameter("constant_threshold")->getValue()),
        "Constant threshold voltage", bounds = { xPos += 160, yPos, 50, C_TEXT_HT });
    constantThreshValue->setEnabled(constantThreshButton->getToggleState());
    optionsPanel->addAndMakeVisible(constantThreshValue);
    opBounds = opBounds.getUnion(bounds);

    thresholdGroupSet->addGroup({ constantThreshButton, constantThreshValue });


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

    auto currStream = processor->getDataStream(processor->getSelectedStream());
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
        DataStream *currStream = processor->getDataStream(processor->getSelectedStream());
        currStream->getParameter("threshold_chan")->setNextValue(channelThreshBox->getSelectedId() - 1);
    }

}

void CrossingDetectorCanvas::labelTextChanged(Label* labelThatHasChanged)
{
    if (labelThatHasChanged == constantThreshValue)
    {
        float newVal;
        float prevVal = (float)processor->getParameter("constant_threshold")->getValue();
        if (updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
            prevVal, &newVal))
        {
            processor->getParameter("constant_threshold")->setNextValue(newVal);
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
            constantThreshValue->setEnabled(true);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::CONSTANT);
        }
    }
    else if (button == randomizeButton)
    {
        bool on = button->getToggleState();
        minThreshEditable->setEnabled(on);
        maxThreshEditable->setEnabled(on);
        if (on)
        {
            constantThreshValue->setEnabled(false);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::RANDOM);
        }
    }
    else if (button == channelThreshButton)
    {
        bool on = button->getToggleState();
        channelThreshBox->setEnabled(on);
        if (on)
        {
            constantThreshValue->setEnabled(false);
            processor->getParameter("threshold_type")->setNextValue(ThresholdType::CHANNEL);
        }
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

    // channel threshold should be selectable iff there are any choices
    channelThreshButton->setEnabled(!channelThreshBoxEmpty);
}

/**************** private ******************/


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