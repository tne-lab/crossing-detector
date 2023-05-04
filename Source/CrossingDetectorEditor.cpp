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
    button = std::make_unique<UtilityButton>(label, Font("Fira Code", "Regular", 14.0f));
    button->addListener(this);
    button->setClickingTogglesState(true);
    button->setToggleState(false, dontSendNotification);
    addAndMakeVisible(button.get());
    setBounds(0, 0, 70, 22);
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
    button->setBounds(0, 0, 70, 22);
}


CrossingDetectorEditor::CrossingDetectorEditor(GenericProcessor* parentNode)
    : VisualizerEditor(parentNode, "Crossing Detector", 205)
{

    addSelectedChannelsParameterEditor("Channel", 15, 40);

    addComboBoxParameterEditor("TTL_OUT", 110, 25);

    Parameter* customParam = getProcessor()->getParameter("Rising");
    addCustomParameterEditor(new CustomButton(customParam, "Rising"), 15, 70);

    customParam = getProcessor()->getParameter("Falling");
    addCustomParameterEditor(new CustomButton(customParam, "Falling"), 15, 95);

    addTextBoxParameterEditor("Timeout_ms", 110, 75);

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
