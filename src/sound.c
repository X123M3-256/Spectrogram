#include<stdlib.h>
#include<stdio.h>
#include "sound.h"


static int sound_callback(const void *input_buffer,void *output_buffer,unsigned long frames_per_buffer,const PaStreamCallbackTimeInfo* time_info,PaStreamCallbackFlags status_flags,void *user_data)
{
playback_state_t* data=(playback_state_t*)user_data; 
float* out =(float*)output_buffer;
	
	for(int i=0;i<frames_per_buffer;i++)
	{
		if(data->index<data->num_samples)
		{
		out[i]=0.1*data->samples[data->index];
		data->index++;
		}
		else
		{
		out[i]=0.0;
		}
	}
return 0;
}


int sound_init()
{
PaError err=Pa_Initialize();
	if(err!=paNoError)
	{
	printf("PortAudio error: %s\n",Pa_GetErrorText(err));
	return 1;
	}
return 0;
}

int sound_play(playback_state_t* state,double* samples,int num_samples,int sample_rate,int start_sample)
{
state->samples=samples;
state->num_samples=num_samples;
state->index=start_sample;

PaError err=Pa_OpenDefaultStream(&(state->stream),0,1,paFloat32,sample_rate,paFramesPerBufferUnspecified,sound_callback,state);
	if(err!=paNoError)goto error;

err=Pa_StartStream(state->stream);
	if(err!=paNoError) goto error;

return 0;
error:
printf("PortAudio error: %s\n",Pa_GetErrorText(err));
return 1;
}



int sound_stop(playback_state_t* state)
{
PaError err=Pa_StopStream(state->stream);
	if(err!=paNoError)
	{
	printf("PortAudio error: %s\n",Pa_GetErrorText(err));
	return 1;
	}
return 0;
}

int sound_finish()
{
PaError err=Pa_Terminate();
	if(err!=paNoError)
	{
	printf("PortAudio error: %s\n",Pa_GetErrorText(err));
	return 1;
	}
return 0;
}
