#include "dsp_primitives.h"
#include <cmath>

float dsp::lsbStep(int bitDepth) {
    // 16 bits -> 65535 intervals across [-1, 1], so one step is 2/65535 wide.
    // Fewer bits means a coarser grid and a bigger step.
    return 2.0f / static_cast<float>((1 << bitDepth) - 1);
}

float dsp::quantize(float x, int bitDepth) {
    const float step = lsbStep(bitDepth);

    // Snap to the nearest step. Note how deterministic this is: the same input
    // always lands on the same code. That is precisely why the error ends up
    // correlated with the signal and is heard as harmonic distortion rather
    // than as noise.
    const float snapped = std::round(x / step) * step;

    // Dither can push a near-full-scale sample past the rails, so clamp.
    return std::fmin(std::fmax(snapped, -1.0f), 1.0f);
}

dsp::NoiseSource::NoiseSource(uint32_t seed)
    // xorshift32 collapses to all-zeros forever if it is ever seeded with 0.
    : state_(seed == 0u ? 0x1u : seed) {}

float dsp::NoiseSource::nextUniform() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    // Scale the full 32-bit range onto [0, 1)  (the constant is 2^-32).
    return static_cast<float>(state_) * 2.3283064e-10f;
}

float dsp::DitherGenerator::process(DitherType type, float step) {
    switch (type) {
        case DitherType::Rectangular:
            // RPDF: a single uniform noise spanning +-0.5 LSB. It decorrelates
            // the error, so the distortion goes away -- but the level of the
            // noise floor still rises and falls with the signal, which is the
            // "breathing" ("respirar") you hear on quiet passages.
            return (noise_.nextUniform() - 0.5f) * step;

        case DitherType::Triangular:
            // TPDF: two independent uniform noises summed, spanning +-1 LSB.
            // Subtracting one draw from another is the same thing as summing
            // two uniform noises, and gives the triangular distribution.
            //
            // This removes the distortion AND the noise modulation, at the cost
            // of ~3 dB more noise than RPDF. It is the industry standard and
            // what virtually every DAW uses by default.
            //
            // Successive xorshift32 outputs are uncorrelated, so two draws from
            // one stream are independent enough to count as two noise sources.
            return (noise_.nextUniform() - noise_.nextUniform()) * step;

        case DitherType::None:
        default:
            return 0.0f;
    }
}
