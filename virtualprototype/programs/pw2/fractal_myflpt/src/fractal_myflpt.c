#include "fractal_myflpt.h"
#include <swap.h>
#include <defs.h>

/* Provided by this file further down; used to normalize in one step. */
int ilog2(unsigned x);

//! Constants of the escape tests, normalized at compile time
#define MYFLPT_TWO  ((myflpt)0xEB400000)   //  2.0 = 0x400000 * 2^-21
#define MYFLPT_FOUR ((myflpt)0xEC400000)   //  4.0 = 0x400000 * 2^-20

//! \brief Secure absolute value for 32-bit integers
__static_inline int32_t abs_32(int32_t val) {
    return (val < 0) ? -val : val;
}

//! \brief Normalize man * 2^exp
//!
//! ilog2() gives the position of the leading bit, so the shift that brings the
//! magnitude into [2^22, 2^23) is known in one step -- no loop.  The magnitude
//! is normalized and the sign reapplied afterwards, so -mantissa always stays
//! representable.  Values below the range flush to zero, values above saturate.
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

//! \brief Negate: only the mantissa changes sign
__static_inline myflpt fp_neg(myflpt a) {
    return (a == 0) ? 0 : MYFLPT_PACK(-MYFLPT_MAN(a), MYFLPT_EXP(a));
}

//! \brief Add two custom floats
//!
//! Both mantissas are moved up by 7 guard bits before alignment (|m| < 2^30,
//! so the sum cannot overflow an int32_t), which keeps the bits of the smaller
//! operand that alignment would shift out.
//!
//! Same signs: the larger aligned operand is in [2^29, 2^30) and the smaller one
//! at most 2^29, so the leading bit of the sum is bit 29 or 30 -- one test, no
//! ilog2().  Only opposite signs (cancellation) need the general fp_norm().
__static_inline myflpt fp_add(myflpt a, myflpt b) {
    int32_t ea, eb, ma, mb, d, sum;
    uint32_t u;

    if (a == 0) return b;
    if (b == 0) return a;

    ea = MYFLPT_EXP(a); eb = MYFLPT_EXP(b);
    ma = MYFLPT_MAN(a) << 7;
    mb = MYFLPT_MAN(b) << 7;

    // Align exponents
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

//! \brief Product of two magnitudes in [2^22, 2^23), renormalized
//!
//! The exact product P = ua * ub is 46 bits wide.  Only floor(P / 2^14) is
//! needed, and it fits in 32 bits.  With ub = bh*2^14 + bl and ua = ah*2^9 + al
//! (bh < 2^9, bl < 2^14, ah < 2^14, al < 2^9), every partial product fits in a
//! uint32_t and
//!
//!     floor(P / 2^14) = ua*bh + ((ah*bl + ((al*bl) >> 9)) >> 5)
//!
//! exactly: three l.mul, no 64-bit software multiply.
//! P / 2^14 is in [2^30, 2^32): the renormalizing shift is 8 or 9, chosen by
//! one test on bit 31.  *e receives ea + eb on entry and the result on exit.
__static_inline uint32_t fp_mul_mag(uint32_t ua, uint32_t ub, int32_t *e) {
    uint32_t bh = ub >> 14, bl = ub & 0x3FFFu;
    uint32_t p = ua * bh + (((ua >> 9) * bl + (((ua & 0x1FFu) * bl) >> 9)) >> 5);

    if (p & 0x80000000u) { *e += 14 + 9; return p >> 9; }
    *e += 14 + 8;
    return p >> 8;
}

//! \brief Multiply two custom floats
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

//! \brief Square: same as fp_mul(a, a), but the sign is known to be positive
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

//! \brief Check if custom float a > b
//!
//! Compared field by field: signs, then exponents, then mantissas.  Thanks to
//! normalization the exponent decides as soon as the signs agree, so no
//! subtraction and no renormalization is needed.
__static_inline int fp_gt(myflpt a, myflpt b) {
    int32_t ma, mb, ea, eb;

    if (a == b) return 0;

    ma = MYFLPT_MAN(a);
    mb = MYFLPT_MAN(b);

    if (ma >= 0 && mb <  0) return 1;
    if (ma <  0 && mb >= 0) return 0;
    if (ma == 0) return 0;               /* a is zero, so b is positive */
    if (mb == 0) return 1;               /* b is zero, so a is positive */

    ea = MYFLPT_EXP(a);
    eb = MYFLPT_EXP(b);
    if (ea != eb) return (ma > 0) ? (ea > eb) : (ea < eb);
    return ma > mb;
}

//! \brief Public entry points (see fractal_myflpt.h)
myflpt create_myflpt(int32_t man, int32_t exp) { return fp_norm(man, exp); }
myflpt myflpt_add(myflpt a, myflpt b)           { return fp_add(a, b); }
myflpt myflpt_mul(myflpt a, myflpt b)           { return fp_mul(a, b); }
myflpt myflpt_neg(myflpt a)                     { return fp_neg(a); }
int    myflpt_gt(myflpt a, myflpt b)            { return fp_gt(a, b); }

//! \brief  Mandelbrot fractal point calculation function
uint16_t calc_mandelbrot_point_soft(myflpt cx, myflpt cy, uint16_t n_max) {
  myflpt x = cx;
  myflpt y = cy;
  uint16_t n = 0;
  myflpt xx, yy, two_xy;

  do {
    // Fast escape: keeps every intermediate below ~10, far inside the range
    myflpt abs_x = (MYFLPT_MAN(x) < 0) ? fp_neg(x) : x;
    myflpt abs_y = (MYFLPT_MAN(y) < 0) ? fp_neg(y) : y;
    if (fp_gt(abs_x, MYFLPT_TWO) || fp_gt(abs_y, MYFLPT_TWO)) {
        ++n;
        break;
    }

    xx = fp_sqr(x);
    yy = fp_sqr(y);

    // 2 * x * y : the exponent occupies bits 31..24, so adding 1 << 24
    // increments it.  |x*y| <= 4 keeps the exponent far below 127.
    two_xy = fp_mul(x, y);
    if (two_xy != 0) two_xy += (1 << 24);

    x = fp_add(fp_add(xx, fp_neg(yy)), cx);
    y = fp_add(two_xy, cy);
    ++n;
  } while (!fp_gt(fp_add(xx, yy), MYFLPT_FOUR) && (n < n_max));

  return n;
}

//! \brief  Map number of performed iterations to black and white
rgb565 iter_to_bw(uint16_t iter, uint16_t n_max) {
  if (iter == n_max) {
    return 0x0000;
  }
  return 0xffff;
}

//! \brief  Map number of performed iterations to grayscale
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

//! \brief  Map number of performed iterations to a colour
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

//! \brief  Draw fractal into frame buffer
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