#pragma once

#include <stdbool.h>
#include <stdint.h>

// Prototype: choose one uniformly random frame in each nominal ~100 ms block.
// Selection uses frame index, not arrival-time threshold, avoiding systematic
// selection of particular jitter phases. Rates are approximate; queries may
// occur on adjacent frames at block boundaries (NOT a hard 10 Hz limit).
typedef struct {
    uint32_t rngState;
    uint32_t framesPerBlock;
    uint32_t frameInBlock;
    uint32_t selectedFrame;
} NDL_RenderStatsSampler;

static inline uint32_t ndl_sampler_rand(NDL_RenderStatsSampler *sampler) {
    uint32_t x = sampler->rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sampler->rngState = x;
    return x;
}

static inline void ndl_sampler_restart(NDL_RenderStatsSampler *sampler) {
    if (sampler->framesPerBlock == 0) {
        return; // A media reload may precede video Open in audio-only sessions.
    }
    sampler->frameInBlock = 0;
    sampler->selectedFrame = ndl_sampler_rand(sampler) % sampler->framesPerBlock;
}

static inline void ndl_sampler_init(NDL_RenderStatsSampler *sampler,
                                    int frameRateNumerator, int frameRateDenominator,
                                    uint32_t seed) {
    // Use one sample per nominal ~100 ms frame block, rounding UP the block
    // size to avoid exceeding the target 10 Hz query rate on average.
    if (frameRateNumerator <= 0 || frameRateDenominator <= 0) {
        sampler->framesPerBlock = 6; // fallback for invalid/missing frame rate
    }
    else {
        uint64_t denominator = (uint64_t)frameRateDenominator * 10;
        uint64_t frames = ((uint64_t)frameRateNumerator + denominator - 1) / denominator;
        if (frames < 1) frames = 1;
        if (frames > UINT32_MAX) frames = UINT32_MAX;
        sampler->framesPerBlock = (uint32_t)frames;
    }
    sampler->rngState = seed != 0 ? seed : UINT32_C(0x6d2b79f5);
    ndl_sampler_restart(sampler);
}

static inline bool ndl_sampler_should_query(NDL_RenderStatsSampler *sampler) {
    // Call for every eligible, successfully submitted frame. The first frame
    // has no valid prior timestamp, so it is not eligible to be sampled.
    bool sample = sampler->frameInBlock == sampler->selectedFrame;
    sampler->frameInBlock++;
    if (sampler->frameInBlock >= sampler->framesPerBlock) {
        ndl_sampler_restart(sampler);
    }
    return sample;
}
