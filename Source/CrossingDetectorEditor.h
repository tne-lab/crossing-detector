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

#ifndef CROSSING_DETECTOR_EDITOR_H_INCLUDED
#define CROSSING_DETECTOR_EDITOR_H_INCLUDED

#include <VisualizerEditorHeaders.h>
#include "CrossingDetector.h"

/*
Editor (in signal chain) contains:
- Input channel selector
- Ouptput event channel selector
- Direction ("rising" and "falling") buttons
- Threshold control (and indicator when random or channel threshold is selected)
- Event timeout control

@see GenericEditor
*/

class CustomButton
    : public ParameterEditor,
      public Button::Listener
{
public:
    /** Constructor*/
    CustomButton(Parameter* param, String label);

    /** Destructor */
    ~CustomButton() { }

    /** Responds to button clicks*/
    void buttonClicked(Button* label);

    /** Ensures button state aligns with underlying parameter*/
    virtual void updateView() override;

    /** Sets component layout*/
    virtual void resized();
    
private:
    
    std::unique_ptr<UtilityButton> button;
    
};

class CrossingDetectorEditor 
    : public VisualizerEditor
    , public Label::Listener
{
public:
    CrossingDetectorEditor(GenericProcessor* parentNode);
    ~CrossingDetectorEditor();

    Visualizer* createNewCanvas() override;

    void setThresholdLabelEnabled(bool enabled);

private:
    ScopedPointer<Label> threshLabel;
    ScopedPointer<Label> threshValue;
    ScopedPointer<Label> constantThreshValue;
    ScopedPointer<Label> acrossLabel;

    // bottom row (timeout)
    ScopedPointer<Label> timeoutLabel;
    ScopedPointer<Label> timeoutEditable;
    ScopedPointer<Label> timeoutUnitLabel;


    void selectedStreamHasChanged() override;

    Label* createEditable(const String& name, const String& initialValue,
        const String& tooltip, juce::Rectangle<int> bounds);

    Label* createLabel(const String& name, const String& text,
        juce::Rectangle<int> bounds);

    void labelTextChanged(Label* labelThatHasChanged) override;


    /* Utilities for parsing entered values
    *  Ouput whether the label contained a valid input; if so, it is stored in *out
    *  and the label is updated with the parsed input. Otherwise, the label is reset
    *  to defaultValue.
    */
    static bool updateIntLabel(Label* label, int min, int max,
        int defaultValue, int* out);
    static bool updateFloatLabel(Label* label, float min, float max,
        float defaultValue, float* out);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossingDetectorEditor);
};

#endif // CROSSING_DETECTOR_EDITOR_H_INCLUDED
