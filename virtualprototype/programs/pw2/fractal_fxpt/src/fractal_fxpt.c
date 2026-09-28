#include "fractal_fxpt.h"
#include <swap.h>

//! \brief Secure absolute value without external library calls
int32_t abs_32(int32_t val) {
    return (val < 0) ? -val : val;
}

//! \brief 100% native 32-bit multiplication, ultra-fast and safe
int32_t mul_q8_24(int32_t a, int32_t b) {
    int32_t abs_a = abs_32(a);
    int32_t abs_b = abs_32(b);
    
    // Safe bit-shift on positive numbers followed by hardware multiplication
    int32_t res = (abs_a >> 12) * (abs_b >> 12);
    
    // Restore the correct sign
    if ((a < 0 && b > 0) || (a > 0 && b < 0)) {
        return -res;
    }
    return res;
}

//! \brief  Mandelbrot fractal point calculation function
//! \param  cx    x-coordinate
//! \param  cy    y-coordinate
//! \param  n_max maximum number of iterations
//! \return       number of performed iterations at coordinate (cx, cy)
uint16_t calc_mandelbrot_point_soft(fxpt_8_24 cx, fxpt_8_24 cy, uint16_t n_max) {
  int32_t x = cx;
  int32_t y = cy;
  uint16_t n = 0;
  int32_t xx, yy, two_xy;
  
  // Mathematical limits in Q8.24 format: 4.0 and 2.0
  int32_t limit = 4 * 16777216; 
  int32_t escape = 2 * 16777216;

  do {
    // FAST ESCAPE SECURITY: Stop immediately to prevent any 32-bit overflow
    if (abs_32(x) > escape || abs_32(y) > escape) {
        break;
    }

    xx = mul_q8_24(x, x);
    yy = mul_q8_24(y, y);
    two_xy = mul_q8_24(x, y) * 2;

    x = xx - yy + cx;
    y = two_xy + cy;
    ++n;
  } while (((xx + yy) < limit) && (n < n_max));
  
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
                  fxpt_8_24 cx_0, fxpt_8_24 cy_0, fxpt_8_24 delta, uint16_t n_max) {
  rgb565 *pixel = fbuf;
  fxpt_8_24 cy = cy_0;
  for (int k = 0; k < height; ++k) {
    fxpt_8_24 cx = cx_0;
    for(int i = 0; i < width; ++i) {
      uint16_t n_iter = (*cfp_p)(cx, cy, n_max);
      rgb565 colour = (*i2c_p)(n_iter, n_max);
      *(pixel++) = colour;
      cx += delta;
    }
    cy += delta;
  }
}