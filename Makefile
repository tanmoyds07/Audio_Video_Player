#Creating variable:
cc=g++

CFLAGS= -Wall -O2 -g

all: audio_video_render

audio_video_render: main.cpp
		$(cc) $(CFLAGS) main.cpp -o audio_video_render -lavcodec -lavformat -lavutil -lswscale -lswresample -lz -lm `sdl2-config --cflags --libs` -I ./

player.o: player.cpp
		$(cc) -c player.cpp -lavcodec -lavformat -lavutil -lswscale -lswresample -lz -lm `sdl2-config --cflags --libs` -I ./

library.o: player.h
		$(cc) -c player.h -lavcodec -lavformat -lavutil -lswscale -lswresample -lz -lm `sdl2-config --cflags --libs`



clean:
	rm -rf *o audio_video_render

#To create live stream run the following commands
#ffmpeg -i input.mp4 -hls_time 10 -hls_list_size 0 output.m3u8
#./audio_video_render output.m3u8
