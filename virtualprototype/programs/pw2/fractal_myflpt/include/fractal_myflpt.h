#ifndef FRACTAL_MYFLPT_H
#define FRACTAL_MYFLPT_H

#include <stdint.h>

/*! Custom 32-bit floating point format, fields split on the half-word boundary.
 *
 *   31                    16 15                      0
 *  +------------------------+------------------------+
 *  |   exponent, 16 bits    |   mantissa, 16 bits    |
 *  |   two's complement     |  sign + 15 magnitude   |
 *  +------------------------+------------------------+
 *
 *      value = mantissa * 2^exponent
 *
 *  A normalized mantissa satisfies 2^14 <= |mantissa| < 2^15, so every non-zero
 *  value carries exactly 15 significant bits.  Zero is the all-zero word.
 *
 *  Why 15 magnitude bits: the product of two mantissas is then bounded by 2^30
 *  and fits in an int32_t, so myflpt_mul() needs a single l.mul with no
 *  pre-shift and no loss of precision.  It also confines that product to the
 *  two octaves [2^28, 2^30), which turns the renormalization after a multiply
 *  into one conditional shift instead of a leading-bit search.
 *
 *  Why the mantissa is signed rather than sign-and-magnitude: once the
 *  exponents are aligned, addition is a plain integer add.  A separate sign bit
 *  would force a magnitude comparison and a sign recomputation on every add,
 *  which is a large part of what makes __addsf3 expensive.
 */

typedef int32_t myflpt;

//! Number of significant bits in a normalized mantissa (magnitude only)
#define MYFLPT_MAN_BITS 15

//! Sign-extended mantissa, bits 15..0
#define MYFLPT_MAN(a) ((int32_t)(int16_t)(a))

//! Sign-extended exponent, bits 31..16
#define MYFLPT_EXP(a) ((a) >> 16)

//! Reassemble a word from a mantissa and an exponent
#define MYFLPT_PACK(m, e) \
    ((myflpt)(((uint32_t)(e) << 16) | ((uint32_t)(m) & 0xFFFFu)))

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
