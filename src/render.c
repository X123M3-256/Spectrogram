#include<stdlib.h>
#include<stdio.h>
#include<stdint.h>
#include<math.h>
#include "render.h"
#include "fft.h"

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


//24KHz 12KHz 6KHz 3KHz 1500Hz 750Hz 375Hz 187.5Hz


const int y_tick_freqs[10]={20000,10000,5000,2000,1000,500,200,100,50,20};


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



void draw_spectrogram(cairo_surface_t* surface,int resolution,float sample_rate,float freq_low,float freq_high,int sample_step,double* samples)
{
cairo_surface_flush(surface);
int stride=cairo_image_surface_get_stride(surface);
unsigned char* data=cairo_image_surface_get_data(surface);
int n=cairo_image_surface_get_height(surface);
int m=cairo_image_surface_get_width(surface);

fft_context_t fft;
fft_init(&fft,resolution);

double* result=calloc(resolution,sizeof(double));
double* magnitude=calloc(resolution/2,sizeof(double));

	for(int j=0;j<m;j++)
	{
	fft_transform(&fft,samples+sample_step*j,result);
		for(int k=0;k<resolution/2;k++)
		{
		magnitude[k]=sqrt(result[2*k]*result[2*k]+result[2*k+1]*result[2*k+1]);
		}
		


	float a=log(freq_high/freq_low)/((float)n);
		for(int k=0;k<n;k++)
		{
		float pixel_freq_low=freq_low*exp(a*k);
		float pixel_freq_high=freq_low*exp(a*(k+1));
		float pixel_index_low=resolution*pixel_freq_low/sample_rate;
		float pixel_index_high=resolution*pixel_freq_high/sample_rate;

		int i_low=(int)floor(pixel_index_low);
		int i_high=(int)floor(pixel_index_high);
		float value=0.0;
			if(i_low==i_high)
			{
			//Pixel smaller than samples; interpolate
			float u=0.5*(pixel_index_high+pixel_index_low)-i_low;
			value=magnitude[i_low]+u*(magnitude[i_low+1]-magnitude[i_low]);
			}
			else
			{
			//Pixel larger than samples, average
				for(int i=i_low+1;i<=i_high;i++)value+=magnitude[i];
			value/=i_high-i_low;
			}
		int index=4*j+((n-1)-k)*stride;
		double intensity=log10(value);
		*((uint32_t*)(data+index))=color_map(scale,intensity);
		}

	}
free(result);

cairo_surface_mark_dirty(surface);

}



