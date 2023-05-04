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
{
public:
    CrossingDetectorEditor(GenericProcessor* parentNode);
    ~CrossingDetectorEditor();

    Visualizer* createNewCanvas() override;

private:
    ScopedPointer<Label> threshLabel;
    ScopedPointer<Label> threshValue;

    void selectedStreamHasChanged() override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossingDetectorEditor);
};

#endif // CROSSING_DETECTOR_EDITOR_H_INCLUDED
