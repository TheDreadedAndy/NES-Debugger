/*
 * TND calculation with fast math enabled.
 */

#include <cstdlib>

[[gnu::pure]] float get_tnd_normal(size_t t, size_t n, size_t d) {
  float x = 0.0121551f * static_cast<float>(t)
          + 0.00816927f * static_cast<float>(n)
          + 0.00441735f * static_cast<float>(d);
  return (1.5979f * x) / (1.0f + x);
}

[[gnu::pure]] float get_tnd_lin_approx(size_t t, size_t n, size_t d) {
  float x = 0.00851f * static_cast<float>(t)
          + 0.00494f * static_cast<float>(n)
          + 0.00335f * static_cast<float>(d);
  return x;
}
