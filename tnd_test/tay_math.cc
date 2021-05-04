/*
 * TND calculation with taylor series
 */

#include <cstdlib>

#define TND_TRIANGLE_COEF 0.000121551f
#define TND_NOISE_COEF 0.0000816927f
#define TND_DMC_COEF 0.0000441735f
#define TND_TAYLOR_CENTER 0.00432935f
#define TND_TAYLOR_TERM0 0.482776f
#define TND_TAYLOR_TERM1 77.821f
#define TND_TAYLOR_TERM2 (-5430.9f)
#define TND_TAYLOR_TERM3 379005.0f

[[gnu::pure]] float get_tnd_tay(size_t t, size_t n, size_t d) {
  // Use a taylor series to approximate the output of the triangle, noise
  // and DMC waves.
  float xtnd = (TND_TRIANGLE_COEF * static_cast<float>(t))
             + (TND_NOISE_COEF * static_cast<float>(n))
             + (TND_DMC_COEF * static_cast<float>(d))
             - TND_TAYLOR_CENTER;
  float tnd_output = TND_TAYLOR_TERM0 + (TND_TAYLOR_TERM1 * xtnd)
                   + (TND_TAYLOR_TERM2 * xtnd * xtnd)
                   + (TND_TAYLOR_TERM3 * xtnd * xtnd * xtnd);
  return tnd_output;
}
