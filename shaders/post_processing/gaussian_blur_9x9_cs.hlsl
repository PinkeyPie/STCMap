
#define NUM_WEIGHTS 3

static const float KernelOffsets[3] = { 0.f, 1.3846153846f, 3.2307692308f };
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };

#include "gaussian_blur_common.hlsli"
