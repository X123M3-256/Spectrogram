#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED
#include <cairo/cairo.h>
#include "vectormath.h"

void draw_spectrogram(cairo_surface_t* surface,double* samples,int step,int resolution);


#endif
