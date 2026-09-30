#include "fractal_myflpt.h"
#include <swap.h>

//! \brief Secure absolute value for 32-bit integers
static inline int32_t abs_32(int32_t val) {
    return (val < 0) ? -val : val;
}

//! \brief Create and normalize our custom floating point
myflpt create_myflpt(int32_t man, int32_t exp) {
    if (man == 0) return 0;
    
    int32_t abs_m = abs_32(man);
    
    // Prevent overflow: Shift down if mantissa exceeds 23-bit magnitude
    while (abs_m > 0x7FFFFF) {
        man /= 2;
        exp += 1;
        abs_m = abs_32(man);
    }
    
    // Maximize precision: Shift up to fill 23 bits
    while (abs_m > 0 && abs_m <= 0x3FFFFF) {
        man *= 2;
        exp -= 1;
        abs_m = abs_32(man);
    }
    
    return (exp << 24) | (man & 0x00FFFFFF);
}

//! \brief Add two custom floats
myflpt myflpt_add(myflpt a, myflpt b) {
    if (a == 0) return b;
    if (b == 0) return a;
    
    int32_t ea = a >> 24;
    int32_t eb = b >> 24;
    int32_t ma = (a << 8) >> 8;
    int32_t mb = (b << 8) >> 8;
    
    // Align exponents
    if (ea > eb) {
        int shift = ea - eb;
        if (shift >= 31) return a;
        mb >>= shift; 
        eb = ea;
    } else if (eb > ea) {
        int shift = eb - ea;
        if (shift >= 31) return b;
        ma >>= shift;
        ea = eb;
    }
    
    return create_myflpt(ma + mb, ea);
}

//! \brief Negate a custom float
myflpt myflpt_neg(myflpt a) {
    if (a == 0) return 0;
    int32_t exp = a >> 24;
    int32_t man = (a << 8) >> 8;
    return (exp << 24) | ((-man) & 0x00FFFFFF);
}

//! \brief Multiply two custom floats
myflpt myflpt_mul(myflpt a, myflpt b) {
    if (a == 0 || b == 0) return 0;
    
    int32_t ea = a >> 24;
    int32_t eb = b >> 24;
    int32_t ma = (a << 8) >> 8;
    int32_t mb = (b << 8) >> 8;
    
    // Shift mantissas down before multiplication to avoid 32-bit overflow
    int32_t ma_shr = ma / 256;
    int32_t mb_shr = mb / 256;
    
    return create_myflpt(ma_shr * mb_shr, ea + eb + 16);
}

//! \brief Check if custom float a > b
int myflpt_gt(myflpt a, myflpt b) {
    myflpt diff = myflpt_add(a, myflpt_neg(b));
    int32_t man = (diff << 8) >> 8;
    return man > 0;
}

//! \brief  Mandelbrot fractal point calculation function
uint16_t calc_mandelbrot_point_soft(myflpt cx, myflpt cy, uint16_t n_max) {
  myflpt x = cx;
  myflpt y = cy;
  uint16_t n = 0;
  myflpt xx, yy, two_xy;
  
  myflpt limit = create_myflpt(4, 0);  // 4.0
  myflpt escape = create_myflpt(2, 0); // 2.0

  do {
    // Fast escape to avoid overflow
    myflpt abs_x = (((x << 8) >> 8) < 0) ? myflpt_neg(x) : x;
    myflpt abs_y = (((y << 8) >> 8) < 0) ? myflpt_neg(y) : y;
    if (myflpt_gt(abs_x, escape) || myflpt_gt(abs_y, escape)) {
        break;
    }

    xx = myflpt_mul(x, x);
    yy = myflpt_mul(y, y);
    two_xy = myflpt_mul(x, y);
    two_xy = myflpt_add(two_xy, two_xy); // 2 * x * y

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