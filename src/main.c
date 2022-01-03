#include<stdlib.h>
#include<stdio.h>
#include<stdint.h>
#include<gtk/gtk.h>
#include<math.h>
#include "sound.h"
#include "render.h"
#include "panel.h"

int playing=0;
int play_auto=0;
playback_state_t playback_state;

int button_pressed=0;


typedef struct
{
uint32_t id;
uint32_t size;
uint32_t format;
}chunk_header_t;

typedef struct
{
uint32_t id;
uint32_t size;
}subchunk_header_t;


plot_data_t plot_data;


typedef struct
{
uint16_t audio_format;
uint16_t num_channels;
uint32_t sample_rate;
uint32_t byte_rate;
uint16_t block_align;
uint16_t bits_per_sample;
}format_chunk_t;


void pause_audio()
{
	if(playing)
	{
	sound_stop(&playback_state);
	playing=0;
	}
}

void play_audio()
{
	if(playing)pause_audio();
	if(!sound_play(&playback_state,plot_data.samples,plot_data.num_samples,plot_data.sample_rate,playback_state.index))playing=1;
}

int update_plot(gpointer data)
{
plot_data.index=playback_state.index;
	if(playing&&plot_x_from_index(&plot_data,plot_data.index)>75+plot_get_x_length(&plot_data)-plot_data.x_tick_spacing)
	{
	plot_data.x_scroll=(int)floor(plot_data.index/plot_data.sample_rate)-1;
	plot_update_spectrogram(&plot_data);
	}
	if(playing)
	{
		if(playback_state.index>=plot_data.num_samples-1)
		{
		pause_audio();
		play_auto=0;
		}
		else g_timeout_add(50,update_plot,data);
	}
gtk_widget_queue_draw(GTK_WIDGET(data));
}



int load_wav(const char* filename)
{
FILE* file=fopen(filename,"r");
	if(!file)return 1;

chunk_header_t header;
	if(fread(&header,sizeof(chunk_header_t),1,file)!=1)
	{
	printf("Failed to read chunk header\n");
	return 1;
	}

	if(header.id!=0x46464952||header.format!=0x45564157)
	{
	printf("Chunk header invalid %x\n",header.id);
	return 1;
	}

int format_read=0;
format_chunk_t format;
subchunk_header_t subchunk_header;
	while(fread(&subchunk_header,sizeof(subchunk_header_t),1,file)==1)
	{
	//printf("Subchunk %c%c%c%c\n",subchunk_header.id&0xFF,(subchunk_header.id&0xFF00)>>8,(subchunk_header.id&0xFF0000)>>16,(subchunk_header.id&0xFF000000)>>24);
		if(subchunk_header.id==0x20746d66)
		{
			if(subchunk_header.size<16)
			{
			printf("Format subchunk is less than 16 bytes\n");
			return 1;
			}			

			if(fread(&format,sizeof(format_chunk_t),1,file)!=1)
			{
			printf("Failed reading format subchunk\n");
			return 1;
			}
			
			if(format.audio_format!=1||format.num_channels!=1||format.bits_per_sample!=16)
			{
			printf("File is not 16 bit PCM mono\n");
			return 1;
			}
		plot_data.sample_rate=format.sample_rate;

			if(subchunk_header.size>16)fseek(file,subchunk_header.size-16,SEEK_CUR);

		format_read=1;
		}
		else if(subchunk_header.id==0x61746164)
		{
			if(format_read!=1)
			{
			printf("Data subchunk encountered before format subchunk\n");
			return 1;
			}

			if(subchunk_header.size&1)
			{
			printf("Data subchunk size not divisible by 2\n");
			return 1;
			}

		plot_data.num_samples=subchunk_header.size/2;
		int16_t* data=calloc(plot_data.num_samples,sizeof(int16_t));
		free(plot_data.samples);
		plot_data.samples=calloc(plot_data.num_samples,sizeof(double));
			if(fread(data,2,plot_data.num_samples,file)!=plot_data.num_samples)
			{
			printf("Failed reading data subchunk\n");
			return 1;
			}
		
			for(int i=0;i<plot_data.num_samples;i++)plot_data.samples[i]=data[i]/32767.0;
		free(data);
		}
		else
		{
		//Seek to next chunk
		fseek(file,subchunk_header.size,SEEK_CUR);
		}
	}


//printf("Sample depth %d\n",header.bits_per_sample);
//printf("Sample rate %d\n",header.plot_data.sample_rate);
//printf("Channels %d\n",header.num_channels);

fclose(file);



pause_audio();
play_auto=0;
playback_state.index=0;
plot_data.x_scroll=0;
plot_data.spectrum_cache.start_index=-1;
plot_data.spectrum_cache.sample_rate=plot_data.sample_rate;
return 0;
}







gboolean on_draw(GtkWidget *widget,cairo_t *cr,gpointer unused)
{
GdkRGBA color;
GtkStyleContext* context=gtk_widget_get_style_context(widget);

plot_update_size(&plot_data,gtk_widget_get_allocated_width(widget),gtk_widget_get_allocated_height(widget));

gtk_render_background(context,cr,0,0,plot_data.width,plot_data.height);
draw_plot(&plot_data,cr);

return FALSE;
}





void play_clicked(GtkWidget *widget,gpointer data)
{
	if(playing)return;
play_audio();
update_plot(data);
play_auto=1;
}

void pause_clicked(GtkWidget *widget,gpointer data)
{
pause_audio();
play_auto=0;
}


gboolean on_scroll(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
	if(event->scroll.direction==GDK_SCROLL_UP)
	{
	plot_data.x_scroll++;
	plot_update_spectrogram(&plot_data);
		if(plot_x_from_index(&plot_data,plot_data.index)<75)
		{
		pause_audio();
		playback_state.index=plot_index_from_x(&plot_data,75);
		plot_data.index=playback_state.index;
			if(plot_data.index>=plot_data.num_samples-1)
			{
			plot_data.x_scroll--;
			plot_data.index=plot_data.num_samples-1;
			playback_state.index=plot_data.num_samples-1;
			}
			else if(play_auto)play_audio();	
		}
	}
	else if(event->scroll.direction==GDK_SCROLL_DOWN)
	{
		if(plot_data.x_scroll>0)
		{
		plot_data.x_scroll--;
		plot_update_spectrogram(&plot_data);

			if(plot_x_from_index(&plot_data,plot_data.index)>75+plot_get_x_length(&plot_data))
			{
			pause_audio();
			playback_state.index=plot_index_from_x(&plot_data,75+plot_get_x_length(&plot_data));
			plot_data.index=playback_state.index;
				//I don't think this is ever actually triggered as it will autoscroll when playing
				if(play_auto)play_audio();
			}

		}
	}
gtk_widget_queue_draw(GTK_WIDGET(widget));
return TRUE;  
}


gboolean on_motion(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
	if(button_pressed)
	{
	pause_audio();
	playback_state.index=plot_index_from_x(&plot_data,event->motion.x);
	plot_data.index=playback_state.index;
	gtk_widget_queue_draw(GTK_WIDGET(widget));
	}
}

gboolean on_button_press(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
button_pressed=1;
}

gboolean on_button_release(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
playback_state.index=plot_index_from_x(&plot_data,event->button.x);
	if(play_auto)play_audio();
button_pressed=0;
update_plot(widget);
}

void on_open_clicked(GtkWidget *widget,gpointer data)
{
GtkWidget* dialog=gtk_file_chooser_dialog_new("Import Log File",GTK_WINDOW(data),GTK_FILE_CHOOSER_ACTION_OPEN,"_Cancel",GTK_RESPONSE_CANCEL,"_Import",GTK_RESPONSE_ACCEPT,NULL);

gint res=gtk_dialog_run(GTK_DIALOG(dialog));
	if(res==GTK_RESPONSE_ACCEPT)
	{
	char *filename;
	GtkFileChooser* chooser=GTK_FILE_CHOOSER(dialog);
    	filename=gtk_file_chooser_get_filename(chooser);
		if(load_wav(filename))
		{
		gtk_widget_destroy (dialog);
		char error_text[512];
		sprintf(error_text,"Could not read WAV file %.400s",filename);
		GtkWidget* error_dialog=gtk_message_dialog_new(GTK_WINDOW(data),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,error_text);
		gtk_window_set_title(GTK_WINDOW(error_dialog),"Error");
		gtk_dialog_run(GTK_DIALOG(error_dialog));
		gtk_widget_destroy(error_dialog);
		return;
		}
	plot_update_spectrogram(&plot_data);
	gtk_widget_queue_draw(GTK_WIDGET(data));
	g_free(filename);
	}
gtk_widget_destroy (dialog);
}


gboolean on_window_delete(GtkWidget *widget,GdkEvent *event,gpointer data)
{
gtk_main_quit();
return FALSE;
}


int main(int argc,char **argv)
{
	if(argc>1)
	{
		if(load_wav(argv[1]))
		{
		printf("Failed loading file %s\n",argv[1]);
		return 1;
		}
	spectrum_cache_init(&(plot_data.spectrum_cache),plot_data.sample_rate);
	}
	else
	{
	plot_data.sample_rate=48000;
	plot_data.num_samples=0;
	plot_data.samples=NULL;
	spectrum_cache_init(&(plot_data.spectrum_cache),48000);
	}	
	

/*
double freq_ref=200;
plot_data.sample_rate=8000;
plot_data.num_samples=8000;
plot_data.samples=calloc(plot_data.num_samples,sizeof(double));
	for(int i=0;i<plot_data.num_samples;i++)
	{
	plot_data.samples[i]=sin(2*M_PI*freq_ref*(i/(double)plot_data.sample_rate));
	}
*/

/*
*/

plot_data.spectrogram=NULL;
plot_data.width=0;
plot_data.height=0;

	if(sound_init())return 1;


gtk_init(&argc,&argv);


GtkBuilder* builder=gtk_builder_new();

	if(gtk_builder_add_from_file(builder,"main.glade",NULL)==0)
	{
	printf("gtk_builder_add_from_file FAILED\n");
	return 0;
	}


GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(builder,"window1"));
//gtk_window_fullscreen(GTK_WINDOW(window));
GtkWidget* draw_area=GTK_WIDGET(gtk_builder_get_object(builder,"draw_area"));
gtk_widget_add_events(draw_area,GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_SCROLL_MASK);
gtk_builder_connect_signals(builder,NULL);

gtk_widget_show_all(window);

gtk_main();

sound_finish();	
return 0;
}
