#include "mixer/Resampler.h"

namespace dalia {

	static inline float HermiteInterpolate(float frac, float xm1, float x0, float x1, float x2) {
       float c0 = x0;
       float c1 = 0.5f * (x1 - xm1);
       float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
       float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
       return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    void ProcessResampler(
    	ResamplerState& state,
       const float* DALIA_RESTRICT sourceData,
       uint32_t maxSourceFrames,
       uint32_t channels,
       float* DALIA_RESTRICT outBuffer,
       uint32_t maxOutputFrames,
       float phaseIncrement,
       uint32_t& outFramesGenerated,
       uint32_t& outFramesConsumed) {

       outFramesGenerated = 0;
       outFramesConsumed = 0;
       float frac = state.fractionalPhase;

       while (outFramesGenerated < maxOutputFrames && outFramesConsumed < maxSourceFrames) {
          for (uint32_t c = 0; c < channels; c++) {
             float xm1, x0, x1, x2;

             // Past (-1)
             // Pull from history if we are at the very start of the fetched block
             if (outFramesConsumed == 0) xm1 = state.history[c];
             else xm1 = sourceData[(outFramesConsumed - 1) * channels + c];

             // Current (0)
             x0 = sourceData[outFramesConsumed * channels + c];

             // Lookahead (+1)
             // Clamp to the last valid sample if we hit the extreme edge of the block
             if (outFramesConsumed + 1 < maxSourceFrames) x1 = sourceData[(outFramesConsumed + 1) * channels + c];
             else x1 = x0;

             // Lookahead (+2)
             if (outFramesConsumed + 2 < maxSourceFrames) x2 = sourceData[(outFramesConsumed + 2) * channels + c];
             else x2 = x1;

             outBuffer[outFramesGenerated * channels + c] = HermiteInterpolate(frac, xm1, x0, x1, x2);
          }

          outFramesGenerated++;
          frac += phaseIncrement;

          while (frac >= 1.0f) {
             frac -= 1.0f;
             outFramesConsumed++;
          }
       }

       state.fractionalPhase = frac;

       // Save the very last consumed sample for the next block's 'Past' (xm1)
       if (outFramesConsumed > 0) {
          for (uint32_t c = 0; c < channels; c++) {
             state.history[c] = sourceData[(outFramesConsumed - 1) * channels + c];
          }
       }
    }

}
