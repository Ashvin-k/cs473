#ifndef FRACTAL_MYFLPT_H
#define FRACTAL_MYFLPT_H

#include <stdint.h>

/*! Custom 32-bit floating point format (8-bit exponent, 24-bit mantissa).
 *
 *   31            24 23                                          0
 *  +----------------+---------------------------------------------+
 *  | exponent, 8 b  |  mantissa, 24 bits                          |
 *  | two's compl.   |  two's complement (sign + 23 magnitude)     |
 *  +----------------+---------------------------------------------+
 *
 *      value = mantissa * 2^exponent
 *
 *  A normalized mantissa satisfies 2^22 <= |mantissa| < 2^23, so every non-zero
 *  value carries 23 significant bits.  Zero is the all-zero word.
 *  Representable range: about 2^-106 <= |value| < 2^150.
 *
 *  Why 23 magnitude bits: the Mandelbrot iteration is chaotic near the border
 *  of the set, so rounding errors are amplified at every iteration and show up
 *  as wrong iteration counts.  Precision is what matters, so the mantissa gets
 *  as many bits as possible.
 *
 *  Why only 8 exponent bits: the range actually used is small.  The escape test
 *  keeps |x|, |y| <= 2 before each multiply, so no intermediate exceeds ~10.
 *  Small values only need to go well below the pixel step (3/512 ~ 2^-7.5).
 *
 *  Why a two's complement mantissa: once the exponents are aligned, addition is
 *  a plain integer add, with no magnitude comparison or sign recomputation.
 *
 *  Cost of the wide mantissa: the product of two magnitudes is 46 bits and no
 *  longer fits in a register.  myflpt_mul() splits it into three 32-bit partial
 *  products instead of calling the 64-bit software multiply.
 */
typedef int32_t myflpt;

//! Number of significant bits in a normalized mantissa (magnitude only)
#define MYFLPT_MAN_BITS 23

#define MYFLPT_EXP_MIN (-128)
#define MYFLPT_EXP_MAX 127

//! Sign-extended mantissa, bits 23..0
#define MYFLPT_MAN(a) (((int32_t)((uint32_t)(a) << 8)) >> 8)

//! Sign-extended exponent, bits 31..24
#define MYFLPT_EXP(a) ((int32_t)(a) >> 24)

//! Reassemble a word from a mantissa and an exponent
#define MYFLPT_PACK(m, e) \
    ((myflpt)(((uint32_t)(e) << 24) | ((uint32_t)(m) & 0x00FFFFFFu)))

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
