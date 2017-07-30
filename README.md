# Crossing Detector Plugin

This plugin fires a TTL event when a specified input data channel crosses a specified threshold level; the criteria for detection and the output are highly customizable. It does not modify the data channels. Each instance only processes one data channel, but multiple instances can be chained together or placed in parallel.

## How it works:

* With the default settings, a positive ("rising") crossing occurs at sample _t_ if and only if sample _t_-1 is less than the threshold and sample _t_ is greater, and vice versa for a negative ("falling") crossing. However, to make it more robust to noise or just tweak it to fit a certain use case, you can also adjust the span and strictness settings:

  * __Span__ controls how many samples before or after the current sample are considered.

  * Of these, the percent that must be on the right side of the threshold is controlled by __strictness__.
  
* The duration of events, in samples, can be adjusted ("dur").

* __Timeout__ controls the minimum number of samples between two consecutive events (i.e. for this number of samples after an event fires, no more crossings can be detected).

## Installation

The crossing detector has been compiled and tested on Windows and Linux, but currently not on OSX. It shouldn't be too hard to get it working though, since it's a standard plugin that requires no external libraries. Let me know if you're interested in porting it - or just go ahead and do it.

On all platforms:

* Copy `crossing-detector/Source/CrossingDetector` to `plugin-GUI/Source/Plugins/CrossingDetector`.

On Windows (VS2013):

* Copy `crossing-detector/Builds/VisualStudio2013/CrossingDetector` to `plugin-GUI/Builds/VisualStudio2013/Plugins/CrossingDetector`.
`
* In Visual Studio, open the `Plugins.sln` solution and add the new project (`File->Add->Existing Project...` and select `Builds/VisualStudio2013/Plugins/CrossingDetector/CrossingDetector.vcxproj`). Then build the solution.

I hope you find this to be useful!
-Ethan Blackwood ([@ethanbb](https://github.com/ethanbb))
