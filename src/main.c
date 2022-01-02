#include<stdlib.h>
#include<stdio.h>
#include<stdint.h>
#include<gtk/gtk.h>
#include<math.h>
#include "sound.h"
#include "render.h"
#include "panel.h"

uint32_t sample_rate;
uint32_t num_samples;
double* samples;
int sample_step;

int playing=0;
int play_auto=0;
playback_state_t playback_state;

int button_pressed=0;


cairo_surface_t* surface;


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

typedef struct
{
uint16_t audio_format;
uint16_t num_channels;
uint32_t sample_rate;
uint32_t byte_rate;
uint16_t block_align;
uint16_t bits_per_sample;
}format_chunk_t;

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
		sample_rate=format.sample_rate;

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

		num_samples=subchunk_header.size/2;
		int16_t* data=calloc(num_samples,sizeof(int16_t));
		samples=calloc(num_samples,sizeof(double));
			if(fread(data,2,num_samples,file)!=num_samples)
			{
			printf("Failed reading data subchunk\n");
			return 1;
			}
		
			for(int i=0;i<num_samples;i++)samples[i]=data[i]/32767.0;
		free(data);
		}
		else
		{
		//Seek to next chunk
		fseek(file,subchunk_header.size,SEEK_CUR);
		}
	}


//printf("Sample depth %d\n",header.bits_per_sample);
//printf("Sample rate %d\n",header.sample_rate);
//printf("Channels %d\n",header.num_channels);

fclose(file);
return 0;
}



typedef struct
{
int x_origin;
float x_tick_spacing;
float x_limit_low;
float x_limit_high;
cairo_surface_t* spectrogram;
}plot_data_t;

plot_data_t plot_data;

double plot_x_from_index(plot_data_t* plot_data,int index)
{
return plot_data->x_origin+plot_data->x_tick_spacing*index/(float)sample_rate;
}

int plot_index_from_x(plot_data_t* plot_data,double x)
{
return (int)round(sample_rate*(x-plot_data->x_origin)/plot_data->x_tick_spacing);
}


gboolean on_draw(GtkWidget *widget,cairo_t *cr,gpointer unused)
{
GdkRGBA color;
GtkStyleContext* context=gtk_widget_get_style_context(widget);

int width=gtk_widget_get_allocated_width(widget);
int height=gtk_widget_get_allocated_height(widget);
gtk_render_background(context,cr,0,0,width,height);

cairo_set_font_size(cr,12);
cairo_set_line_width(cr,1);

plot_data.x_origin=75;
int y_origin=height-50;
int x_length=cairo_image_surface_get_width(surface);
int y_length=cairo_image_surface_get_height(surface);


cairo_set_source_surface(cr,surface,plot_data.x_origin,y_origin-cairo_image_surface_get_height(surface));
cairo_rectangle(cr,0,0,width,height);
cairo_fill(cr);

//Draw X axis
cairo_set_source_rgba(cr,0,0,0,1.0);
cairo_move_to(cr,plot_data.x_origin,y_origin+0.5);
cairo_line_to(cr,plot_data.x_origin+x_length,y_origin+0.5);
cairo_stroke(cr);
plot_data.x_tick_spacing=round(sample_rate/1200.0);
int x_ticks=(int)floor(x_length/plot_data.x_tick_spacing);
	for(int i=0;i<x_ticks;i++)
	{
	float x_offset=plot_data.x_origin+i*plot_data.x_tick_spacing+0.5;
	cairo_move_to(cr,x_offset,y_origin);
	cairo_line_to(cr,x_offset,y_origin+6);
	cairo_stroke(cr);

	char label[256];
	sprintf(label,"%ds",i);
	cairo_text_extents_t extents;
	cairo_text_extents(cr,label,&extents);	
	cairo_move_to(cr,(int)(x_offset-(int)(extents.width/2)-extents.x_bearing),(int)(8-extents.y_bearing+y_origin));
	cairo_show_text(cr,label);
	cairo_fill(cr);
	}

const int y_ticks=8;
const int y_tick_freqs[8]={20000,10000,5000,2000,1000,500,200,100};

cairo_move_to(cr,plot_data.x_origin+0.5,y_origin);
cairo_line_to(cr,plot_data.x_origin+0.5,y_origin-y_length);

	for(int i=0;i<y_ticks;i++)
	{
	float y_offset=y_origin-round(y_length*(1.0-(log(y_tick_freqs[i])-log(y_tick_freqs[0]))/(log(y_tick_freqs[y_ticks-1])-log(y_tick_freqs[0]))))+0.5;
	cairo_move_to(cr,plot_data.x_origin,y_offset);
	cairo_line_to(cr,plot_data.x_origin-5,y_offset);
	cairo_stroke(cr);
	
	char label[256];
		if(y_tick_freqs[i]>=1000)sprintf(label,"%d kHz",y_tick_freqs[i]/1000);
		else sprintf(label,"%d Hz",y_tick_freqs[i]);
	cairo_text_extents_t extents;
	cairo_text_extents(cr,label,&extents);	
	cairo_move_to(cr,(int)(plot_data.x_origin-(int)(extents.width)-extents.x_bearing-8),(int)(y_offset-extents.y_bearing-(int)(extents.height/2)));
	cairo_show_text(cr,label);
	cairo_fill(cr);
	}

int cursor_offset=plot_x_from_index(&plot_data,playback_state.index);
cairo_move_to(cr,cursor_offset+0.5,y_origin);
cairo_line_to(cr,cursor_offset+0.5,y_origin-y_length);
cairo_set_source_rgba(cr,1.0,0,1.0,1.0);
cairo_stroke(cr);

return FALSE;
}

int update_plot(gpointer data)
{
	if(playing)g_timeout_add(50,update_plot,data);
gtk_widget_queue_draw(GTK_WIDGET(data));
}

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
sound_play(&playback_state,samples,num_samples,sample_rate,playback_state.index);
playing=1;
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



gboolean on_motion(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
	if(button_pressed)
	{
	pause_audio();
	playback_state.index=plot_index_from_x(&plot_data,event->motion.x);
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




int main(int argc,char **argv)
{
	if(argc<2)
	{
	printf("Usage: spectrogram <file>\n");
	return 1;
	}

surface=cairo_image_surface_create(CAIRO_FORMAT_RGB24,1200,800);


/*
double freq_ref=200;
sample_rate=8000;
num_samples=8000;
samples=calloc(num_samples,sizeof(double));
	for(int i=0;i<num_samples;i++)
	{
	samples[i]=sin(2*M_PI*freq_ref*(i/(double)sample_rate));
	}
*/

	if(load_wav(argv[1]))
	{
	printf("Failed loading file %s\n",argv[1]);
	return 1;
	}	

sample_step=1200;
draw_spectrogram(surface,2048,48000,100.0,20000,sample_step,samples);


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
