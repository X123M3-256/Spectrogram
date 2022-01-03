#ifndef SPECTRUM_H_INCLUDED
#define SPECTRUM_H_INCLUDED
#include <cairo/cairo.h>

typedef struct
{
int start_index;
uint32_t sample_rate;
double* amplitude;
}spectrum_cache_t;


typedef struct
{
uint32_t sample_rate;
uint32_t num_samples;
double* samples;

spectrum_cache_t spectrum_cache;

int width;
int height;
int index;
float x_tick_spacing;
int x_scroll;
cairo_surface_t* spectrogram;
}plot_data_t;



double plot_x_from_index(plot_data_t* plot_data,int index);
int plot_index_from_x(plot_data_t* plot_data,double x);

void spectrum_cache_init(spectrum_cache_t* cache,int sample_rate);
void draw_plot(plot_data_t* plot_data,cairo_t *cr);
double plot_get_x_length(plot_data_t* plot_data);
double plot_get_y_length(plot_data_t* plot_data);
void plot_update_size(plot_data_t* plot_data,int width,int height);
void plot_update_spectrogram(plot_data_t* plot_data);


#endif
