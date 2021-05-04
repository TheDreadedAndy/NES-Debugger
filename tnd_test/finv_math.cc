/*
 * TND calculation with fast math enabled.
 */

#include <cstdlib>
#include <cstdint>
#include <cstdio>

[[gnu::pure]] float fast_inv(float x);
[[gnu::pure]] float rough_inv(float x);
[[gnu::pure]] float rcpss(float x);
[[gnu::pure]] unsigned msb_index(unsigned n);

[[gnu::pure]] float get_tnd_finv(size_t t, size_t n, size_t d) {
  float x = 0.0121551f * static_cast<float>(t)
          + 0.00816927f * static_cast<float>(n)
          + 0.00441735f * static_cast<float>(d);
  return (1.5979f * x) * fast_inv(1.0f + x);
}

[[gnu::pure]] float get_tnd_rcpss(size_t t, size_t n, size_t d) {
  float x = 0.0121551f * static_cast<float>(t)
          + 0.00816927f * static_cast<float>(n)
          + 0.00441735f * static_cast<float>(d);
  return (1.5979f * x) * rcpss(1.0f + x);
}

/* Cursed code */
[[gnu::pure]] float get_tnd_shifty(size_t t, size_t n, size_t d) {
  uint32_t mantissa = ((n + t) << 16) + ((t + d) << 15) + (d << 13) + (t << 12);
  union { float f; uint32_t i; } x = { .i = (mantissa | 0x3F800000U) };
  return (1.5979f * (x.f - 1.0f)) / (x.f);
}

/* VERY cursed code */
[[gnu::pure]] float get_tnd_finv_shifty(size_t t, size_t n, size_t d) {
  uint32_t mantissa = ((n + t) << 16) + ((t + d) << 15) + (d << 13) + (t << 12);
  union { float f; uint32_t i; } x = { .i = (mantissa | 0x3F800000U) };
  return (1.5979f * (x.f - 1.0f)) * fast_inv(x.f);
}

/* EXTREMELY cursed code */
[[gnu::pure]] float get_tnd_rcpss_shifty(size_t t, size_t n, size_t d) {
  uint32_t mantissa = ((n + t) << 16) + ((t + d) << 15) + (d << 13) + (t << 12);
  union { float f; uint32_t i; } x = { .i = (mantissa | 0x3F800000U) };
  return (1.5979f * (x.f - 1.0f)) * rcpss(x.f);
}

/* UNSPEAKABLY cursed code */
[[gnu::pure]] float get_tnd_rcpss_shifty2(size_t t, size_t n, size_t d) {
  uint32_t mantissa = ((n + t) << 16) + ((t + d) << 15) + ((n + t + d) << 13) + (t << 12);
  union { float f; uint32_t i; } x = { .i = (mantissa + 0x3F8CCCCDU) };
  return (1.5979f * (x.f - 1.1f)) * rcpss(x.f);
}

/* UNIMAGINABLY cursed code */
[[gnu::pure]] float get_tnd_rcpss_shifty3(size_t t, size_t n, size_t d) {
  //uint32_t mantissa = (t * 55296) + (n * 36864) + (d * 20480);
  uint32_t mantissa = (t * 88474) + (n * 58982) + (d * 32768);
  union { float f; uint32_t i; } x, y;
  x.i = 0x40000000U | ((mantissa >> 1) + (mantissa >> 3));
  x.f -= 0.9f;
  y.i = 0x40000000U | mantissa;
  y.f -= 2.0f;
  return y.f * rcpss(x.f);
}

/* Honestly, this is less cursed than the last one */
[[gnu::pure]] float get_tnd_rcpss_intmult(size_t t, size_t n, size_t d) {
  uint32_t mantissa = (t * 0x18e4c) + (n * 0x10bb0) + (d * 0x90bf);
  union { float f; uint32_t i; } x = { .i = (0x3F800000U | mantissa) };
  return 1.5979f * (x.f - 1.0f) * rcpss(x.f);
}

/* Dude, same */
[[gnu::pure]] float get_tnd_finv_intmult(size_t t, size_t n, size_t d) {
  uint32_t mantissa = (t * 0x18e4c) + (n * 0x10bb0) + (d * 0x90bf);
  union { float f; uint32_t i; } x = { .i = (0x3F800000U | mantissa) };
  return 1.5979f * (x.f - 1.0f) * fast_inv(x.f);
}

/* Dude, same */
[[gnu::pure]] float get_tnd_rough_intmult(size_t t, size_t n, size_t d) {
  uint32_t mantissa = (t * 0x18e4c) + (n * 0x10bb0) + (d * 0x90bf);
  union { float f; uint32_t i; } x = { .i = (0x3F800000U | mantissa) };
  return 1.5979f * (x.f - 1.0f) * rough_inv(x.f);
}

/* Forgive me */
[[gnu::pure]] float get_tnd_log_approx(size_t t, size_t n, size_t d) {
  uint32_t mantissa = (t * 0x18e4c) + (n * 0x10bb0) + (d * 0x90bf);

  union { float f; uint32_t i; } x, xp, c, y;
  xp.i = (0x3F800000U | mantissa);
  x.f = xp.f - 1.0f;
  c.f = 1.5979f;
  y.i = c.i + x.i - xp.i;

  return y.f;
}

/* Forgive me */
[[gnu::pure]] float get_tnd_shifty_log_approx(size_t t, size_t n, size_t d) {
  uint32_t mantissa = (t << 17) + (n << 16) + (d << 15);

  union { float f; uint32_t i; } x, xp, c, y;
  xp.i = (0x3F800000U | mantissa);
  x.f = xp.f - 1.0f;
  c.f = 1.5979f;
  y.i = c.i + x.i - xp.i;

  return y.f;
}

/* There are no limits */
[[gnu::pure]] float get_tnd_heavy_log_approx(size_t t, size_t n, size_t d) {
  const uint32_t c = 0x3fcc87fdu; /* 1.5979f */
  const uint32_t one_i = 0x3f800000u; /* 1.0f */
  const uint32_t float_bias = 127;
  const uint32_t exp_shift = 23;

  uint32_t mantissa = (t * 0x18e4c) + (n * 0x10bb0) + (d * 0x90bf);

  uint32_t xp = one_i | mantissa;
  uint32_t mantissa_shift = exp_shift - msb_index(mantissa);
  uint32_t x = ((float_bias - mantissa_shift - 1) << exp_shift) + (mantissa << mantissa_shift);

  union { float f; uint32_t i; } y;
  y.i = c + x - xp;

  return y.f;
}

/* Lmao whats a multiplier */
[[gnu::pure]] float get_tnd_shifty_heavy_log_approx(size_t t, size_t n, size_t d) {
  const uint32_t c = 0x3fcc87fdu; /* 1.5979f */
  const uint32_t one_i = 0x3f800000u; /* 1.0f */
  const uint32_t float_bias = 127;
  const uint32_t exp_shift = 23;

  uint32_t mantissa = (t << 17) + (n << 16) + (d << 15);

  uint32_t xp = one_i | mantissa;
  uint32_t mantissa_shift = exp_shift - msb_index(mantissa);
  uint32_t x = ((float_bias - mantissa_shift - 1) << exp_shift) + (mantissa << mantissa_shift);

  union { float f; uint32_t i; } y;
  y.i = c + x - xp;

  return y.f;
}

/* ASM shortcut */
[[gnu::pure]] float rcpss(float x) {
  asm ("rcpss %1, %0" : "=x" (x) : "x" (x));
  return x;
}

/* ASM shortcut */
[[gnu::pure]] unsigned msb_index(unsigned n) {
  asm ("bsr %1, %0" : "=X" (n) : "X" (n));
  return n;
}

/*
 * Modification of the fast inverse square root function.
 *
 * Gets an approximation of the inverse of x.
 */
[[gnu::pure]] float fast_inv(float x) {
  union { float f; uint32_t i; } hack = { .f = x };
  hack.i = 0x7EF4FB9D - hack.i;
  return hack.f * (2.0f - hack.f * x);
}

/*
 * Fast inverse without a newton approximation.
 */
[[gnu::pure]] float rough_inv(float x) {
  union { float f; uint32_t i; } hack = { .f = x };
  hack.i = 0x7EF4FB9D - hack.i;
  return hack.f;
}
