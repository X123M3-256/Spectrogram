#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED
#include <cairo/cairo.h>

void draw_spectrogram(cairo_surface_t* surface,int resolution,float sample_rate,float freq_low,float freq_high,int sample_step,double* samples);


#endif
