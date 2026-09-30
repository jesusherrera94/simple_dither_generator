#pragma once
#include "dsp_primitives.h"
#include <atomic>
#include <cstdint>

// Platform-agnostic DSP core for simple_dither_generator.
//
// It knows nothing about Hothouse/Daisy or HeadroomLab — it just processes
// audio and exposes thread-safe control setters. Both the firmware
// (HothouseAdapter) and the simulator (hl_adapter.cpp) drive this same class.
//
// Signal chain:
//
//     input -> [+ dither] -> [quantize to N bits] -> [volume] -> output
//
// The order is the whole lesson: dither has to go in *before* the quantizer,
// and the volume control has to come *after* it.
class EffectProcessor {
public:
    // `sampleRate` is accepted to keep the signature both adapters expect, but
    // this effect does not need it: dither and quantization are memoryless,
    // per-sample operations with no filters or time constants involved. Add a
    // sampleRate_ member if you extend the effect with anything time-dependent.
    explicit EffectProcessor(float sampleRate);

    // --- Thread-safe control setters (called from the UI / control loop) ---
    // Indices follow the Hothouse hardware layout so both adapters can forward
    // their readings unchanged. Unused indices are ignored.
    //
    //   knob 0   -> output volume
    //   knob 1   -> target bit depth
    //   switch 0 -> dither type

    // Knob value in 0.0..1.0.
    void setKnob(uint32_t index, float value);
    // Three-way switch, position 0 = UP, 1 = MID, 2 = DOWN.
    void setSwitch(uint32_t index, int32_t position);
    // Footswitch 1: the stompable dither A/B.
    void setDitherEnabled(bool enabled);

    // --- Audio processing (called from the audio callback) ---
    float processSample(float input);

private:
    // The range knob 2 sweeps. 16 bits is the real-world mastering target the
    // blog post describes, where dither is inaudible by design; 4 bits is
    // coarse enough that both the quantization distortion and the dither that
    // cures it are plainly audible on a guitar.
    static constexpr int kMaxBitDepth = 16;
    static constexpr int kMinBitDepth = 4;

    // Control state, stored atomically for a lock-free hand-off to the audio
    // thread. Knob positions are converted into meaningful units in the setters
    // — that is, at control rate — so that processSample() only ever has to
    // load ready-to-use values. Read them with memory_order_relaxed.
    std::atomic<float> volume_{1.0f};
    std::atomic<int> bitDepth_{kMaxBitDepth};
    std::atomic<dsp::DitherType> ditherType_{dsp::DitherType::Triangular};
    std::atomic<bool> ditherEnabled_{false};

    // Owned as a plain RAII member: it holds only its PRNG state, so there is
    // nothing to allocate.
    dsp::DitherGenerator dither_;
};
