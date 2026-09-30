#include "fractal_myflpt.h"
#include <swap.h>

/* Provided by this file further down; used here to normalize in one step
   instead of looping.  Not modified. */
int ilog2(unsigned x);

//! \brief Absolute value of a myflpt (mantissa sign only)
static myflpt myflpt_abs(myflpt a) {
    return (MYFLPT_MAN(a) < 0) ? myflpt_neg(a) : a;
}

//! \brief Create and normalize our custom floating point
//!
//! ilog2() gives the position of the leading bit, so the shift needed to bring
//! the mantissa into [2^14, 2^15) is known in one step -- no loop.
//! The mantissa is normalized on its magnitude and the sign is reapplied
//! afterwards, which keeps |mantissa| strictly below 2^15 and therefore keeps
//! myflpt_neg() exact.
myflpt create_myflpt(int32_t man, int32_t exp) {
    int32_t neg, s;
    uint32_t m;

    if (man == 0) return 0;

    neg = (man < 0);
    m   = (uint32_t)(neg ? -man : man);

    s = ilog2(m) - (MYFLPT_MAN_BITS - 1);
    if (s > 0)      { m >>= s;  exp += s;  }
    else if (s < 0) { m <<= -s; exp -= -s; }

    return MYFLPT_PACK(neg ? -(int32_t)m : (int32_t)m, exp);
}

//! \brief Negate a custom float
myflpt myflpt_neg(myflpt a) {
    return (a == 0) ? 0 : MYFLPT_PACK(-MYFLPT_MAN(a), MYFLPT_EXP(a));
}

//! \brief Add two custom floats
//!
//! Past a 15-bit exponent gap the smaller operand cannot change the result, so
//! it is dropped rather than shifted away.  After alignment both mantissas are
//! below 2^15, so ma + mb cannot overflow.
myflpt myflpt_add(myflpt a, myflpt b) {
    int32_t ea, eb, ma, mb, d;

    if (a == 0) return b;
    if (b == 0) return a;

    ea = MYFLPT_EXP(a); eb = MYFLPT_EXP(b);
    ma = MYFLPT_MAN(a); mb = MYFLPT_MAN(b);

    d = ea - eb;
    if (d > 0) {
        if (d > MYFLPT_MAN_BITS) return a;
        mb >>= d;
    } else if (d < 0) {
        if (-d > MYFLPT_MAN_BITS) return b;
        ma >>= -d;
        ea = eb;
    }

    return create_myflpt(ma + mb, ea);
}

//! \brief Multiply two custom floats
//!
//! Both mantissas are in [2^14, 2^15), so the product is exact in an int32_t
//! (|p| <= 2^30) and lies in [2^28, 2^30): only two octaves.  The renormalizing
//! shift is therefore always 14 or 15, chosen by a single comparison, and
//! create_myflpt() is not needed at all.
myflpt myflpt_mul(myflpt a, myflpt b) {
    int32_t p, ap, e;

    if (a == 0 || b == 0) return 0;

    p = MYFLPT_MAN(a) * MYFLPT_MAN(b);
    e = MYFLPT_EXP(a) + MYFLPT_EXP(b);

    ap = (p < 0) ? -p : p;
    if (ap < (1 << 29)) { ap >>= (MYFLPT_MAN_BITS - 1); e += (MYFLPT_MAN_BITS - 1); }
    else                { ap >>= MYFLPT_MAN_BITS;       e += MYFLPT_MAN_BITS;       }

    return MYFLPT_PACK((p < 0) ? -ap : ap, e);
}

//! \brief Check if custom float a > b
//!
//! Compared field by field rather than by building a difference, which would
//! cost an addition and a full renormalization.
int myflpt_gt(myflpt a, myflpt b) {
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

//! \brief  Mandelbrot fractal point calculation function
uint16_t calc_mandelbrot_point_soft(myflpt cx, myflpt cy, uint16_t n_max) {
  myflpt x = cx;
  myflpt y = cy;
  uint16_t n = 0;
  myflpt xx = 0, yy = 0, two_xy;

  myflpt limit = create_myflpt(4, 0);  // 4.0
  myflpt escape = create_myflpt(2, 0); // 2.0

  do {
    // Fast escape to avoid overflow
    if (myflpt_gt(myflpt_abs(x), escape) || myflpt_gt(myflpt_abs(y), escape)) {
        break;
    }

    xx = myflpt_mul(x, x);
    yy = myflpt_mul(y, y);

    // 2 * x * y : the mantissa occupies bits 0..15, so adding 1<<16 increments
    // the exponent without ever carrying into the mantissa field.
    two_xy = myflpt_mul(x, y);
    if (two_xy != 0) two_xy += (1 << 16);

    x = myflpt_add(myflpt_add(xx, myflpt_neg(yy)), cx);
    y = myflpt_add(two_xy, cy);
    ++n;
  } while (!myflpt_gt(myflpt_add(xx, yy), limit) && (n < n_max));

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
      cx = myflpt_add(cx, delta);
    }
    cy = myflpt_add(cy, delta);
  }
}