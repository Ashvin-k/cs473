#ifndef FRACTAL_MYFLPT_H
#define FRACTAL_MYFLPT_H

#include <stdint.h>

//! Custom Floating Point Format (Q-Float 8.24)
//! Bits 31..24 : Signed Exponent
//! Bits 23..0  : Signed Mantissa
typedef int32_t myflpt;

//! Colour type (5-bit red, 6-bit green, 5-bit blue)
typedef uint16_t rgb565;

//! Custom math operations for our floating point format
myflpt create_myflpt(int32_t man, int32_t exp);
myflpt myflpt_add(myflpt a, myflpt b);
myflpt myflpt_mul(myflpt a, myflpt b);
myflpt myflpt_neg(myflpt a);
int myflpt_gt(myflpt a, myflpt b);

//! \brief Pointer to fractal point calculation function
typedef uint16_t (*calc_frac_point_p)(myflpt cx, myflpt cy, uint16_t n_max);

uint16_t calc_mandelbrot_point_soft(myflpt cx, myflpt cy, uint16_t n_max);

//! Pointer to function mapping iteration to colour value
typedef rgb565 (*iter_to_colour_p)(uint16_t iter, uint16_t n_max);

rgb565 iter_to_bw(uint16_t iter, uint16_t n_max);
rgb565 iter_to_grayscale(uint16_t iter, uint16_t n_max);
rgb565 iter_to_colour(uint16_t iter, uint16_t n_max);
rgb565 iter_to_colour1(uint16_t iter, uint16_t n_max);

void draw_fractal(rgb565 *fbuf, int width, int height,
                  calc_frac_point_p cfp_p, iter_to_colour_p i2c_p,
                  myflpt cx_0, myflpt cy_0, myflpt delta, uint16_t n_max);

#endif // FRACTAL_MYFLPT_H