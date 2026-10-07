#include "fractal_myflpt.h"
#include <swap.h>
#include <defs.h>

int ilog2(unsigned x);

#define MYFLPT_TWO  ((myflpt)0xEB400000)
#define MYFLPT_FOUR ((myflpt)0xEC400000)

//! \brief Absolute value of a 32-bit integer
__static_inline int32_t abs_32(int32_t val) {
    return (val < 0) ? -val : val;
}

//! \brief Normalize man * 2^exp into a myflpt
__static_inline myflpt fp_norm(int32_t man, int32_t exp) {
    uint32_t m;
    int32_t s;

    if (man == 0) return 0;

    m = (uint32_t)abs_32(man);
    s = ilog2(m) - (MYFLPT_MAN_BITS - 1);
    if (s > 0)      m >>= s;
    else if (s < 0) m <<= -s;
    exp += s;

    if (exp < MYFLPT_EXP_MIN) return 0;
    if (exp > MYFLPT_EXP_MAX) { exp = MYFLPT_EXP_MAX; m = (1u << MYFLPT_MAN_BITS) - 1; }

    return MYFLPT_PACK((man < 0) ? -(int32_t)m : (int32_t)m, exp);
}

//! \brief Negate a myflpt
__static_inline myflpt fp_neg(myflpt a) {
    return (a == 0) ? 0 : MYFLPT_PACK(-MYFLPT_MAN(a), MYFLPT_EXP(a));
}

//! \brief Add two myflpt
__static_inline myflpt fp_add(myflpt a, myflpt b) {
    int32_t ea, eb, ma, mb, d, sum;
    uint32_t u;

    if (a == 0) return b;
    if (b == 0) return a;

    ea = MYFLPT_EXP(a); eb = MYFLPT_EXP(b);
    ma = MYFLPT_MAN(a) << 7;
    mb = MYFLPT_MAN(b) << 7;

    d = ea - eb;
    if (d > 0) {
        if (d > 30) return a;
        mb >>= d;
    } else if (d < 0) {
        if (-d > 30) return b;
        ma >>= -d;
        ea = eb;
    }

    sum = ma + mb;
    if ((ma ^ mb) >= 0) {
        u = (uint32_t)abs_32(sum);
        if (u & 0x40000000u) { u >>= 8; ea += 1; }
        else                 { u >>= 7; }
        return MYFLPT_PACK((sum < 0) ? -(int32_t)u : (int32_t)u, ea);
    }
    return fp_norm(sum, ea - 7);
}

//! \brief Multiply two normalized magnitudes and renormalize the product
__static_inline uint32_t fp_mul_mag(uint32_t ua, uint32_t ub, int32_t *e) {
    uint32_t bh = ub >> 14, bl = ub & 0x3FFFu;
    uint32_t p = ua * bh + (((ua >> 9) * bl + (((ua & 0x1FFu) * bl) >> 9)) >> 5);

    if (p & 0x80000000u) { *e += 14 + 9; return p >> 9; }
    *e += 14 + 8;
    return p >> 8;
}

//! \brief Multiply two myflpt
__static_inline myflpt fp_mul(myflpt a, myflpt b) {
    int32_t ma, mb, e;
    uint32_t p;

    if (a == 0 || b == 0) return 0;

    ma = MYFLPT_MAN(a);
    mb = MYFLPT_MAN(b);
    e  = MYFLPT_EXP(a) + MYFLPT_EXP(b);
    p  = fp_mul_mag((uint32_t)abs_32(ma), (uint32_t)abs_32(mb), &e);

    if (e < MYFLPT_EXP_MIN) return 0;
    if (e > MYFLPT_EXP_MAX) { e = MYFLPT_EXP_MAX; p = (1u << MYFLPT_MAN_BITS) - 1; }

    return MYFLPT_PACK(((ma ^ mb) < 0) ? -(int32_t)p : (int32_t)p, e);
}

//! \brief Square a myflpt
__static_inline myflpt fp_sqr(myflpt a) {
    int32_t e;
    uint32_t ua, p;

    if (a == 0) return 0;

    ua = (uint32_t)abs_32(MYFLPT_MAN(a));
    e  = 2 * MYFLPT_EXP(a);
    p  = fp_mul_mag(ua, ua, &e);

    if (e < MYFLPT_EXP_MIN) return 0;
    if (e > MYFLPT_EXP_MAX) { e = MYFLPT_EXP_MAX; p = (1u << MYFLPT_MAN_BITS) - 1; }

    return MYFLPT_PACK((int32_t)p, e);
}

//! \brief Return 1 if a > b, 0 otherwise
__static_inline int fp_gt(myflpt a, myflpt b) {
    int32_t ma, mb, ea, eb;

    if (a == b) return 0;

    ma = MYFLPT_MAN(a);
    mb = MYFLPT_MAN(b);

    if (ma >= 0 && mb <  0) return 1;
    if (ma <  0 && mb >= 0) return 0;
    if (ma == 0) return 0;
    if (mb == 0) return 1;

    ea = MYFLPT_EXP(a);
    eb = MYFLPT_EXP(b);
    if (ea != eb) return (ma > 0) ? (ea > eb) : (ea < eb);
    return ma > mb;
}

//! \brief Create a myflpt equal to man * 2^exp
myflpt create_myflpt(int32_t man, int32_t exp) { return fp_norm(man, exp); }

//! \brief Add two myflpt
myflpt myflpt_add(myflpt a, myflpt b) { return fp_add(a, b); }

//! \brief Multiply two myflpt
myflpt myflpt_mul(myflpt a, myflpt b) { return fp_mul(a, b); }

//! \brief Negate a myflpt
myflpt myflpt_neg(myflpt a) { return fp_neg(a); }

//! \brief Return 1 if a > b, 0 otherwise
int myflpt_gt(myflpt a, myflpt b) { return fp_gt(a, b); }

//! \brief Count the Mandelbrot iterations of the point cx + i*cy
uint16_t calc_mandelbrot_point_soft(myflpt cx, myflpt cy, uint16_t n_max) {
  myflpt x = cx;
  myflpt y = cy;
  uint16_t n = 0;
  myflpt xx, yy, two_xy;

  do {
    myflpt abs_x = (MYFLPT_MAN(x) < 0) ? fp_neg(x) : x;
    myflpt abs_y = (MYFLPT_MAN(y) < 0) ? fp_neg(y) : y;
    if (fp_gt(abs_x, MYFLPT_TWO) || fp_gt(abs_y, MYFLPT_TWO)) {
        ++n;
        break;
    }

    xx = fp_sqr(x);
    yy = fp_sqr(y);

    two_xy = fp_mul(x, y);
    if (two_xy != 0) two_xy += (1 << 24);

    x = fp_add(fp_add(xx, fp_neg(yy)), cx);
    y = fp_add(two_xy, cy);
    ++n;
  } while (!fp_gt(fp_add(xx, yy), MYFLPT_FOUR) && (n < n_max));

  return n;
}

//! \brief Map number of performed iterations to black and white
rgb565 iter_to_bw(uint16_t iter, uint16_t n_max) {
  if (iter == n_max) {
    return 0x0000;
  }
  return 0xffff;
}

//! \brief Map number of performed iterations to grayscale
rgb565 iter_to_grayscale(uint16_t iter, uint16_t n_max) {
  if (iter == n_max) {
    return 0x0000;
  }
  uint16_t brightness = iter & 0xf;
  return swap_u16(((brightness << 12) | ((brightness << 7) | brightness<<1)));
}

//! \brief Calculate binary logarithm for unsigned integer argument x
int ilog2(unsigned x) {
  if (x == 0) return -1;
  int n = 1;
  if ((x >> 16) == 0) { n += 16; x <<= 16; }
  if ((x >> 24) == 0) { n += 8; x <<= 8; }
  if ((x >> 28) == 0) { n += 4; x <<= 4; }
  if ((x >> 30) == 0) { n += 2; x <<= 2; }
  n -= x >> 31;
  return 31 - n;
}

//! \brief Map number of performed iterations to a colour
rgb565 iter_to_colour(uint16_t iter, uint16_t n_max) {
  if (iter == n_max) {
    return 0x0000;
  }
  uint16_t brightness = (iter&1)<<4|0xF;
  uint16_t r = (iter & (1 << 3)) ? brightness : 0x0;
  uint16_t g = (iter & (1 << 2)) ? brightness : 0x0;
  uint16_t b = (iter & (1 << 1)) ? brightness : 0x0;
  return swap_u16(((r & 0x1f) << 11) | ((g & 0x1f) << 6) | ((b & 0x1f)));
}

//! \brief Map number of performed iterations to a colour
rgb565 iter_to_colour1(uint16_t iter, uint16_t n_max) {
  if (iter == n_max) {
    return 0x0000;
  }
  uint16_t brightness = ((iter&0x78)>>2)^0x1F;
  uint16_t r = (iter & (1 << 2)) ? brightness : 0x0;
  uint16_t g = (iter & (1 << 1)) ? brightness : 0x0;
  uint16_t b = (iter & (1 << 0)) ? brightness : 0x0;
  return swap_u16(((r & 0xf) << 12) | ((g & 0xf) << 7) | ((b & 0xf)<<1));
}

//! \brief Draw fractal into frame buffer
void draw_fractal(rgb565 *fbuf, int width, int height,
                  calc_frac_point_p cfp_p, iter_to_colour_p i2c_p,
                  myflpt cx_0, myflpt cy_0, myflpt delta, uint16_t n_max) {
  rgb565 *pixel = fbuf;
  myflpt cy = cy_0;
  for (int k = 0; k < height; ++k) {
    myflpt cx = cx_0;
    for(int i = 0; i < width; ++i) {
      uint16_t n_iter = (*cfp_p)(cx, cy, n_max);
      rgb565 colour = (*i2c_p)(n_iter, n_max);
      *(pixel++) = colour;
      cx = fp_add(cx, delta);
    }
    cy = fp_add(cy, delta);
  }
}