#ifndef FRACTAL_MYFLPT_H
#define FRACTAL_MYFLPT_H

#include <stdint.h>

typedef int32_t myflpt;

#define MYFLPT_MAN_BITS 23

#define MYFLPT_EXP_MIN (-128)
#define MYFLPT_EXP_MAX 127

#define MYFLPT_MAN(a) (((int32_t)((uint32_t)(a) << 8)) >> 8)

#define MYFLPT_EXP(a) ((int32_t)(a) >> 24)

#define MYFLPT_PACK(m, e) \
    ((myflpt)(((uint32_t)(e) << 24) | ((uint32_t)(m) & 0x00FFFFFFu)))

typedef uint16_t rgb565;

//! \brief Create a myflpt equal to man * 2^exp
myflpt create_myflpt(int32_t man, int32_t exp);

//! \brief Add two myflpt
myflpt myflpt_add(myflpt a, myflpt b);

//! \brief Multiply two myflpt
myflpt myflpt_mul(myflpt a, myflpt b);

//! \brief Negate a myflpt
myflpt myflpt_neg(myflpt a);

//! \brief Return 1 if a > b, 0 otherwise
int myflpt_gt(myflpt a, myflpt b);

typedef uint16_t (*calc_frac_point_p)(myflpt cx, myflpt cy, uint16_t n_max);

//! \brief Count the Mandelbrot iterations of the point cx + i*cy
uint16_t calc_mandelbrot_point_soft(myflpt cx, myflpt cy, uint16_t n_max);

typedef rgb565 (*iter_to_colour_p)(uint16_t iter, uint16_t n_max);

//! \brief Map number of performed iterations to black and white
rgb565 iter_to_bw(uint16_t iter, uint16_t n_max);

//! \brief Map number of performed iterations to grayscale
rgb565 iter_to_grayscale(uint16_t iter, uint16_t n_max);

//! \brief Map number of performed iterations to a colour
rgb565 iter_to_colour(uint16_t iter, uint16_t n_max);

//! \brief Map number of performed iterations to a colour
rgb565 iter_to_colour1(uint16_t iter, uint16_t n_max);

//! \brief Draw fractal into frame buffer
void draw_fractal(rgb565 *fbuf, int width, int height,
                  calc_frac_point_p cfp_p, iter_to_colour_p i2c_p,
                  myflpt cx_0, myflpt cy_0, myflpt delta, uint16_t n_max);

#endif
