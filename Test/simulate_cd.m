function [eventTimes, eventTimesActual] = simulate_cd(inputDataFile, eventFile, settings)
% simulate_cd.m: Simuluate crossing detector, outputting an array of times
% (in seconds) when event onsets should occur, given input settings struct.
% Also plots the input data along with the actual and expected event times.
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
% First output is the expected event times, second is the actual times as
% recorded in eventFile.

[data, tsSec] = load_open_ephys_data_faster(inputDataFile);

eventTimes = [];
eventInds = [];

timeoutSamp = round(settings.timeout * settings.Fs / 1000);

kSample = settings.pastSpan+2;
while kSample <= length(data)-settings.futureSpan
    bTurnOn = (settings.bRising && shouldTurnOn(kSample, true)) || ...
              (settings.bFalling && shouldTurnOn(kSample, false));
    
    if bTurnOn
        eventTimes(end+1, 1) = tsSec(kSample);
        eventInds(end+1, 1) = kSample;
        kSample = kSample + timeoutSamp;
    end
    kSample = kSample + 1;
end

% plot data
figure;
plot(tsSec, data);
hold on;
plot(eventTimes, data(eventInds), 'g*');

[eventData, eventTs, eventInfo] = load_open_ephys_data_faster(eventFile);
eventTimesActual = eventTs(eventData == settings.eventChannel - 1 & eventInfo.eventId == 1);
[~, eventTimeActualInds] = min(abs(bsxfun(@minus, tsSec(:), eventTimesActual(:)')));
plot(eventTimesActual, data(eventTimeActualInds), '*r');

legend('raw data', 'expected event times', 'actual event times');

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