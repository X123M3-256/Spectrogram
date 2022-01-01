#ifndef FFT_H_INCLUDED
#define FFT_H_INCLUDED

typedef struct
{
int n;
int* ip;
double* w;
}fft_context_t;



void fft_init(fft_context_t* context,int n);
void fft_transform(fft_context_t* context,double* samples,double* result);


#endif

