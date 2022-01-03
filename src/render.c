#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include<math.h>
#include "render.h"
#include "fft.h"

#define FREQUENCY_RESOLUTION 2048
#define TIME_RESOLUTION 20
#define SPECTRUM_CACHE_LENGTH 2048
#define X_SCALE 2

typedef struct
{
double min_value;
double max_value;
int num_colors;
double* colors;
}color_map_t;

int color_from_rgb(double* rgb)
{
int r=(int)round(255*rgb[0]);
int g=(int)round(255*rgb[1]);
int b=(int)round(255*rgb[2]);
return (r<<16)|(g<<8)|b;
}

int color_map(color_map_t color_map,double level)
{
double u=(color_map.num_colors-1)*(level-color_map.min_value)/(color_map.max_value-color_map.min_value);
int index=(int)floor(u);
u=u-index;
	if(index<0)return color_from_rgb(color_map.colors);
	else if(index>=color_map.num_colors-1)return color_from_rgb(color_map.colors+3*(color_map.num_colors-1));
	else
	{
	int base=3*index;
	double rgb[3];
	rgb[0]=color_map.colors[base]+u*(color_map.colors[base+3]-color_map.colors[base]);
	rgb[1]=color_map.colors[base+1]+u*(color_map.colors[base+4]-color_map.colors[base+1]);
	rgb[2]=color_map.colors[base+2]+u*(color_map.colors[base+5]-color_map.colors[base+2]);
	return color_from_rgb(rgb);
	}
}

double scale_colors[3*6]={0,0.0,0.5, 0.0,0.0,1.0, 0.0,0.0,0.0, 1.0,0.0,0.0, 1.0,1.0,0.0, 1.0,1.0,1.0};
color_map_t scale={-2,3,6,scale_colors};

const int x_origin=75;
const int x_margin=40;

/*
double fourier_transform(int n,float* samples)
{
double N=n;
double real=0.0;
double imag=0.0;
	for(int i=0;i<n;i++)
	{
	double theta=2*M_PI*k*i/N;
	real+= samples[i]*cos(theta);
	imag+=-samples[i]*sin(theta);
	}
return sqrt(real*real+imag*imag);
}
*/

double plot_x_from_index(plot_data_t* plot_data,int index)
{
return x_origin+plot_data->x_tick_spacing*(index/(float)plot_data->sample_rate-plot_data->x_scroll);
}

int plot_index_from_x(plot_data_t* plot_data,double x)
{
int index=(int)round(plot_data->sample_rate*((x-x_origin)/plot_data->x_tick_spacing+plot_data->x_scroll));
	if(index<0)return 0;
	if(index>=plot_data->num_samples)return plot_data->num_samples-1;
return index;
}


void spectrum_cache_init(spectrum_cache_t* cache,int sample_rate)
{
cache->start_index=-1;
cache->sample_rate=sample_rate;
cache->amplitude=malloc(SPECTRUM_CACHE_LENGTH*FREQUENCY_RESOLUTION*sizeof(double));
}

void spectrum_cache_update(spectrum_cache_t* cache,int start_index,int end_index,int num_samples,double* samples)
{
//Update memory allocation
	if(end_index-start_index>SPECTRUM_CACHE_LENGTH)
	{
	printf("Spectrogram size exceeded cache size (TODO make this variable)\n");
	exit(0);
	}


double* amplitude=cache->amplitude;
	if(cache->start_index>=0)
	{
		if(start_index>=cache->start_index&&end_index<=cache->start_index+SPECTRUM_CACHE_LENGTH)
		{
		return;//Nothing to update
		}
		else if(start_index<cache->start_index)
		{
		start_index=end_index-SPECTRUM_CACHE_LENGTH;
			if(start_index<0)start_index=0;
		int start_offset=cache->start_index-start_index;
		memmove(cache->amplitude+start_offset*FREQUENCY_RESOLUTION,cache->amplitude,(SPECTRUM_CACHE_LENGTH+start_index-cache->start_index)*FREQUENCY_RESOLUTION*sizeof(double));
		end_index=cache->start_index;
		cache->start_index=start_index;
		}
		else
		{
		int start_offset=start_index-cache->start_index;
		memmove(cache->amplitude,cache->amplitude+start_offset*FREQUENCY_RESOLUTION,(SPECTRUM_CACHE_LENGTH-start_index+cache->start_index)*FREQUENCY_RESOLUTION*sizeof(double));
		int new_start_index=start_index;
		start_index=cache->start_index+SPECTRUM_CACHE_LENGTH;
		end_index=new_start_index+SPECTRUM_CACHE_LENGTH;
		amplitude=cache->amplitude+(SPECTRUM_CACHE_LENGTH-(new_start_index-cache->start_index))*FREQUENCY_RESOLUTION;
		cache->start_index=new_start_index;
		//printf("%d %d\n",start_index,end_index);
		}
	}
	else
	{
	cache->start_index=start_index;
	end_index=start_index+SPECTRUM_CACHE_LENGTH;
	}

fft_context_t fft;
fft_init(&fft,2*FREQUENCY_RESOLUTION);


double* result=calloc(2*FREQUENCY_RESOLUTION,sizeof(double));
	for(int j=start_index;j<end_index;j++)
	{
	int index=(j*cache->sample_rate)/TIME_RESOLUTION;
		if(index+2*FREQUENCY_RESOLUTION<=num_samples)
		{
		fft_transform(&fft,samples+index,result);
			for(int k=0;k<FREQUENCY_RESOLUTION;k++)
			{
			amplitude[(j-start_index)*FREQUENCY_RESOLUTION+k]=sqrt(result[2*k]*result[2*k]+result[2*k+1]*result[2*k+1]);
			}
		} else memset(amplitude+(j-start_index)*FREQUENCY_RESOLUTION,0,FREQUENCY_RESOLUTION*sizeof(double));
	}
free(result);
}

#define FREQ_LOW 100.0
#define FREQ_HIGH 20000.0

double spectrum_cache_fetch(spectrum_cache_t* cache,int time_step,double freq_low,double freq_high)
{
	if(time_step<cache->start_index)time_step=cache->start_index;
	else if(time_step>=cache->start_index+SPECTRUM_CACHE_LENGTH)time_step=cache->start_index+SPECTRUM_CACHE_LENGTH-1;

double* magnitude=cache->amplitude+FREQUENCY_RESOLUTION*(time_step-cache->start_index);

float index_low=(2*FREQUENCY_RESOLUTION*freq_low)/cache->sample_rate;
float index_high=(2*FREQUENCY_RESOLUTION*freq_high)/cache->sample_rate;
int i_low=(int)floor(index_low);
int i_high=(int)floor(index_high);

double value=0.0;
	if(i_low==i_high)
	{
	//Pixel smaller than samples; interpolate
	double u=0.5*(index_high+index_low)-i_low;
	value=magnitude[i_low]+u*(magnitude[i_low+1]-magnitude[i_low]);
	}
	else
	{
	//Pixel larger than samples, average
		for(int i=i_low+1;i<=i_high;i++)value+=magnitude[i];
	value/=i_high-i_low;
	}
return value;
}

double plot_get_x_length(plot_data_t* plot_data)
{
return plot_data->width-x_origin-40;
}

double plot_get_y_length(plot_data_t* plot_data)
{
return (plot_data->height-125)/2;
}


void plot_update_spectrogram(plot_data_t* plot_data)
{
int width=plot_get_x_length(plot_data)/X_SCALE;
int height=plot_get_y_length(plot_data);

	if(plot_data->spectrogram==NULL)
	{
	plot_data->spectrogram=cairo_image_surface_create(CAIRO_FORMAT_RGB24,width/2,height);
	}

int m=cairo_image_surface_get_width(plot_data->spectrogram);
int n=cairo_image_surface_get_height(plot_data->spectrogram);

	if(width!=m||height!=n)
	{
	cairo_surface_destroy(plot_data->spectrogram);
	plot_data->spectrogram=cairo_image_surface_create(CAIRO_FORMAT_RGB24,width,height);
	}

int stride=cairo_image_surface_get_stride(plot_data->spectrogram);
unsigned char* data=cairo_image_surface_get_data(plot_data->spectrogram);

cairo_surface_flush(plot_data->spectrogram);

int start_index=plot_data->x_scroll*TIME_RESOLUTION;
spectrum_cache_update(&(plot_data->spectrum_cache),start_index,start_index+m,plot_data->num_samples,plot_data->samples);

double a=log(FREQ_HIGH/FREQ_LOW)/((double)height);
	for(int k=0;k<height;k++)
	{
	float pixel_freq_low=FREQ_LOW*exp(a*k);
	float pixel_freq_high=FREQ_LOW*exp(a*(k+1));
		for(int j=0;j<width;j++)
		{
		int index=4*j+((height-1)-k)*stride;
		double intensity=log10(spectrum_cache_fetch(&(plot_data->spectrum_cache),start_index+j,pixel_freq_low,pixel_freq_high));
		*((uint32_t*)(data+index))=color_map(scale,intensity);
		}
	}

cairo_surface_mark_dirty(plot_data->spectrogram);
}

void plot_update_size(plot_data_t* plot_data,int width,int height)
{
	if(width==plot_data->width&&height==plot_data->height)return;

plot_data->width=width;
plot_data->height=height;
plot_update_spectrogram(plot_data);
}


void plot_draw_spectrogram(plot_data_t* plot_data,cairo_t* cr)
{
int x_length=plot_get_x_length(plot_data);
int y_length=plot_get_y_length(plot_data);
int y_origin=y_length+25;

	if(plot_data->spectrogram)
	{
	cairo_save(cr);
	cairo_translate(cr,-x_origin,0);
	cairo_scale(cr,X_SCALE,1);
	cairo_set_source_surface(cr,plot_data->spectrogram,x_origin,y_origin-cairo_image_surface_get_height(plot_data->spectrogram));
	cairo_rectangle(cr,0,0,plot_data->width,plot_data->height);
	cairo_fill(cr);
	cairo_restore(cr);
	}

//Draw X axis
cairo_set_source_rgba(cr,0,0,0,1.0);
cairo_move_to(cr,x_origin,y_origin+0.5);
cairo_line_to(cr,x_origin+x_length,y_origin+0.5);
cairo_stroke(cr);
plot_data->x_tick_spacing=TIME_RESOLUTION*X_SCALE;
int x_ticks=(int)ceil(x_length/plot_data->x_tick_spacing);
	for(int i=0;i<x_ticks;i++)
	{
	float x_offset=x_origin+i*plot_data->x_tick_spacing+0.5;
	cairo_move_to(cr,x_offset,y_origin);
	cairo_line_to(cr,x_offset,y_origin+6);
	cairo_stroke(cr);

	char label[256];
	sprintf(label,"%ds",i+plot_data->x_scroll);
	cairo_text_extents_t extents;
	cairo_text_extents(cr,label,&extents);	
	cairo_move_to(cr,(int)(x_offset-(int)(extents.width/2)-extents.x_bearing),(int)(8-extents.y_bearing+y_origin));
	cairo_show_text(cr,label);
	cairo_fill(cr);
	}


//Draw Y axis
const int y_ticks=8;
const int y_tick_freqs[8]={20000,10000,5000,2000,1000,500,200,100};

cairo_move_to(cr,x_origin+0.5,y_origin);
cairo_line_to(cr,x_origin+0.5,y_origin-y_length);

	for(int i=0;i<y_ticks;i++)
	{
	float y_offset=y_origin-round(y_length*(1.0-(log(y_tick_freqs[i])-log(y_tick_freqs[0]))/(log(y_tick_freqs[y_ticks-1])-log(y_tick_freqs[0]))))+0.5;
	cairo_move_to(cr,x_origin,y_offset);
	cairo_line_to(cr,x_origin-5,y_offset);
	cairo_stroke(cr);
	
	char label[256];
		if(y_tick_freqs[i]>=1000)sprintf(label,"%d kHz",y_tick_freqs[i]/1000);
		else sprintf(label,"%d Hz",y_tick_freqs[i]);
	cairo_text_extents_t extents;
	cairo_text_extents(cr,label,&extents);	
	cairo_move_to(cr,(int)(x_origin-(int)(extents.width)-extents.x_bearing-8),(int)(y_offset-extents.y_bearing-(int)(extents.height/2)));
	cairo_show_text(cr,label);
	cairo_fill(cr);
	}

int cursor_offset=plot_x_from_index(plot_data,plot_data->index);
cairo_move_to(cr,cursor_offset+0.5,y_origin);
cairo_line_to(cr,cursor_offset+0.5,y_origin-y_length);
cairo_set_source_rgba(cr,1.0,0,1.0,1.0);
cairo_stroke(cr);
}

void plot_draw_spectrum(plot_data_t* plot_data,cairo_t* cr)
{
int x_length=plot_get_x_length(plot_data);
int y_length=plot_get_y_length(plot_data);
int y_origin=2*y_length+75;

//Draw X axis
const int x_ticks=10;
const int x_tick_freqs[10]={20000,10000,5000,2000,1000,500,200,100,50,20};

cairo_set_source_rgba(cr,0,0,0,1.0);
cairo_move_to(cr,x_origin,y_origin+0.5);
cairo_line_to(cr,x_origin+x_length,y_origin+0.5);
cairo_move_to(cr,x_origin,y_origin-y_length+0.5);
cairo_line_to(cr,x_origin+x_length,y_origin-y_length+0.5);
cairo_stroke(cr);

	for(int i=0;i<x_ticks;i++)
	{
	float x_offset=x_origin+round(x_length*(1.0-(log(x_tick_freqs[i])-log(x_tick_freqs[0]))/(log(x_tick_freqs[x_ticks-1])-log(x_tick_freqs[0]))))+0.5;
	cairo_move_to(cr,x_offset,y_origin);
	cairo_line_to(cr,x_offset,y_origin+7);
	cairo_stroke(cr);
	
	char label[256];
		if(x_tick_freqs[i]>=1000)sprintf(label,"%d kHz",x_tick_freqs[i]/1000);
		else sprintf(label,"%d Hz",x_tick_freqs[i]);
	cairo_text_extents_t extents;
	cairo_text_extents(cr,label,&extents);	
	cairo_move_to(cr,(int)(x_offset-(int)(extents.width/2)-extents.x_bearing),(int)(y_origin-extents.y_bearing+10));
	cairo_show_text(cr,label);
	cairo_fill(cr);
	}

//Draw Y axis
cairo_move_to(cr,x_origin+0.5,y_origin);
cairo_line_to(cr,x_origin+0.5,y_origin-y_length);
cairo_move_to(cr,x_origin+x_length+0.5,y_origin);
cairo_line_to(cr,x_origin+x_length+0.5,y_origin-y_length);
cairo_stroke(cr);


//Draw spectrum
int x_resolution=1024;
double a=log(FREQ_HIGH/FREQ_LOW)/((double)x_resolution);
int time_index=(plot_data->index*TIME_RESOLUTION)/plot_data->sample_rate;
cairo_move_to(cr,x_origin+x_length,y_origin);
cairo_line_to(cr,x_origin,y_origin);
	for(int k=0;k<x_resolution;k++)
	{
	float pixel_freq_low=FREQ_LOW*exp(a*k);
	float pixel_freq_high=FREQ_LOW*exp(a*(k+1));
	double intensity=-spectrum_cache_fetch(&(plot_data->spectrum_cache),time_index,pixel_freq_low,pixel_freq_high);
	cairo_line_to(cr,x_origin+x_length*((k+0.5)/x_resolution),y_origin+0.0025*y_length*intensity);
	}
cairo_set_source_rgba(cr,0.5,0.5,1.0,1.0);
cairo_fill_preserve(cr);
cairo_set_source_rgba(cr,0,0,0.5,1.0);
cairo_stroke(cr);
}



void draw_plot(plot_data_t* plot_data,cairo_t *cr)
{
cairo_set_font_size(cr,12);
cairo_set_line_width(cr,1);
plot_draw_spectrogram(plot_data,cr);
plot_draw_spectrum(plot_data,cr);
}
