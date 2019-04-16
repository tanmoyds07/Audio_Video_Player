#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <math.h>

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

#ifndef PLAYER_H
#define PLAYER_H

using namespace std;

class PacketQueue;
class VideoPicture;
class VideoState;  

enum 
{
	AV_SYNC_AUDIO_MASTER,
	AV_SYNC_VIDEO_MASTER,
	AV_SYNC_EXTERNAL_MASTER,
};

SDL_Window *screen = NULL;
SDL_mutex       *screen_mutex;
SDL_Renderer *renderer = NULL;

/* Since we only have one decoding thread, the Big Struct
   can be global in case we need it. */
VideoState *global_video_state;
uint64_t global_video_pkt_pts = AV_NOPTS_VALUE;
AVPacket flush_pkt;

//Global variables to control pause features
int video_stop = 0;
int pause_count = 0;
int bar_visibility = 0;

class PacketQueue
{
	AVPacketList *first_pkt, *last_pkt;
	SDL_mutex *mutex;
	SDL_cond *cond;

	public:
	int nb_packets;
	int size;

	static void packet_queue_flush(PacketQueue *q);
	void packet_queue_init(PacketQueue *q);
	int packet_queue_put(PacketQueue *q, AVPacket *pkt);
	static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
};
class VideoPicture 
{
	public:
	SDL_Texture *texture;
	Uint8 *yPlane, *uPlane, *vPlane;
	size_t yPlaneSz, uvPlaneSz;
	int uvPitch;
	int width, height; /* source height & width */
	int allocated;
	double pts;
};
class VideoState : public PacketQueue, VideoPicture
{
	AVFormatContext *pFormatCtx;
	int             videoStream, audioStream;
	double          external_clock; /* external clock base */
	int64_t         external_clock_time;
	double          audio_clock;
	AVStream        *audio_st;
	AVCodecContext  *audio_ctx;
	PacketQueue     audioq;
	DECLARE_ALIGNED(16, uint8_t, audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2]);
	unsigned int    audio_buf_size;
	unsigned int    audio_buf_index;
	AVFrame         audio_frame;
	AVPacket        audio_pkt;
	uint8_t         *audio_pkt_data;
	int             audio_pkt_size;
	int             audio_hw_buf_size;
	double          audio_diff_cum; /* used for AV difference average computation */
  	double          audio_diff_avg_coef;
  	double          audio_diff_threshold;
  	int             audio_diff_avg_count;
	int             seek_req;
	int             seek_flags;
	int64_t         seek_pos;
	double          frame_timer;
	double          frame_last_pts;
	double          frame_last_delay;
	double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
	double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)
	int64_t         video_current_pts_time;  
	AVStream        *video_st;
	AVCodecContext  *video_ctx;
	PacketQueue     videoq;
	struct SwsContext *sws_ctx;
	VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
	int             pictq_size, pictq_rindex, pictq_windex;
	SDL_Thread      *video_tid;

	public:
	char            filename[1024];
	SDL_mutex       *pictq_mutex;
	SDL_cond        *pictq_cond;
	int             av_sync_type;
	SDL_Thread      *parse_tid;
	int             quit;

	static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque);
	static void schedule_refresh(VideoState *is, int delay);
	double get_video_clock(VideoState *is);
	double get_external_clock(VideoState *is);
	double get_audio_clock(VideoState *is);
	double get_master_clock(VideoState *is);
	int synchronize_audio(VideoState *is, short *samples, int samples_size, double pts);
	static int audio_resampling(AVCodecContext *audio_decode_ctx,
		                    AVFrame *audio_decode_frame,
		                    enum AVSampleFormat out_sample_fmt,
		                    int out_channels,
		                    int out_sample_rate,
		                    uint8_t *out_buf);
	int audio_decode_frame(VideoState *is, uint8_t *audio_buf, int buf_size, double *pts_ptr);
	static void audio_callback(void *userdata, Uint8 *stream, int len);
	void alloc_picture(void *userdata);
	int queue_picture(VideoState *is, AVFrame *pFrame, double pts);
	void video_display(VideoState *is);
	void video_refresh_timer(void *userdata);
	double synchronize_video(VideoState *is, AVFrame *src_frame, double pts);
	static int video_thread(void *arg);
	int stream_component_open(VideoState *is, int stream_index);
	static int decode_thread(void *arg) ;
	void stream_seek(VideoState *is, int64_t pos, int rel);
};      

#endif
