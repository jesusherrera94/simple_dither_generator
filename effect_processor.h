#pragma once
#include "dsp_primitives.h"
#include <atomic>
#include <cstdint>


class EffectProcessor {
public:
    explicit EffectProcessor(float sampleRate);
    void setKnob(uint32_t index, float value);
    void setSwitch(uint32_t index, int32_t position);
    void setDitherEnabled(bool enabled);
    float processSample(float input);
    
private:
    static constexpr int kMaxBitDepth = 16;
    static constexpr int kMinBitDepth = 4;
    
    std::atomic<float> volume_{1.0f};
    std::atomic<int> bitDepth_{kMaxBitDepth};
    std::atomic<dsp::DitherType> ditherType_{dsp::DitherType::Triangular};
    std::atomic<bool> ditherEnabled_{false};
    
    dsp::DitherGenerator dither_;
};
