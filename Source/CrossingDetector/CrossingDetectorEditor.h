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

#ifndef CROSSING_DETECTOR_EDITOR_H_INCLUDED
#define CROSSING_DETECTOR_EDITOR_H_INCLUDED

#include <VisualizerEditorHeaders.h>
#include <VisualizerWindowHeaders.h>

/*
Editor (in signal chain) contains:
- Input channel selector
- Ouptput event channel selector
- Direction ("rising" and "falling") buttons
- Threshold control (and indicator when random or channel threshold is selected)
- Event timeout control

Canvas/visualizer contains:
- Threshold type selection - constant, random, or channel (with parameters)
- Jump limiting toggle and max jump box
- Voting settings (pre/post event span and strictness)
- Event duration control

@see GenericEditor
*/


// LookAndFeel class with radio-button-style ToggleButton
class RadioButtonLookAndFeel : public LookAndFeel_V2
{
    void drawTickBox(Graphics& g, Component& component, float x, float y, float w, float h,
        const bool ticked, const bool isEnabled,
        const bool isMouseOverButton, const bool isButtonDown) override;
};

class CrossingDetectorCanvas;

class CrossingDetectorEditor 
    : public VisualizerEditor
    , public ComboBox::Listener
    , public Label::Listener
{
public:
    CrossingDetectorEditor(GenericProcessor* parentNode, bool useDefaultParameterEditors = false);
    ~CrossingDetectorEditor();
    void comboBoxChanged(ComboBox* comboBoxThatHasChanged) override;
    void labelTextChanged(Label* labelThatHasChanged) override;

    // overrides GenericEditor
    void buttonEvent(Button* button) override;

    void updateSettings() override;
    void updateChannelThreshBox();

    // disable input channel selection during acquisition so that events work correctly
    void startAcquisition() override;
    void stopAcquisition() override;

    Visualizer* createNewCanvas() override;

    Component* getOptionsPanel();

    void saveCustomParameters(XmlElement* xml) override;
    void loadCustomParameters(XmlElement* xml) override;

private:

    // Basic UI element creation methods. Always register "this" (the editor) as the listener,
    // but may specify a different Component in which to actually display the element.
    Label* createEditable(const String& name, const String& initialValue,
        const String& tooltip, juce::Rectangle<int> bounds);
    Label* createLabel(const String& name, const String& text, juce::Rectangle<int> bounds);

    /* Utilities for parsing entered values
    *  Ouput whether the label contained a valid input; if so, it is stored in *out
    *  and the label is updated with the parsed input. Otherwise, the label is reset
    *  to defaultValue.
    */
    
    static bool updateIntLabel(Label* label, int min, int max,
        int defaultValue, int* out);
    static bool updateFloatLabel(Label* label, float min, float max,
        float defaultValue, float* out);

    RadioButtonLookAndFeel rbLookAndFeel;

    // top row (channels)
    ScopedPointer<Label> inputLabel;
    ScopedPointer<ComboBox> inputBox;
    ScopedPointer<Label> outputLabel;
    ScopedPointer<ComboBox> outputBox;

    // middle row (threshold)
    ScopedPointer<UtilityButton> risingButton;
    ScopedPointer<UtilityButton> fallingButton;
    ScopedPointer<Label> acrossLabel;
    ScopedPointer<Label> thresholdEditable; 
     
    // bottom row (timeout)
    ScopedPointer<Label> timeoutLabel;
    ScopedPointer<Label> timeoutEditable;
    ScopedPointer<Label> timeoutUnitLabel;

    // --- Canvas elements are managed by editor but invisible until visualizer is opened ----
    CrossingDetectorCanvas* canvas;
    ScopedPointer<Component> optionsPanel;

    ScopedPointer<Label> optionsPanelTitle;
    
    ScopedPointer<Label> thresholdTitle;

    const static int threshRadioId = 1;

    ScopedPointer<ToggleButton> constantThreshButton;

    // threshold randomization
    ScopedPointer<ToggleButton> randomizeButton;
    ScopedPointer<Label> minThreshLabel;
    ScopedPointer<Label> minThreshEditable;
    ScopedPointer<Label> maxThreshLabel;
    ScopedPointer<Label> maxThreshEditable;

    // threshold from channel
    ScopedPointer<ToggleButton> channelThreshButton;
    ScopedPointer<ComboBox> channelThreshBox; 

    ScopedPointer<Label> criteriaTitle;

    // jump limiting
    ScopedPointer<ToggleButton> limitButton;
    ScopedPointer<Label> limitLabel;
    ScopedPointer<Label> limitEditable;

    // sample voting
    ScopedPointer<Label> votingHeader;
    
    ScopedPointer<Label> pastStrictLabel;
    ScopedPointer<Label> pastPctEditable;
    ScopedPointer<Label> pastPctLabel;
    ScopedPointer<Label> pastSpanEditable;
    ScopedPointer<Label> pastSpanLabel;

    ScopedPointer<Label> futureStrictLabel;
    ScopedPointer<Label> futurePctEditable;
    ScopedPointer<Label> futurePctLabel;
    ScopedPointer<Label> futureSpanLabel;
    ScopedPointer<Label> futureSpanEditable;

    ScopedPointer<Label> votingFooter;

    ScopedPointer<Label> outputTitle;

    // event duration
    ScopedPointer<Label> durLabel;
    ScopedPointer<Label> durationEditable;
    ScopedPointer<Label> durUnitLabel;
};

// Visualizer window containing additional settings

class CrossingDetectorCanvas : public Visualizer
{
public:
    CrossingDetectorCanvas(GenericProcessor* n);
    ~CrossingDetectorCanvas();
    void refreshState() override;
    void update() override;
    void refresh() override;
    void beginAnimation() override;
    void endAnimation() override;
    void setParameter(int, float) override;
    void setParameter(int, int, int, float) override;

    void paint(Graphics& g) override;
    void resized() override;

    GenericProcessor* processor;
    CrossingDetectorEditor* editor;
private:
    ScopedPointer<Viewport> viewport;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossingDetectorCanvas);
};

#endif // CROSSING_DETECTOR_EDITOR_H_INCLUDED
