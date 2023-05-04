function [eventTimes, eventTimesActual] = simulate_cd(input, settings, eventFile, bPlot)
% simulate_cd.m: Simuluate crossing detector, outputting an array of times
% (in seconds) when event onsets should occur, given input settings struct.
% Also plots the input data along with the actual and expected event times.
%
% 'input' can be a vector of data (in which case eventTimes are in samples)
% or a path to a data file (in which case eventTimes are in seconds, matching
% the timestamps attached to the file).
%
% Settings struct contains the fields:
%
% - threshold
% - Fs         (sample rate)
% - bRising    ("rising" selected)
% - bFalling   ("falling" selected)
% - timeout    (in ms)
% - jumpLimit  ([] = disabled)
% - pastSpan
% - pastPct
% - futureSpan
% - futurePct
% - eventChannel (1-based event output channel)
%
% If 'bPlot' is false, does not make plots.
%
% First output is the expected event times, second is the actual times as
% recorded in eventFile.

if nargin < 3 || isempty(eventFile)
    eventFile = [];
    assert(nargout < 2, 'Must provide event file to get second output (actual event times)');
else
    assert(ischar(input), 'Cannot match timestamps of event file with raw data vector');
end

if nargin < 4
    bPlot = true;
end

if ischar(input)
    assert(exist(input, 'file') == 2, 'File %s does not exist', input);
    [data, ts] = load_open_ephys_data_faster(input);
else
    assert(isvector(input) && isnumeric(input) && isreal(input), 'Invalid input vector');
    data = input(:);
    ts = (1:length(input))';
end

eventTimes = [];
eventInds = [];

timeoutSamp = round(settings.timeout * settings.Fs / 1000);

kSample = settings.pastSpan+2;
while kSample <= length(data)-settings.futureSpan
    bTurnOn = (settings.bRising && shouldTurnOn(kSample, true)) || ...
              (settings.bFalling && shouldTurnOn(kSample, false));
    
    if bTurnOn
        eventTimes(end+1, 1) = ts(kSample);
        eventInds(end+1, 1) = kSample;
        kSample = kSample + timeoutSamp;
    end
    kSample = kSample + 1;
end

if ~isempty(eventFile)
    [eventData, eventTs, eventInfo] = load_open_ephys_data_faster(eventFile);
    eventTimesActual = eventTs(eventData == settings.eventChannel - 1 & eventInfo.eventId == 1);
end    

if bPlot
    % plot data
    figure;
    plot(ts, data);
    hold on;
    plot(eventTimes, data(eventInds), 'g*');
    
    if ~isempty(eventFile)
        % plot actual events        
        [~, eventTimeActualInds] = min(abs(bsxfun(@minus, ts(:), eventTimesActual(:)')));
        plot(eventTimesActual, data(eventTimeActualInds), '*r');        
        legend('raw data', 'expected event times', 'actual event times');
    else
        legend('raw data', 'expected event times');
    end
end

    function bTurnOn = shouldTurnOn(samp, bRising)
        bThreshSat = (bRising == (data(samp-1) - settings.threshold <= 0)) && ...
                     (bRising == (data(samp) - settings.threshold > 0));
        
        if ~isempty(settings.jumpLimit)
            bJumpSat = abs(data(samp) - data(samp-1)) <= settings.jumpLimit;
        else
            bJumpSat = true;
        end
        
        if settings.pastSpan > 0
            pastNum = sum(bRising == (data(samp-1-(1:settings.pastSpan)) - settings.threshold <= 0));
            pastPct = pastNum * 100 / settings.pastSpan;
            bPastSat = pastPct >= settings.pastPct;
        else
            bPastSat = true;
        end
        
        if settings.futureSpan > 0
            futureNum = sum(bRising == (data(samp+(1:settings.futureSpan)) - settings.threshold > 0));
            futurePct = futureNum * 100 / settings.futureSpan;
            bFutureSat = futurePct >= settings.futurePct;
        else
            bFutureSat = true;
        end
        
        bTurnOn = bThreshSat && bJumpSat && bPastSat && bFutureSat;
    end
end