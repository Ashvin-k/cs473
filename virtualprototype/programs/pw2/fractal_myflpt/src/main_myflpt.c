#include "fractal_myflpt.h"
#include "swap.h"
#include "vga.h"
#include "cache.h"
#include <stddef.h>
#include <stdio.h>
#include "spr.h"

const int SCREEN_WIDTH = 512;
const int SCREEN_HEIGHT = 512;
const uint16_t N_MAX = 64;

//! \brief Draw the Mandelbrot fractal on the VGA screen
int main() {
   volatile unsigned int *vga = (unsigned int *) 0x50000020;
   rgb565 frameBuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
   int i;

   myflpt cx_0  = create_myflpt(-2, 0);
   myflpt cy_0  = create_myflpt(-3, -1);
   myflpt delta = create_myflpt(3, -9);

   vga_clear();
   printf("Starting drawing a fractal\n");

#ifdef __OR1300__
   icache_write_cfg( CACHE_DIRECT_MAPPED | CACHE_SIZE_8K | CACHE_REPLACE_FIFO );
   dcache_write_cfg( CACHE_FOUR_WAY | CACHE_SIZE_8K | CACHE_REPLACE_LRU | CACHE_WRITE_BACK );
   icache_enable(1);
   dcache_enable(1);
#endif

   vga[0] = swap_u32(SCREEN_WIDTH);
   vga[1] = swap_u32(SCREEN_HEIGHT);
   vga[2] = swap_u32(1);
   vga[3] = swap_u32((unsigned int)&frameBuffer[0]);

   for (i = 0 ; i < SCREEN_WIDTH*SCREEN_HEIGHT ; i++) frameBuffer[i]=0;

#ifdef __OR1300__
   dcache_flush();
#endif

   draw_fractal(frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                &calc_mandelbrot_point_soft, &iter_to_colour,
                cx_0, cy_0, delta, N_MAX);

#ifdef __OR1300__
   dcache_flush();
#endif
   printf("Done\n");
}