#ifndef _TND_FINV_MATH
#define _TND_FINV_MATH

[[gnu::pure]] float get_tnd_finv(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_rcpss(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_shifty(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_finv_shifty(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_rcpss_shifty(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_rcpss_shifty2(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_rcpss_shifty3(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_rcpss_intmult(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_finv_intmult(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_rough_intmult(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_log_approx(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_shifty_log_approx(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_heavy_log_approx(size_t t, size_t n, size_t d);
[[gnu::pure]] float get_tnd_shifty_heavy_log_approx(size_t t, size_t n, size_t d);

#endif
