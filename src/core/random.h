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

static float Halton(uint32 index, uint32 base) {
    float fraction = 1.f;
    float result = 0.f;
    while (index > 0) {
        fraction /= (float)base;
        result += fraction * (index % base);
        index = ~~(index / base);
    }
    return result;
}

static vec2 Halton23(uint32 index) {
    return vec2(Halton(index, 2), Halton(index, 3));
}