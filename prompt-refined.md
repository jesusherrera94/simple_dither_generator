# Simple Dither Generator — refined prompt

What `prompt.md` should have said, written after the implementation was finished. The differences
below are not style preferences; each one cost a round trip, a build failure, or a wrong assumption.

## What the original got right

Worth keeping, because these were load-bearing:

- **Spelling out the signal chain and the reason for the order.** "Dither before the quantizer,
  volume last" with the *why* attached meant there was nothing to guess and nothing to get subtly
  wrong. This is the single highest-value paragraph in the prompt.
- **Banning `std::rand()` explicitly, with the reason.** Naming the trap is what prevents it.
- **Recording the two resolved control conflicts** (footswitch vs. toggle, footswitch vs. DFU) and
  saying "so they don't get 'fixed' back". Decisions without rationale get re-litigated.
- **Justifying the deviation from the post.** "16-bit dither is inaudible on a pedal, so bit depth is
  a knob" turned an apparent inaccuracy into a documented design choice.

## What it got wrong

### 1. The dependency paths were wrong

The prompt said `HothouseExamples` did not exist on this machine and that only `make dylib` could be
verified. It is actually at `~/development/HothouseExamples`, and the firmware builds fine.

Getting this wrong nearly meant shipping the firmware half unverified — and the firmware build is
where the one real portability bug showed up (see below).

**Say instead:** `HOTHOUSE_DIR=~/development/HothouseExamples`. Both targets must build before the
work is done; neither one alone is sufficient.

### 2. It never mentioned that the two targets use different C++ standards

This is the big one. The host `dylib` target is `-std=c++17`; libDaisy forces the firmware to
`-std=gnu++14`. A `static_assert` using `std::atomic<T>::is_always_lock_free` compiled clean on the
host and failed on ARM, because that trait is C++17.

libDaisy also builds with `-fshort-enums`, `-fno-exceptions` and `-fno-rtti`, none of which the host
build applies.

**Say instead:** *Anything in `effect_processor.*` or `dsp_primitives.*` must compile as both C++17
and gnu++14, with `-fshort-enums`, `-fno-exceptions` and `-fno-rtti`. The host build is the
permissive one — a clean `make dylib` proves nothing about the firmware.*

### 3. It didn't say what `hl_set_footswitch`'s `pressed` argument means

The prompt said to implement index 0 but not whether `pressed` is a momentary press or a latched
state. The answer only exists in HeadroomLab's Rust source: the simulator's footswitch widget is a
**latching toggle** that reports the new ON/OFF state, whereas the hardware switch is momentary and
has to be latched with `RisingEdge()`.

Guessing wrong here yields a dither that only engages while the mouse button is held.

**Say instead:** *HeadroomLab's footswitch latches and reports state — mirror `pressed` straight in.
The Hothouse footswitch is momentary — latch it yourself. The two adapters are asymmetric on
purpose.*

### 4. "Keep the existing generic setKnob/setSwitch signatures" was ambiguous

It was read as *keep the signatures* (correct) rather than *keep the generic `knobs_[6]` array*.
Replacing the arrays with named atomics (`volume_`, `bitDepth_`, `ditherType_`) and doing the mapping
in the setters is both more readable and better practice — mapping math belongs at control rate, not
in `processSample()`.

**Say instead:** *Keep the `setKnob`/`setSwitch` signatures so both adapters keep working, but
replace the generic arrays with named atomics and convert knob positions to meaningful units inside
the setters.*

### 5. It asked for contradictory scope

The prompt specified a three-position dither-type toggle including an RPDF position, and in the next
section said RPDF was out of scope. RPDF is one line and it is what makes the toggle worth having;
the intent was clearly to exclude Gaussian and noise shaping.

**Say instead:** *Implement `None`, `RPDF` and `TPDF`. Gaussian dither and noise shaping are out of
scope for the code and appear in `implementation.md` as further reading only.*

### 6. It said "no tests" and left verification to the ear

Technically true — there is no test framework here. But the two central claims of the post are
straightforwardly measurable, and measuring them caught nothing wrong yet proved the DSP correct in a
way that listening cannot:

- Mean output for a DC input swept across one quantization step: undithered output is a hard
  staircase (mean error `0.03175` at 4 bits), TPDF tracks the input (`0.00009`).
- Error variance at 0.0 vs 0.5 LSB: RPDF `0.000000` → `0.004444` (noise modulation), TPDF
  `0.004442` → `0.004444` (flat).

Those numbers went straight into `implementation.md` and are the most convincing part of it.

**Say instead:** *There is no test framework, but verify numerically with a throwaway host harness
before declaring done: (a) a DC sweep across one step showing dither linearizes the mean, (b) error
variance at two sub-LSB offsets showing RPDF modulates and TPDF does not. Put the resulting numbers
in `implementation.md`.*

### 7. It under-specified the doc

"Follows the post's voice" is not actionable enough. What actually mattered: the post is
conversational, uses second person, leans on the reader *hearing* things, and introduces each idea
before its formula. The doc also needed a table of controls, which the prompt never asked for.

**Say instead:** *Spanish, second person, conversational. Introduce each concept before its code.
Include the measured numbers, a control table, and an explicit "what was left out and why" section.
Code identifiers and comments stay in English.*

## Things nobody asked for but that turned out to matter

- **The `quantize` grid is centered on zero**, so silence is exactly representable — which is what you
  want in audio. The side effect is 15 levels at "4 bits" instead of 16, and a max of 0.933 instead
  of 1.0. Harmless, but a careful reader will notice, so `implementation.md` says so.
- **`sampleRate` is genuinely unused.** Dither and quantization are memoryless — no filters, no time
  constants. The scaffold's `sampleRate_` member produced a `-Wunused-private-field` warning; the
  honest fix is to drop the member, keep the constructor parameter for signature compatibility, and
  explain why in a comment.
- **Assert that the control atomics are lock-free.** A `std::atomic` that falls back to a mutex would
  put a lock in the audio path. Cheap to assert, invisible if you don't.

## The prompt this should have been

Keep `prompt.md`'s Description, "Why the design deviates", Signal chain, and Controls sections
verbatim. Replace Implementation requirements and Verification with the corrections above, and add a
short **Build environment** section stating up front:

```
HOTHOUSE_DIR = ~/development/HothouseExamples

make dylib                          # host simulator, -std=c++17
make HOTHOUSE_DIR=<path>            # firmware, -std=gnu++14 -fshort-enums -fno-exceptions -fno-rtti

Both must build warning-clean with -Wall before the work is done.
The shared core must satisfy the stricter of the two standards.
```
