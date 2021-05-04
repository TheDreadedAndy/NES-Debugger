/*
 * Checks the speeds of several TND calculations.
 */

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <unistd.h>
#include "fast_math.h"
#include "tay_math.h"
#include "normal_math.h"
#include "finv_math.h"

typedef float tnd_t(size_t t, size_t n, size_t d);
void test_mixer(tnd_t *fn, const char *name);
float get_avg_latency(tnd_t *fn);
float get_latency(tnd_t *fn);
float get_avg_tp(tnd_t *fn);
float get_tp(tnd_t *fn);
void get_stats(tnd_t *fn, float &avg_err, float &max_err);
float get_err(float real, float approx);

int main() {
  float diff = get_avg_latency(get_tnd_normal);
  printf("----------------------------------\n");
  printf("Normal latency test took %fs\n", diff);
  diff = get_avg_tp(get_tnd_normal);
  printf("Normal throughput test took %fs\n", diff);
  printf("----------------------------------\n");

  test_mixer(get_tnd_tay, "Taylor");
  test_mixer(get_tnd_fast, "Fast Math");
  test_mixer(get_tnd_finv, "Fast Inverse");
  test_mixer(get_tnd_rcpss, "RCPSS");
  test_mixer(get_tnd_shifty, "Shifty");
  test_mixer(get_tnd_finv_shifty, "Fast Inverse Shifty");
  test_mixer(get_tnd_rcpss_shifty, "RCPSS Shifty V1");
  test_mixer(get_tnd_rcpss_shifty2, "RCPSS Shifty V2");
  test_mixer(get_tnd_rcpss_shifty3, "RCPSS Shifty V3");
  test_mixer(get_tnd_rcpss_intmult, "RCPSS INT MULT");
  test_mixer(get_tnd_finv_intmult, "Fast Inverse INT MULT");
  test_mixer(get_tnd_rough_intmult, "Rough Inverse INT MULT");
  test_mixer(get_tnd_lin_approx, "Linear Approximation");
  test_mixer(get_tnd_log_approx, "Log Approx INT MULT");
  test_mixer(get_tnd_shifty_log_approx, "Shift Log Approx");
  test_mixer(get_tnd_heavy_log_approx, "Floatless Log Approx");
  test_mixer(get_tnd_shifty_heavy_log_approx, "Multless Log Approx");

  return 0;
}

void test_mixer(tnd_t *fn, const char *name) {
  float diff, avg_err, max_err;
  diff = get_avg_latency(fn);
  printf("%s latency test took %fs\n", name, diff);
  diff = get_avg_tp(fn);
  printf("%s throughput test took %fs\n", name, diff);

  get_stats(fn, avg_err, max_err);
  printf("%s avg error: %f\n%s max error: %f\n", name, avg_err, name, max_err);
  printf("---------------------------------\n");
  return;
}

float get_avg_latency(tnd_t *fn) {
  float  diff = 0;
  for (size_t i = 0; i < 250; i++) { diff += get_latency(fn); }
  diff = diff / 250.0f;
  return diff;
}

float get_latency(tnd_t *fn) {
  size_t t, n, d;
  union { float f; uint32_t i; } conv = { .i = (uint32_t) random() };
  clock_t start = clock();
  for (size_t i = 0; i < 10000; i++) {
    t = (conv.i >> 8) & 0xF;
    n = (conv.i >> 12) & 0xF;
    d = (conv.i >> 16) & 0x7F;
    conv.f = fn(t, n, d);
  }
  clock_t end = clock();
  return (float)(end - start) / CLOCKS_PER_SEC;
}

float get_avg_tp(tnd_t *fn) {
  float diff = 0;
  for (size_t i = 0; i < 250; i++) { diff += get_tp(fn); }
  diff = diff / 250.0f;
  return diff;
}

float get_tp(tnd_t *fn) {
  size_t t, n, d;
  union { float f; uint32_t i; } conv = { .i = (uint32_t) random() };
  t = (conv.i >> 8) & 0xF;
  n = (conv.i >> 12) & 0xF;
  d = (conv.i >> 16) & 0x7F;
  clock_t start = clock();
  for (size_t i = 0; i < 10000; i++) {
    (void)fn(t, n, d);
  }
  clock_t end = clock();
  return (float)(end - start) / CLOCKS_PER_SEC;
}

void get_stats(tnd_t *fn, float &avg_err, float &max_err) {
  float real, approx, err;
  avg_err = 0;
  max_err = 0;
  for (size_t t = 0; t < 16; t++) {
    for (size_t n = 0; n < 16; n++) {
      for (size_t d = 0; d < 128; d++) {
        real = get_tnd_normal(t, n, d);
        if (real == 0) { continue; }
        approx = fn(t, n, d);
        err = get_err(real, approx);
        avg_err += err;
        if (err > max_err) { max_err = err; }
      }
    }
  }
  avg_err = avg_err / 32768.0f;
  return;
}

float get_err(float real, float approx) {
  float err = (real - approx) / real;
  return (err < 0) ? -err : err;
}
