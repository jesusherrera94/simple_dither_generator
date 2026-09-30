# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

A guitar-pedal effect for the **Hothouse** DIY DSP platform (Daisy Seed), scaffolded by **HeadroomLab**. The effect is a *triangular (TPDF) dither generator* applied before bit-depth reduction, written to be as simple as possible because it doubles as the worked example for a blog post (`dithering.md`, in Spanish; `implementation.md` is the Spanish companion piece).

Signal chain — the order is the lesson, don't rearrange it:

```
input → [+ dither] → [quantize to N bits] → [volume] → out
```

Dither must go in *before* the quantizer (after it, it's just noise on top of distortion), and volume must come *after* (scaling first changes the signal's size relative to 1 LSB, silently altering the effect).

Control map:

| Control | Function |
|---|---|
| `KNOB_1` | Output volume |
| `KNOB_2` | Target bit depth, 16 → 4 |
| `TOGGLESWITCH_1` | Dither type — UP=TPDF, MID=RPDF, DOWN=none |
| `FOOTSWITCH_1` | Dither enable (also the DFU hold — a tap flips dither, a 2 s hold enters the bootloader) |
| `FOOTSWITCH_2` | Bypass |
| `LED_1` / `LED_2` | Dither state / bypass state |

Bit depth is a knob rather than a fixed 16 because 16-bit dither is ~96 dB down and inaudible on a pedal. Parked at 16 it's the honest case; swept to 4 the effect is plainly audible. See `prompt.md` for the full rationale and `prompt-refined.md` for the hindsight notes.

The footswitch and toggle compose: dither applies only when the footswitch is engaged **and** the toggle isn't on `none`.

## Builds

Two independent targets share the same DSP core:

```bash
# Host shared library for the HeadroomLab simulator (no embedded toolchain needed)
make dylib          # -> build/libsimple_dither_generator.dylib
make clean-dylib

# Firmware for the Hothouse pedal (needs the ARM toolchain + Hothouse sources)
make HOTHOUSE_DIR=/path/to/HothouseExamples
make program-dfu HOTHOUSE_DIR=...   # flash over USB (hold FOOTSWITCH_1 2s to enter DFU)
```

On this machine the Hothouse tree lives at `~/development/HothouseExamples`, so use
`make HOTHOUSE_DIR=~/development/HothouseExamples` — the Makefile's `../../HothouseExamples` default does not resolve. The Makefile deliberately skips `include`-ing the libDaisy/Hothouse makefiles when the goal is `dylib`/`clean-dylib`, which is why the simulator build works with plain `clang++`.

**The two targets use different C++ standards, and this bites.** The host `dylib` is `-std=c++17`; libDaisy forces the firmware to `-std=gnu++14` plus `-fshort-enums -fno-exceptions -fno-rtti`. Anything in the shared core must compile as both — e.g. `std::atomic<T>::is_always_lock_free` is C++17 and fails on ARM; use the `ATOMIC_*_LOCK_FREE` macros instead. A clean `make dylib` proves nothing about the firmware, so **always build both.**

There are no tests, lint config, or CI. Verification is: both targets build warning-clean under `-Wall`, then load the dylib in HeadroomLab. For DSP changes, a throwaway host harness that links the three core `.cpp` files directly against the `hl_*` symbols is the fastest way to check behavior numerically.

## Architecture

The whole point of the layout is that the DSP never knows what platform it's on:

```
simple_dither_generator.cpp   firmware main(): owns Hothouse hw, EffectProcessor, HothouseAdapter
hothouse_adapter.{h,cpp}      ONLY file allowed to include hothouse.h / daisy headers
hl_adapter.cpp                ONLY file that exposes the HeadroomLab C FFI
effect_processor.{h,cpp}      platform-agnostic core: control state + processSample()
dsp_primitives.{h,cpp}        reusable `dsp::` building blocks (filters, noise, quantizers…)
```

Rules that follow from this and must be preserved:

- **`dsp_primitives.h` and `effect_processor.h` must not include any Hothouse/Daisy header** — they are compiled by the host `clang++` for the dylib target, where those headers don't exist.
- **Control → audio hand-off is lock-free.** `EffectProcessor` keeps *named* atomics (`volume_`, `bitDepth_`, `ditherType_`, `ditherEnabled_`), not a generic knob array. Knob positions are converted to meaningful units inside the setters — i.e. at control rate — so `processSample()` only loads ready-to-use values. Read with `memory_order_relaxed`; never allocate, lock, or block in `processSample()`. `static_assert`s in `effect_processor.cpp` enforce that the atomics really are lock-free.
- **Owned DSP objects are RAII members**, not raw `new` in the audio path. `dsp::DitherGenerator` holds only PRNG state, so it needs no `unique_ptr`.
- **No `std::rand()` in the audio path** — not real-time safe, not thread-safe. `dsp::NoiseSource` is a xorshift32; note it must never be seeded with 0 or it latches at 0 forever.
- Firmware is **mono in → dual mono out**; bypass is handled in `HothouseAdapter::AudioCallback` (buffered, dry passthrough). In the simulator, bypass is HeadroomLab's job.
- **The two adapters treat footswitches asymmetrically, on purpose.** HeadroomLab's footswitch is a *latching toggle* that reports the new ON/OFF state, so `hl_set_footswitch` mirrors `pressed` straight in. The Hothouse footswitch is *momentary*, so `HothouseAdapter` latches it itself via `RisingEdge()`.
- `EffectProcessor`'s constructor takes `sampleRate` for signature compatibility but **does not use it** — dither and quantization are memoryless. Don't re-add the member without something time-dependent to justify it (it triggers `-Wunused-private-field`).

### The FFI contract

`hl_adapter.cpp` exports exactly six `extern "C"` symbols — `hl_create`, `hl_destroy`, `hl_process`, `hl_set_knob`, `hl_set_switch`, `hl_set_footswitch`. These must stay in sync with HeadroomLab's loader at `src/infrastructure/dylib_plugin.rs` in `/Users/jesusherrera/Documents/Parallels Projects/rustProjects/HeadroomLab`. `hl_process` receives an **interleaved** buffer and currently ignores `channels`/`sample_rate`. Check the exports survived a change with `nm -gU build/libsimple_dither_generator.dylib | grep hl_`.

### Reference implementations

- `/Users/jesusherrera/Documents/Parallels Projects/pedalEffects/HyperF` — a fully implemented effect following this exact pattern; read it for how `EffectProcessor`/`dsp_primitives` are meant to be filled in.
- `/Users/jesusherrera/Documents/Parallels Projects/rustProjects/HeadroomLab` — the simulator host.

## Current state

Implemented and verified — both targets build warning-clean, and the DSP was checked numerically
(dither cuts mean sub-LSB error at 4 bits from `0.03175` to `0.00009`; RPDF error variance swings
`0.000000`→`0.004444` across a step while TPDF stays flat).

`dsp::` provides `lsbStep`, `quantize`, `NoiseSource` (xorshift32) and `DitherGenerator` with a
`DitherType` enum of `None`/`Rectangular`/`Triangular`.

Deliberately **not** implemented: Gaussian dither and noise shaping. Both are discussed in
`implementation.md` as further reading; noise shaping needs error feedback and filter state, which
works against the beginner framing. Don't add them without being asked.

One detail a careful reader will notice: `quantize`'s grid is centered on zero so that silence is
exactly representable, which means "4 bits" yields 15 levels rather than 16 and a maximum of `0.933`
rather than `1.0`. This is intended and documented in `implementation.md`.
