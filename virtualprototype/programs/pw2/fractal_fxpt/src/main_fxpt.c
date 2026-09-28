#include "fractal_fxpt.h"
#include "swap.h"
#include "vga.h"
#include "cache.h"
#include <stddef.h>
#include <stdio.h>

// Constants describing the output device
const int SCREEN_WIDTH = 512;   //!< screen width
const int SCREEN_HEIGHT = 512;  //!< screen height

int main() {
   volatile unsigned int *vga = (unsigned int *) 0x50000020;
   rgb565 frameBuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
   int i;
   
   // Clean variable definition evaluated at compile-time (no magic numbers, no floats)
   fxpt_8_24 cx_0  = -2 * (1 << 24);                  // Corresponds to -2.0
   fxpt_8_24 cy_0  = -(3 * (1 << 24)) / 2;            // Corresponds to -1.5
   fxpt_8_24 delta = (3 * (1 << 24)) / SCREEN_WIDTH;  // Corresponds to 3.0 / 512
   uint16_t n_max  = 64; 
   
   vga_clear();
   printf("STARTING FRACTAL DRAWING...\n");
   
#ifdef __OR1300__   
   /* enable the caches */
   icache_write_cfg( CACHE_DIRECT_MAPPED | CACHE_SIZE_8K | CACHE_REPLACE_FIFO );
   dcache_write_cfg( CACHE_FOUR_WAY | CACHE_SIZE_8K | CACHE_REPLACE_LRU | CACHE_WRITE_BACK );
   icache_enable(1);
   dcache_enable(1);
#endif

   /* Enable the vga-controller's graphic mode */
   vga[0] = swap_u32(SCREEN_WIDTH);
   vga[1] = swap_u32(SCREEN_HEIGHT);
   vga[2] = swap_u32(1);
   vga[3] = swap_u32((unsigned int)&frameBuffer[0]);
   
   /* Clear screen (set to black) */
   for (i = 0 ; i < SCREEN_WIDTH*SCREEN_HEIGHT ; i++) frameBuffer[i]=0;

   /* Push the cleared black cache to the physical RAM/screen immediately */
#ifdef __OR1300__
   dcache_flush();
#endif

   // The processor calculates and draws the fractal
   draw_fractal(frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, 
                &calc_mandelbrot_point_soft, &iter_to_colour, 
                cx_0, cy_0, delta, n_max);

   /* MANDATORY: Push the final computed result to the screen */
#ifdef __OR1300__
   dcache_flush();
#endif

   printf("Done\n");
}