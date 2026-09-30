#pragma once
#include <cstdint>

// Reusable, platform-agnostic DSP building blocks live in the `dsp` namespace.
//
// Keep this file free of Hothouse/Daisy headers: it must also build for the
// HeadroomLab simulator (the host `dylib` target), not just the firmware.
//
// Samples are handled in the normalized [-1, 1] range used by both Daisy and
// HeadroomLab, so "1 LSB" below always means one quantization step of that
// range at the current bit depth.
namespace dsp {

// Width of one quantization step ("1 LSB") for a `bitDepth`-bit grid.
// The grid divides [-1, 1] into (2^bitDepth - 1) equal intervals.
float lsbStep(int bitDepth);

// Rounds `x` onto the nearest point of a `bitDepth`-bit grid, clamped to
// [-1, 1]. This is the bit-depth reduction that dither exists to fix.
float quantize(float x, int bitDepth);

// White-noise source: xorshift32 (Marsaglia). Cheap, allocation-free and
// self-contained.
//
// Note we deliberately do NOT use std::rand(): it is neither real-time safe
// nor thread-safe, and this runs inside the audio callback.
class NoiseSource {
public:
    explicit NoiseSource(uint32_t seed = 0x1234567u);

    // Uniformly distributed in [0, 1).
    float nextUniform();

private:
    uint32_t state_;
};

// The shape of the noise floor we add, i.e. its probability density function.
enum class DitherType {
    None,         // no dither: the quantization error stays correlated
    Rectangular,  // RPDF, +-0.5 LSB
    Triangular,   // TPDF, +-1 LSB
};

// Produces the noise that gets added to a sample *before* it is quantized.
class DitherGenerator {
public:
    // Returns the offset to add to a sample, in amplitude units, for a grid
    // whose step width is `step` (see lsbStep).
    float process(DitherType type, float step);

private:
    NoiseSource noise_;
};

} // namespace dsp
