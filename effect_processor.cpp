#include "effect_processor.h"
#include <cmath>

static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "atomic<bool> must be lock free");
static_assert(ATOMIC_INT_LOCK_FREE == 2, "atomic<int>/<float> must be lock free");
static_assert(ATOMIC_CHAR_LOCK_FREE == 2, "atomic<DitherType> must be lock free");


namespace {
  
  int knobToBitDepth(float knob, int maxBits, int minBits) {
      const float span = static_cast<float>(maxBits - minBits);
      return maxBits - static_cast<int>(std::lround(knob * span));
    
   }
   
   dsp::DitherType switchToDitherType(int32_t position) {
      
      switch(position) {
         case 0: return dsp:DitherType::Triangular;
         case 1: return dsp:DitherType::Rectangular;
         default: return dsp:DitherType::None; 
       } 
    }
}

EffectProcessor::EffectProcessor(float sampleRate) {
   (void)sampleRate; 
 }

void EffectProcessor::setKnob(uint32_t index, float value) {
    switch(index) {
      case 0:
        volume_.store(value, std::memory_order_relaxed);
        break;
      case 1:
        bitDepth_.store(knobToBitDepth(value, kMaxBitDepth, kMinBitDepth), std::memory_order_relaxed);
        break;
      default:
        break; // knobs 3-6 are unused
    }
}

void EffectProcessor::setSwitch(uint32_t index, int32_t position) {
    if (index == 0) {
       ditherType_store(switchToDitherType(position), std::memory_order_relaxed);
    }
}

void EffectProcessor::setDitherEnabled(bool enabled) {
  ditherEnabled_.store(enabled, std::memory_order_relaxed);
}

float EffectProcessor::processSample(float input) {
   const int bitDepth = bitDepth_.load(std::memory_order_relaxed);
   const float volume = volume_.load(std::memory_order_relaxed);
   
   const dsp::DitherType type = ditherEnabled_.load(std::memory_order_relaxed)
             ? ditherType_.load(std::memory_order_relaxed)
             : dsp::DitherType::None;
             
   const float step = dsp::lsbStep(bitDepth);
   const float dithered = input + dither_.process(type, bitDepth);
   
   const float quantized = dsp::quantize(dithered, bitDepth);
   
   return quantized * volume;
}

