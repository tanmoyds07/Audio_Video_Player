#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "player.h"
#include "player.cpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0
#define AUDIO_DIFF_AVG_NB 20
#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_ALLOC_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)
#define VIDEO_PICTURE_QUEUE_SIZE 1
#define DEFAULT_AV_SYNC_TYPE AV_SYNC_AUDIO_MASTER
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

using namespace std;

int main(int argc, char *argv[])
{

	SDL_Event       event;
	VideoState      *is = new(nothrow) VideoState;
	if(argc < 2) 
	{
		fprintf(stderr, "Usage: test <file>\n");
		exit(1);
	}
	// Initialize libavformat & Register all formats and codecs
	av_register_all();
	// Do global initialization of network components.
	avformat_network_init();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS)) 
	{
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

	  // Creating a screen to put our video
	screen = SDL_CreateWindow("Audio Video Player",
		    		  SDL_WINDOWPOS_UNDEFINED,
		    		  SDL_WINDOWPOS_UNDEFINED,
		    		  1024, /* pCodecCtx->width, */
		    		  800, /* pCodecCtx->height, */
				  SDL_WINDOW_RESIZABLE);
	if(!screen)
	{
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

	renderer = SDL_CreateRenderer(screen, -1, 0);
	if (!renderer) 
	{
		fprintf(stderr, "SDL: could not create renderer - exiting\n");
		exit(1);
	}
	screen_mutex = SDL_CreateMutex();
	if (!screen_mutex) 
	{
  		fprintf(stderr, "Couldn't create mutex\n");
  		exit(1);
	}
	av_strlcpy(is->filename, argv[1], sizeof(is->filename));
		

	is->pictq_mutex = SDL_CreateMutex();
	is->pictq_cond = SDL_CreateCond();
	

	is->schedule_refresh(is, 0);
	is->av_sync_type = DEFAULT_AV_SYNC_TYPE;
	is->parse_tid = SDL_CreateThread(is->decode_thread, "audio_video_thread", is);
	if(!is->parse_tid) 
	{
		av_free(is);
		return -1;
	}
	av_init_packet(&flush_pkt);
  	//flush_pkt.data = "FLUSH";
	for(;;)
	{
		double pos, incr;
		
		SDL_WaitEvent(&event);
		switch(event.type) 
		{
			case SDL_KEYDOWN:
						switch(event.key.keysym.scancode)
						{
							case SDL_SCANCODE_LEFT:	
										SDL_PauseAudio(1);
										video_stop = 1;
										incr = -10.00;
										goto do_seek;
										
										
							case SDL_SCANCODE_RIGHT:
										SDL_PauseAudio(1);
										video_stop = 1;
										incr = 10.00;
										goto do_seek;
							case SDL_SCANCODE_UP:
										SDL_PauseAudio(1);
										video_stop = 1;
										incr = 30.00;
										goto do_seek;
							case SDL_SCANCODE_DOWN:
										SDL_PauseAudio(1);
										video_stop = 1;
										incr = -30.00;
										goto do_seek;
										
							case SDL_SCANCODE_SPACE:
								pause_count++;
								if(pause_count%2!=0)
								{
									SDL_PauseAudio(1);
									video_stop = 1;
								}
								else if(pause_count%2==0)
								{
									SDL_PauseAudio(0);
									video_stop = 0;
									//schedule_refresh(is, 0);
								}
								break;
							do_seek:
								if(is) 
								{
	  								pos = is->get_master_clock(is);
	  								pos += incr;
	  								is->stream_seek(is, (int64_t)(pos * AV_TIME_BASE), incr);
								}
								break;
							default:
									break;
						}
						break;
			case SDL_MOUSEBUTTONDOWN:
						switch(event.button.button)
						{
							case SDL_BUTTON_LEFT:
								pause_count++;
								if(pause_count%2!=0)
								{
									SDL_PauseAudio(1);
									video_stop = 1;
								}
								else if(pause_count%2==0)
								{
									SDL_PauseAudio(0);
									video_stop = 0;
									//schedule_refresh(is, 0);
								}
								break;
							default:
								break;
						}
						break;
			case SDL_MOUSEMOTION:
						
						if(event.motion.y>500)
							bar_visibility = 1;
						else
							bar_visibility = 0;
						break;	
			case FF_QUIT_EVENT:
			case SDL_QUIT:
						is->quit = 1;
						SDL_Quit();
						exit (0);
						break;
			case FF_ALLOC_EVENT:
						is->alloc_picture(event.user.data1);
						break;
			case FF_REFRESH_EVENT:
						is->video_refresh_timer(event.user.data1);
						break;
			default:
						break;
		}
	}
	return 0;
}

