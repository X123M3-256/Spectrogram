#ifndef SOUND_H_INCLUDED
#define SOUND_H_INCLUDED
#include<portaudio.h>

typedef struct
{
PaStream* stream;
int num_samples;
double* samples;
int index;
}   
playback_state_t;


int sound_init();
int sound_play(playback_state_t* state,double* samples,int num_samples,int sample_rate,int start_sample);
int sound_stop(playback_state_t* state);
int sound_finish();

#endif
