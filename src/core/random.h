#pragma once

#include "math.h"

struct RandomNumberGenerator {
    // XOR shift generator
    uint32 State;

    uint32 RandomUint() {
        uint32 x = State;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        State = x;
        return x;
    }

    uint32 RandomUintBetween(uint32 lo, uint32 hi) {
        return RandomUint() % (hi - lo) + lo;
    }

    float RandomFloat01() {
        return RandomUint() / (float)UINT_MAX;
    }

    float RandomFloatBetween(float lo, float hi) {
        return remap(RandomFloat01(), 0.f, 1.f, lo, hi);
    }
};