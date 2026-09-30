#include "effect_processor.h"
#include <cmath>

// The control atomics are read from the audio callback, so they must be
// lock-free: anything else would hide a mutex in the audio path and risk
// priority inversion, i.e. an audible dropout. Assert it instead of assuming it.
//
// These C++11 macros are used rather than std::atomic<T>::is_always_lock_free
// because the firmware toolchain builds with -std=gnu++14, where that C++17
// trait does not exist. A value of 2 means "always lock-free".
//
// Between them these cover every control member: float matches int's width, and
// DitherType matches char's under the firmware's -fshort-enums.
static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "atomic<bool> must be lock-free");
static_assert(ATOMIC_INT_LOCK_FREE == 2, "atomic<int>/<float> must be lock-free");
static_assert(ATOMIC_CHAR_LOCK_FREE == 2, "atomic<DitherType> must be lock-free");

namespace {

// Bit depth only exists in whole numbers — there is no such thing as 11.4 bits
// — so the knob position is rounded to an integer instead of interpolated.
// Turning the knob up gives *fewer* bits, i.e. a more obvious effect.
int knobToBitDepth(float knob, int maxBits, int minBits) {
    const float span = static_cast<float>(maxBits - minBits);
    return maxBits - static_cast<int>(std::lround(knob * span));
}

dsp::DitherType switchToDitherType(int32_t position) {
    switch (position) {
        case 0:  return dsp::DitherType::Triangular;   // UP    - TPDF
        case 1:  return dsp::DitherType::Rectangular;  // MID   - RPDF
        default: return dsp::DitherType::None;         // DOWN  - no dither
    }
}

} // namespace

EffectProcessor::EffectProcessor(float sampleRate) {
    (void)sampleRate; // unused: see the note on the declaration
}

void EffectProcessor::setKnob(uint32_t index, float value) {
    switch (index) {
        case 0:
            volume_.store(value, std::memory_order_relaxed);
            break;
        case 1:
            bitDepth_.store(knobToBitDepth(value, kMaxBitDepth, kMinBitDepth),
                            std::memory_order_relaxed);
            break;
        default:
            break; // knobs 3..6 are unused by this effect
    }
}

void EffectProcessor::setSwitch(uint32_t index, int32_t position) {
    if (index == 0) {
        ditherType_.store(switchToDitherType(position), std::memory_order_relaxed);
    }
}

void EffectProcessor::setDitherEnabled(bool enabled) {
    ditherEnabled_.store(enabled, std::memory_order_relaxed);
}

float EffectProcessor::processSample(float input) {
    const int   bitDepth = bitDepth_.load(std::memory_order_relaxed);
    const float volume   = volume_.load(std::memory_order_relaxed);

    // The footswitch gates the toggle: dither is applied only when the switch is
    // engaged and the toggle has actually selected a distribution.
    const dsp::DitherType type =
        ditherEnabled_.load(std::memory_order_relaxed)
            ? ditherType_.load(std::memory_order_relaxed)
            : dsp::DitherType::None;

    // 1. Dither goes in BEFORE the quantizer. Added afterwards it would just be
    //    noise sitting on top of the distortion instead of a cure for it.
    const float step     = dsp::lsbStep(bitDepth);
    const float dithered = input + dither_.process(type, step);

    // 2. Quantize — the bit-depth reduction itself.
    const float quantized = dsp::quantize(dithered, bitDepth);

    // 3. Volume comes LAST. Scaling before the quantizer would change how big
    //    the signal is relative to 1 LSB, which silently changes the very thing
    //    this effect is meant to demonstrate.
    return quantized * volume;
}
