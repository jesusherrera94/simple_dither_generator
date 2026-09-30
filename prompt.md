# Simple Dither Generator

## Description

A Hothouse pedal effect that implements **triangular (TPDF) dither** applied before bit-depth
reduction, built as the worked code example for the blog post in `dithering.md`.

The audience is beginners. The priority is *readable* code that maps one-to-one onto the concepts
in the post — not a production-grade bitcrusher. Where there's a tension between "clever" and
"obvious", choose obvious.

## Why the design deviates from the post

The post explains dither at 16-bit, which is the real-world case. On a guitar pedal that is
**inaudible** — 16-bit quantization distortion sits roughly 96 dB down, so a faithful 16-bit
implementation would produce a pedal that sounds like a passthrough.

So the effect makes bit depth a **control** rather than a constant. Parked at 16 bits it is the
honest, correct case; swept down to 4 bits the listener can actually hear quantization distortion
appear, and then hear it dissolve into a steady noise floor when dither is switched in. This is the
whole pedagogical point: *dither trades correlated distortion for uncorrelated noise*, and you
should be able to hear that trade happen.

## Signal chain

Order matters and is part of the lesson:

```
input → [+ dither] → [quantize to N bits] → [output volume] → out
```

- Dither is added **before** quantization. Added after, it would just be noise.
- Output volume is the **last** stage. Applied earlier it would scale the signal relative to the
  LSB and silently change the effect being demonstrated.

## Controls

| Control | Function |
|---|---|
| `KNOB_1` | Output volume, 0.0 … 1.0 (unity at max) |
| `KNOB_2` | Target bit depth, mapped to integers 16 … 4 |
| `TOGGLESWITCH_1` | Dither type — UP = TPDF, MID = RPDF, DOWN = none |
| `FOOTSWITCH_1` | Dither enable — stompable A/B |
| `FOOTSWITCH_2` | Bypass (as already scaffolded) |
| `LED_1` | Lit when dither is active |
| `LED_2` | Bypass state (as already scaffolded) |

Two resolved conflicts, recorded so they don't get "fixed" back:

1. **Footswitch 1 vs. the toggle's `none` position.** These would be redundant if both simply meant
   "no dither". Instead they compose: dither is applied only when the footswitch is enabled **and**
   the toggle is not on `none`. The toggle picks *which* dither; the footswitch is the fast A/B you
   can stomp mid-phrase.
2. **Footswitch 1 vs. DFU.** `hw.CheckResetToBootloader()` uses `FOOTSWITCH_1` held for 2 s. The
   dither toggle fires on `RisingEdge()` (a tap), so entering DFU will also flip the dither state
   once. Harmless, and this dual use is the normal Hothouse convention — leave it.

## Implementation requirements

### `dsp_primitives.{h,cpp}` — the reusable pieces

- **A small, self-contained PRNG** (xorshift32 or an LCG) producing a float in `[0, 1)`.
  `std::rand()` is not acceptable: it is neither real-time safe nor thread-safe, and this runs in
  the audio callback.
- **`quantize(x, bits)`** — step size `2.0f / ((1 << bits) - 1)` for a signal in `[-1, 1]`, then
  `std::round(x / step) * step`, then clamp back into `[-1, 1]` (dither can push a full-scale
  sample past the rails).
- **A dither generator** with an enum for the type:
  - `RPDF` = `(rand01() - 0.5f) * step` → ±0.5 LSB, uniform. Kills distortion but leaves noise
    modulation — the "respirar" the post describes.
  - `TPDF` = `(rand01() - rand01()) * step` → ±1 LSB, triangular. Two independent uniform noises
    summed. Kills distortion *and* noise modulation. This is the industry standard and the
    headline of the post.

Gaussian dither and noise shaping are **out of scope for the code.** They are covered in
`implementation.md` as further reading only — noise shaping in particular drags in error feedback,
filter state and stability, which works against the beginner framing.

### `effect_processor.{h,cpp}` — the platform-agnostic core

- Replace the passthrough in `processSample()` with the chain above.
- Keep the existing generic `setKnob` / `setSwitch` signatures: both adapters call them. Add a
  `setDitherEnabled(bool)` for the footswitch.
- Read control atomics with `memory_order_relaxed`. No allocation, locks, or blocking in
  `processSample()`.
- Construct the PRNG / dither generator as RAII members in the constructor, not with raw `new`.
- Must not include any Hothouse or Daisy header — this file is compiled by the host `clang++` for
  the `dylib` target, where those headers don't exist.

Bit depth stepping between integers will click, and the volume knob may zipper. Both are acceptable
for a demo; do not add smoothing unless it stays trivially readable.

### Adapters

- `hothouse_adapter.cpp` — add the `FOOTSWITCH_1` rising-edge handler and the `LED_1` update.
- `hl_adapter.cpp` — `hl_set_footswitch` is currently a documented no-op. **Implement index 0** so
  the dither A/B works in the simulator too; leave bypass to HeadroomLab. Do not change the six
  exported symbol names or signatures — they must stay in sync with HeadroomLab's loader at
  `src/infrastructure/dylib_plugin.rs`.

## Reference material

- `/Users/jesusherrera/Documents/Parallels Projects/pedalEffects/HyperF` — a fully implemented
  effect following this exact scaffold pattern. Read it for how `EffectProcessor` and
  `dsp_primitives` are meant to be filled in.
- `/Users/jesusherrera/Documents/Parallels Projects/rustProjects/HeadroomLab` — the simulator host
  and the FFI contract on the Rust side.
- `dithering.md` — the blog post this accompanies. It is in Spanish; read it for terminology and
  narrative order so the code and docs use the same vocabulary.

## Deliverables

1. The implementation described above.
2. **`implementation.md`, written in Spanish**, to be appended to the blog post. It explains the
   implementation and follows the post's existing voice and section flow. Code comments and
   identifiers stay in English. It should explicitly address why bit depth is a knob here when the
   post talks about 16-bit.
3. **`prompt-refined.md`** — written *after* the implementation is finished, capturing what the
   prompt should have said with hindsight.

## Verification

There are no tests in this repo. Done means:

- `make dylib` compiles clean, and the resulting library loads and sounds correct in HeadroomLab
  (sweep bit depth down with dither off, hear the distortion; stomp dither on, hear it become noise).
- Firmware builds with `make HOTHOUSE_DIR=/path/to/HothouseExamples` — note the default
  `../../HothouseExamples` does not exist on this machine, so the variable must be passed.
