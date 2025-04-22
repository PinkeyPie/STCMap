//
// Created by Chingis on 18.04.2025.
//

#include "GameTimer.h"
#include "windows.h"

GameTimer::GameTimer() : _nSecondsPerCount(0.0), _nDeltaTime(-1.0), _nBaseTime(0), _nStopTime(0.0),
_nPausedTime(0), _nPrevTime(0), _nCurrTime(0), _bStopped(false) {
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    _nSecondsPerCount = 1.0 / static_cast<double>(countsPerSec);
}

// Returns the total time elapsed since Reset() was called, NOT counting any time when the clock is stopped
float GameTimer::TotalTime() const {
    // If we are stopped, do not count the time that has passed since we stopped.
    // Moreover, if we previously already had a pause, the distance
    // mStopTime - mBaseTime includes paused time, which we do not want to count.
    // To correct this, we can subtract the paused time from mStopTime:
    //
    //                     |<--paused time-->|
    // ----*---------------*-----------------*------------*------------*------> time
    //  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime
    if(_bStopped) {
        return static_cast<float>(((_nStopTime - _nPausedTime) - _nBaseTime) * _nSecondsPerCount);
    }
    // The distance mCurrTime - mBaseTime includes paused time,
    // which we do not want to count.  To correct this, we can subtract
    // the paused time from mCurrTime:
    //
    //  (mCurrTime - mPausedTime) - mBaseTime
    //
    //                     |<--paused time-->|
    // ----*---------------*-----------------*------------*------> time
    //  mBaseTime       mStopTime        startTime     mCurrTime
    return static_cast<float>(((_nCurrTime - _nPausedTime) - _nBaseTime) * _nSecondsPerCount);
}

float GameTimer::DeltaTime() const {
    return static_cast<float>(_nDeltaTime);
}

void GameTimer::Reset() {
    __int64 nCurrTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&nCurrTime));

    _nBaseTime = nCurrTime;
    _nPrevTime = nCurrTime;
    _nStopTime = 0;
    _bStopped = false;
}

void GameTimer::Start() {
    __int64 nStartTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&nStartTime));

    // Accumulate the time elapsed between stop and start pairs.
    //
    //                     |<-------d------->|
    // ----*---------------*-----------------*------------> time
    //  mBaseTime       mStopTime        startTime

    if(_bStopped) {
        _nPausedTime += (nStartTime - _nStopTime);

        _nPrevTime = nStartTime;
        _nStopTime = 0;
        _bStopped = false;
    }
}

void GameTimer::Stop() {
    if(!_bStopped) {
        __int64 nCurrTime;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&nCurrTime));

        _nStopTime = nCurrTime;
        _bStopped = true;
    }
}

void GameTimer::Tick() {
    if(_bStopped) {
        _nDeltaTime = 0.0;
        return;
    }

    __int64 nCurrTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&nCurrTime));
    _nCurrTime = nCurrTime;
    // Time difference between this frame and the previous.
    _nDeltaTime = (_nCurrTime - _nPrevTime) * _nSecondsPerCount;
    // Prepare for next frame.
    _nPrevTime = _nCurrTime;
    // Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the
    // processor goes into a power save mode or we get shuffled to another
    // processor, then mDeltaTime can be negative.
    if(_nDeltaTime < 0.0) {
        _nDeltaTime = 0.0;
    }
}

