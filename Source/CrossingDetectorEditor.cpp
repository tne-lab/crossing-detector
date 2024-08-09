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

#include "CrossingDetectorEditor.h"
#include "CrossingDetectorCanvas.h"
#include <string>  // stoi, stof
#include <climits> // INT_MAX
#include <cfloat>  // FLT_MAX


CustomButton::CustomButton(Parameter* param, String label) : ParameterEditor(param)
{
    button = std::make_unique<UtilityButton>(label, Font("Fira Code", "Regular", 10.0f));
    button->addListener(this);
    button->setClickingTogglesState(true);
    button->setToggleState(false, dontSendNotification);
    addAndMakeVisible(button.get());
    setBounds(0, 0, 70, 18);
}

void CustomButton::buttonClicked(Button*)
{
    param->setNextValue(button->getToggleState());
}

void CustomButton::updateView()
{
    if(param != nullptr)
        button->setToggleState(param->getValue(), dontSendNotification);
}

void CustomButton::resized()
{
    button->setBounds(0, 0, 70, 18);
}

void CrossingDetectorEditor::updateSettings()
{
    timeoutEditable->setText(getProcessor()->getParameter("Timeout_ms")->getValueAsString(), dontSendNotification);
}


CrossingDetectorEditor::CrossingDetectorEditor(GenericProcessor* parentNode)
    : VisualizerEditor(parentNode, "Crossing Detector", 205)
{
    CrossingDetector* processor = static_cast<CrossingDetector*>(parentNode);

    addSelectedChannelsParameterEditor("Channel", 15, 40);

    addComboBoxParameterEditor("TTL_OUT", 110, 20);

    Parameter* customParam = getProcessor()->getParameter("Rising");
    addCustomParameterEditor(new CustomButton(customParam, "Rising"), 15, 70);

    customParam = getProcessor()->getParameter("Falling");
    addCustomParameterEditor(new CustomButton(customParam, "Falling"), 15, 90);

    acrossLabel = createLabel("AcrossL", "THRESHOLD:", { 110, 65, 100, 18 });
    addAndMakeVisible(acrossLabel);

    constantThreshValue = createEditable("Threshold", processor->thresholdVal.getValue(), "Threshold voltage",
        { 110, 80, 85, 18 });

    auto thresholdType = ThresholdType((int)processor->getParameter("threshold_type")->getValue());

    constantThreshValue->setEnabled(thresholdType == ThresholdType::CONSTANT || thresholdType == ThresholdType::ADAPTIVE
    || thresholdType == ThresholdType::AVERAGE);
    addAndMakeVisible(constantThreshValue);
    constantThreshValue->getTextValue().referTo(processor->thresholdVal);

    int xPos = 40;
    int yPos = 110;
    timeoutLabel = createLabel("TimeoutL", "Timeout:", { xPos, yPos, 64, 18 });
    addAndMakeVisible(timeoutLabel);

    timeoutEditable = createEditable("Timeout", processor->getParameter("Timeout_ms")->getValueAsString(),
        "Minimum length of time between consecutive events", { xPos += 67, yPos, 50, 18 });
    addAndMakeVisible(timeoutEditable);

    timeoutUnitLabel = createLabel("TimeoutUnitL", "ms", { xPos += 53, yPos, 30, 18 });
    addAndMakeVisible(timeoutUnitLabel);


}

CrossingDetectorEditor::~CrossingDetectorEditor() {}

Visualizer* CrossingDetectorEditor::createNewCanvas()
{
    CrossingDetectorCanvas* cdc = new CrossingDetectorCanvas(getProcessor());
    return cdc;
}

void CrossingDetectorEditor::selectedStreamHasChanged()
{
    CrossingDetector* processor = (CrossingDetector*)getProcessor();
    processor->setSelectedStream(getCurrentStream());

    // inform the canvas about selected stream updates
    updateVisualizer();
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

void CrossingDetectorEditor::labelTextChanged(Label* labelThatHasChanged)
{
    CrossingDetector* processor = static_cast<CrossingDetector*>(getProcessor());
    if (labelThatHasChanged == constantThreshValue)
    {
        float newVal;
        bool isok;

        ThresholdType thresholdType = ThresholdType((int)processor->getParameter("threshold_type")->getValue());

        isok = false;
        switch (thresholdType)
        {
        case ThresholdType::CONSTANT:
        case ThresholdType::ADAPTIVE:
        case ThresholdType::AVERAGE:
            isok = updateFloatLabel(labelThatHasChanged, -FLT_MAX, FLT_MAX,
                (float) processor->getParameter("constant_threshold")->getValue(), &newVal);
            break;
        default:
            break;
        }

        if (isok && (thresholdType == ThresholdType::ADAPTIVE) && ((bool)processor->getParameter("use_adapt_threshold_range")->getValue()))
        {
            // enforce threshold range
            float valInRange = processor->toThresholdInRange(newVal);
            if (valInRange != newVal)
            {
                labelThatHasChanged->setText(String(valInRange), dontSendNotification);
                newVal = valInRange;
            }
        }

        if (isok)
            processor->getParameter("constant_threshold")->setNextValue(newVal);
    }
    else if (labelThatHasChanged == timeoutEditable)
    {
        int newVal;
        if (updateIntLabel(labelThatHasChanged, 0, INT_MAX, (int)processor->getParameter("Timeout_ms")->getValue(), &newVal))
        {
            processor->getParameter("Timeout_ms")->setNextValue(newVal);
        }
    }
}

Label* CrossingDetectorEditor::createLabel(const String& name, const String& text,
    juce::Rectangle<int> bounds)
{
    Label* label = new Label(name, text);
    label->setBounds(bounds);
    label->setFont(Font("Silkscreen", "Regular", 12));
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

void CrossingDetectorEditor::setThresholdLabelEnabled(bool enabled)
{
    constantThreshValue->setEditable(enabled);
}