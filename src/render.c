#include<stdlib.h>
#include<stdio.h>
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
color_map_t scale={-3,2,6,scale_colors};


//24KHz 12KHz 6KHz 3KHz 1500Hz 750Hz 375Hz 187.5Hz


const int y_tick_freqs[10]={20000,10000,5000,2000,1000,500,200,100,50,20};


void draw_spectrogram(cairo_surface_t* surface,double* samples,int step,int resolution)
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
	fft_transform(&fft,samples+step*j,result);
		for(int k=0;k<resolution/2;k++)
		{
		magnitude[k]=sqrt(result[2*k]*result[2*k]+result[2*k+1]*result[2*k+1]);
		}
		

	float freq_low=100.0;
	float freq_high=20000.0;

	float a=log(freq_high/freq_low)/(float)(n-1);
		for(int k=0;k<n;k++)
		{
		float freq=freq_low*exp(a*k);
		float y_offset=0.5*resolution*freq/24000;
		int i=(int)floor(y_offset);
			if(i<0)
			{
			printf("Lower limit exceeded\n");
			i=0;
			}

			if(i>=resolution/2)
			{
			printf("Upper limit exceeded %f %f\n",freq,y_offset);
			i=n/2-1;
			}

		float u=y_offset-i;
		float value=magnitude[i]+u*(magnitude[i+1]-magnitude[i]);

		int index=4*j+((n-1)-k)*stride;
		double intensity=log10(value);
		*((uint32_t*)(data+index))=color_map(scale,intensity);
		}

	}
free(result);

cairo_surface_mark_dirty(surface);

}



