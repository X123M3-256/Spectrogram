#include <stdlib.h>
#include <math.h>
#include "fft.h"

void rdft(int,int,double*,int*,double*);

void fourier_transform(int n,double* samples,double* r,double* i)
{
double N=n;
	for(int k=0;k<n/2;k++)
	{
	double real=0.0;
	double imag=0.0;
		for(int i=0;i<n;i++)
		{
		double theta=2*M_PI*k*i/N;
		real+= samples[i]*cos(theta);
		imag+=-samples[i]*sin(theta);
		}
	r[k]=real;
	i[k]=imag;
	}
}

double hanning_window(int n,double* samples,double* windowed_samples)
{
double N=n;
	for(int i=0;i<n;i++)
	{
	windowed_samples[i]=samples[i]*0.5*(1-cos(2*M_PI*i/N));
	}
}

void fft_init(fft_context_t* context,int n)
{
context->n=n;
context->ip=calloc((int)ceil(2+sqrt(n/2)),sizeof(int));
context->w=calloc(n/2,sizeof(double));
}


void fft_transform(fft_context_t* context,double* samples,double* result)
{
hanning_window(context->n,samples,result);
rdft(context->n,1,result,context->ip,context->w);
}
