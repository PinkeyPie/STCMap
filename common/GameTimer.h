#pragma once
#include "CommonGlobal.h"

class COMMON_EXPORT GameTimer {
public:
    GameTimer();

    [[nodiscard]] float TotalTime() const; // in seconds
    [[nodiscard]] float DeltaTime() const; // in seconds

    void Reset();   // Call before message loop
    void Start();   // Call when updated
    void Stop();    // Call when paused
    void Tick();    // Call every frame

private:
    double _nSecondsPerCount;
    double _nDeltaTime;

    __int64 _nBaseTime;
    __int64 _nPausedTime;
    __int64 _nStopTime;
    __int64 _nPrevTime;
    __int64 _nCurrTime;

    bool _bStopped;
};
