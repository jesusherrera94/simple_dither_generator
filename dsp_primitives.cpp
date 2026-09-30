#include "dsp_primitives.h"
#include <cmath>


float dsp::lsbStep(int bitDepth) {
   
   return 2.0f / static_cast<float>((1 << bitDepth) - 1);
  
 }
 
 
 float dsp::quantize(float x, int bitDepth) {
   
   const float step = lsbStep(bitDepth);
   
   const float snapped = std::round(x / step) * step;
   
   return std::fmin(std::fmax(snapped, -1.0f), 1.0f); // <- clamp to avoid passing full range.  
   
 }
 
 dsp::NoiseSource::NoiseSource(uint32_t seed) : state_(seed == 0u ? 0x1u : seed) {
     
 }
 
 float dsp::NoiseSource::nextUniform() {
    
    state_ = state_ << 13;
    state_ = state_ << 17;
    state_ = state_ << 5;
    
    return static_cast<float>(state_) * 2.3283064e-10f;
   
}

float dsp::DitherGenerator::process(DitherType type, float step) {
   switch(type) {
     case DitherType::Rectangular:
          return (noise_.nextUniform() - 0.5f) * step;
         
      case DitherType::Triangular:
          return (noise_.nextUniform() - noise_.nextUniform()) * step;
          
      case DitherType::None: 
      default:
        return 0.0f;
     } 
  
 }
  