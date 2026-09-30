#include "hothouse_adapter.h"

using clevelandmusicco::Hothouse;
using daisy::AudioHandle;

EffectProcessor* HothouseAdapter::audioDsp_ = nullptr;
std::atomic<bool> HothouseAdapter::bypassed_{true};

HothouseAdapter::HothouseAdapter(Hothouse& hw, EffectProcessor& dsp)
    : hw_(hw), dsp_(dsp) {
    // Bind the instance for the static audio callback.
    audioDsp_ = &dsp;
    led_bypass_.Init(hw.seed.GetPin(Hothouse::LED_2), false);
    led_dither_.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    dsp_.setDitherEnabled(ditherEnabled_);
}

void HothouseAdapter::updateControls() {
    // Refresh ADC + switch readings before mapping.
    hw_.ProcessAllControls();

    // Toggle bypass when FOOTSWITCH_2 is pressed.
    if (hw_.switches[Hothouse::FOOTSWITCH_2].RisingEdge()) {
        bypassed_.store(!bypassed_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
    }

    // Toggle the dither A/B when FOOTSWITCH_1 is tapped. The hardware switch is
    // momentary, so we latch it ourselves here.
    //
    // FOOTSWITCH_1 is also the DFU hold (see CheckResetToBootloader), which means
    // entering the bootloader will flip the dither state once on the way in.
    // Harmless, and this dual use is the normal Hothouse convention.
    if (hw_.switches[Hothouse::FOOTSWITCH_1].RisingEdge()) {
        ditherEnabled_ = !ditherEnabled_;
        dsp_.setDitherEnabled(ditherEnabled_);
    }

    // Only the controls this effect actually uses are mapped; the remaining
    // knobs and toggles are left unwired on purpose.
    dsp_.setKnob(0, hw_.GetKnobValue(Hothouse::KNOB_1));  // output volume
    dsp_.setKnob(1, hw_.GetKnobValue(Hothouse::KNOB_2));  // target bit depth

    // TOGGLESWITCH_1 picks the dither distribution (UP=TPDF, MID=RPDF, DOWN=off).
    // Its UP/MIDDLE/DOWN values are already 0/1/2, which is what setSwitch wants.
    dsp_.setSwitch(0, hw_.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_1));

    // Update LED state.
    led_bypass_.Set(bypassed_.load(std::memory_order_relaxed) ? 0.0f : 1.0f);
    led_bypass_.Update();
    led_dither_.Set(ditherEnabled_ ? 1.0f : 0.0f);
    led_dither_.Update();
}

void HothouseAdapter::AudioCallback(AudioHandle::InputBuffer in,
                                    AudioHandle::OutputBuffer out,
                                    size_t size) {
    const bool bypass = bypassed_.load(std::memory_order_relaxed);

    for (size_t i = 0; i < size; ++i) {
        if (bypass) {
            // Buffered bypass: dry signal straight through (mono -> dual mono).
            out[0][i] = out[1][i] = in[0][i];
        } else {
            float processed = audioDsp_->processSample(in[0][i]);
            out[0][i] = out[1][i] = processed;
        }
    }
}
