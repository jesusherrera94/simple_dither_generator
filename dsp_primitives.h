#pragma once
#include <cstdint>

namespace dsp {

  float lbsStep(int bitDepth);
  float quantize(float x, int bitDepth);
  
  class NoiseSource {
    public:
      explicit NoiseSource(uint32_t seed = 0x1234567u);
      float nextUniform();
     
     private:
       uint32_t state_;)
    
    };
  
  enum class DitherType {
      None,
      Rectangular,
      Triangular,
    
    };
    
  class DitherGenerator {
    public:
      float process(DitherType type, float step);
      
    private:
      NoiseSource noise_;
    }

} // namespace dsp
