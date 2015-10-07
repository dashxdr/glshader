#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "misc.h"

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#include <libavutil/timestamp.h>

#include <libavformat/avformat.h>


int encode_my_video(char *filename, int width, int height, int numframes,
	float framerate)
{

	struct myframe aframe;
	memset(&aframe, 0, sizeof(aframe));
	aframe.width = width;
	aframe.height = height;
	aframe.linesize[0] = width;
	aframe.linesize[1] = aframe.linesize[2] = width>>1;
	aframe.data[0] = malloc(width*height);
	aframe.data[1] = malloc(width*height>>1);
	aframe.data[2] = malloc(width*height>>1);

	/* register all the codecs */
	avcodec_register_all();

	int codec_id = AV_CODEC_ID_H264;


	AVCodec *codec;
	AVCodecContext *c= NULL;
	int i, ret, got_output;
	FILE *f;
	AVFrame *frame;
	AVPacket pkt;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };

	printf("Encode video file %s\n", filename);

	/* find the mpeg1 video encoder */
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	/* put sample parameters */
	c->bit_rate = 900000;
	/* resolution must be a multiple of two */
	c->width = width;
	c->height = height;
	/* frames per second */
	c->time_base = (AVRational){100,(int)(framerate * 100)};
	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	c->gop_size = 250;
	c->max_b_frames = 5;
	c->pix_fmt = AV_PIX_FMT_YUV420P; // THIS IS IT!

	if (codec_id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	frame->format = c->pix_fmt;
	frame->width  = c->width;
	frame->height = c->height;

	/* the image can be allocated by any means and av_image_alloc() is
	 * just the most convenient way if av_malloc() is to be used */
	ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
						 c->pix_fmt, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate raw picture buffer\n");
		exit(1);
	}

	/* encode 1 second of video */
	for (i = 0; i < numframes; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL;	// packet data will be allocated by the encoder
		pkt.size = 0;

		fflush(stdout);
#if 1
		int x, y;
		/* prepare a dummy image */
		/* Y */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
			}
		}

		/* Cb and Cr */
		for (y = 0; y < c->height/2; y++) {
			for (x = 0; x < c->width/2; x++) {
				frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
				frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
			}
		}

		frame->pts = i * (double)c->time_base.num / c->time_base.den;
#else
		aframe.pts = frame->pts;
		draw_frame(&aframe);
#endif

		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		fflush(stdout);

		ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}

	/* add sequence end code to have a real mpeg file */
	fwrite(endcode, 1, sizeof(endcode), f);
	fclose(f);

	avcodec_close(c);
	av_free(c);
	av_freep(&frame->data[0]);
	av_frame_free(&frame);
	printf("\n");

	return 0;
}
