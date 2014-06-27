#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>

#include "misc.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/mathematics.h"

#undef exit

//#define STREAM_FRAME_RATE 24
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */
//#define STREAM_PIX_FMT PIX_FMT_YUVJ422P /* default pix_fmt */

//static int sws_flags = SWS_BICUBIC;

/**************************************************************/
/* audio output */

int16_t *samples;
uint8_t *audio_outbuf;
int audio_outbuf_size;
int audio_input_frame_size;



/**************************************************************/
/* video output */

AVFrame *picture, *tmp_picture;
uint8_t *video_outbuf;
int frame_count, video_outbuf_size;

/* add a video output stream */
static AVStream *add_video_stream(AVFormatContext *oc, enum CodecID codec_id,
	int width, int height, float framerate)
{
AVCodecContext *c;
AVStream *st;

	st = av_new_stream(oc, 0);
	if (!st) {
		fprintf(stderr, "Could not alloc stream\n");
		exit(1);
	}

	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = AVMEDIA_TYPE_VIDEO;

	c->bit_rate=8*10 * 100000;
	c->time_base.den=framerate*100;
	c->time_base.num=100;
	c->gop_size=24;
	c->height=height;
	c->width=width;
	c->max_b_frames=5; //vmax_b_frames=0
	c->max_qdiff=4; //vqdiff=4
	c->me_range=16; // me_range=16
	c->qmin=10; // vqmin=10
	c->qmax=69; // vqmax=31
	c->qcompress=0.6; //vqcomp=0.6
	c->keyint_min=10; //keyint=10
	c->trellis=0;
	c->level=FF_LEVEL_UNKNOWN;
	c->profile=FF_PROFILE_H264_MAIN;
	c->me_method=ME_UMH;//7;//ME_EPZS;
	c->thread_count=2;
	c->qblur=0.5;
	c->pix_fmt=STREAM_PIX_FMT; //YUV420P
	c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	st->sample_aspect_ratio = c->sample_aspect_ratio = av_d2q(1.0, 255);

	return st;
}

static AVFrame *alloc_picture(enum PixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;

	picture = avcodec_alloc_frame();
	if (!picture)
		return NULL;
	size = avpicture_get_size(pix_fmt, width, height);
	picture_buf = av_malloc(size);
	if (!picture_buf) {
		av_free(picture);
		return NULL;
	}
	avpicture_fill((AVPicture *)picture, picture_buf,
				   pix_fmt, width, height);
	return picture;
}

static void open_video(AVFormatContext *oc, AVStream *st)
{
	AVCodec *codec;
	AVCodecContext *c;

	c = st->codec;

	/* find the video encoder */
	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		fprintf(stderr, "codec not found\n");
		exit(1);
	}

	/* open the codec */
	if (avcodec_open(c, codec) < 0) {
		fprintf(stderr, "could not open codec\n");
		exit(1);
	}

	video_outbuf = NULL;
	if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
		/* allocate output buffer */
		/* XXX: API change will be done */
		/* buffers passed into lav* can be allocated any way you prefer,
		   as long as they're aligned enough for the architecture, and
		   they're freed appropriately (such as using av_free for buffers
		   allocated with av_malloc) */
		video_outbuf_size = 2000000;
		video_outbuf = av_malloc(video_outbuf_size);
	}

	/* allocate the encoded raw picture */
	picture = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!picture) {
		fprintf(stderr, "Could not allocate picture\n");
		exit(1);
	}

	/* if the output format is not YUV420P, then a temporary YUV420P
	   picture is needed too. It is then converted to the required
	   output format */
	tmp_picture = NULL;
	if (c->pix_fmt != PIX_FMT_YUV420P) {
		tmp_picture = alloc_picture(PIX_FMT_YUV420P, c->width, c->height);
		if (!tmp_picture) {
			fprintf(stderr, "Could not allocate temporary picture\n");
			exit(1);
		}
	}
}

void write_video_frame(AVFormatContext *oc, AVStream *st,
	struct myframe *fr, int pts)
{
	int out_size, ret;
	AVCodecContext *c;

	c = st->codec;

	picture->pts = pts;
	if (c->pix_fmt == PIX_FMT_YUVJ422P)
	{
		int i, j, h;
		for(i=0;i<3;++i)
		{
			int inwide, outwide;
			h = c->height;
			if(i) h>>=1;
			inwide = fr->linesize[i];
			outwide = picture->linesize[i];
			for(j=0;j<h;++j)
				memcpy(picture->data[i] + j*outwide,
					fr->data[i] + j*inwide, outwide);
		}
//#if 0
//	static struct SwsContext *img_convert_ctx = 0;
//			/* as we only generate a YUV420P picture, we must convert it
//			   to the codec pixel format if needed */
//		if (img_convert_ctx == NULL)
//		{
//			img_convert_ctx = sws_getContext(c->width, c->height,
//								 PIX_FMT_YUV420P,
//								 c->width, c->height,
//								 c->pix_fmt,
//								 sws_flags, NULL, NULL, NULL);
//			if (img_convert_ctx == NULL)
//			{
//				fprintf(stderr, "Cannot initialize the conversion context\n");
//				exit(1);
//			}
//		}
//		sws_scale(img_convert_ctx, tmp_picture->data, tmp_picture->linesize,
//					  0, c->height, picture->data, picture->linesize);
//#else
//		printf("Can't handle image format...\n");
//		exit(0);
//#endif
	} else if(c->pix_fmt == PIX_FMT_YUV420P)
	{
		int i, j, h;
		for(i=0;i<3;++i)
		{
			int inwide, outwide;
			h = c->height;
			if(i) h>>=1;
			inwide = fr->linesize[i];
			outwide = picture->linesize[i];
			for(j=0;j<h;++j)
				memcpy(picture->data[i] + j*outwide,
					fr->data[i] + j*inwide, outwide);
		}
	} else
	{
		printf("epicfail: Can't handle image format...\n");
		exit(0);
	}

	if (oc->oformat->flags & AVFMT_RAWPICTURE)
	{
		/* raw video case. The API will change slightly in the near
		   futur for that */
		AVPacket pkt;
		av_init_packet(&pkt);

		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index= st->index;
		pkt.data= (uint8_t *)picture;
		pkt.size= sizeof(AVPicture);

		ret = av_interleaved_write_frame(oc, &pkt);
	} else
	{
		/* encode the image */
		out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);
		/* if zero size, it means the image was buffered */
		if (out_size > 0)
		{
			AVPacket pkt;
			av_init_packet(&pkt);

			if (c->coded_frame->pts != AV_NOPTS_VALUE)
				pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
			if(c->coded_frame->key_frame)
				pkt.flags |= AV_PKT_FLAG_KEY;
			pkt.stream_index= st->index;
			pkt.data= video_outbuf;
			pkt.size= out_size;

			/* write the compressed frame in the media file */
			ret = av_interleaved_write_frame(oc, &pkt);
		} else
		{
			ret = 0;
		}
	}
	if (ret != 0)
	{
		fprintf(stderr, "Error while writing video frame\n");
		exit(1);
	}
	frame_count++;
}

static void close_video(AVFormatContext *oc, AVStream *st)
{
	avcodec_close(st->codec);
	av_free(picture->data[0]);
	av_free(picture);
	if (tmp_picture) {
		av_free(tmp_picture->data[0]);
		av_free(tmp_picture);
	}
	av_free(video_outbuf);
}

/**************************************************************/
/* media file output */

int encode_my_video(char *filename, int width, int height, int numframes,
	float framerate)
{
AVOutputFormat *fmt;
AVFormatContext *oc;
AVStream *video_st;
int i;
int framecount;
struct myframe aframe;


	memset(&aframe, 0, sizeof(aframe));
	aframe.width = width;
	aframe.height = height;
	aframe.linesize[0] = width;
	aframe.linesize[1] = aframe.linesize[2] = width>>1;
	aframe.data[0] = malloc(width*height);
	aframe.data[1] = malloc(width*height>>1);
	aframe.data[2] = malloc(width*height>>1);

	/* initialize libavcodec, and register all codecs and formats */
	av_register_all();

	if(0) // list all formats
	{
		fmt = 0;
		for(;;)
		{
			fmt = av_oformat_next(fmt);
			if(!fmt) break;
			printf("name %s, long_name %s, extensions %s\n",
				fmt->name, fmt->long_name, fmt->extensions);
		}
	}
	/* auto detect the output format from the name. default is
	   mpeg. */
	fmt = av_guess_format("mp4", NULL, NULL);
//	fmt = av_guess_format("h264", NULL, NULL);
//	fmt = av_guess_format("mjpeg", NULL, NULL);
	if (!fmt) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		fmt = av_guess_format("mpeg", NULL, NULL);
	}
	if (!fmt) {
		fprintf(stderr, "Could not find suitable output format\n");
		exit(1);
	}

	/* allocate the output media context */
	oc = avformat_alloc_context();
	if (!oc) {
		fprintf(stderr, "Memory error\n");
		exit(1);
	}
	oc->oformat = fmt;
	snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

	/* add the audio and video streams using the default format codecs
	   and initialize the codecs */
	video_st = NULL;

	if (fmt->video_codec != CODEC_ID_NONE) {
//		video_st = add_video_stream(oc, fmt->video_codec, width, height, framerate);
		video_st = add_video_stream(oc, CODEC_ID_H264, width, height, framerate);
	}

	/* set the output parameters (must be done even if no
	   parameters). */
	if (av_set_parameters(oc, NULL) < 0) {
		fprintf(stderr, "Invalid output format parameters\n");
		exit(1);
	}

	dump_format(oc, 0, filename, 1);

	/* now that all the parameters are set, we can open the audio and
	   video codecs and allocate the necessary encode buffers */
	if (video_st)
		open_video(oc, video_st);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
			fprintf(stderr, "Could not open '%s'\n", filename);
			exit(1);
		}
	}

	/* write the stream header, if any */
	av_write_header(oc);

	for(framecount=0;framecount<numframes;++framecount) {
		/* compute current video time */

		AVCodecContext *c;

		c = video_st->codec;
		aframe.pts = framecount * (double)c->time_base.num / c->time_base.den;
		draw_frame(&aframe);
		write_video_frame(oc, video_st, &aframe, framecount);
	}

	/* write the trailer, if any.  the trailer must be written
	 * before you close the CodecContexts open when you wrote the
	 * header; otherwise write_trailer may try to use memory that
	 * was freed on av_codec_close() */
	av_write_trailer(oc);

	/* close each codec */
	if (video_st)
		close_video(oc, video_st);

	/* free the streams */
	for(i = 0; i < oc->nb_streams; i++) {
		av_freep(&oc->streams[i]->codec);
		av_freep(&oc->streams[i]);
	}

	if (!(fmt->flags & AVFMT_NOFILE)) {
		/* close the output file */
		url_fclose(oc->pb);
	}

	/* free the stream */
	av_free(oc);

	return 0;
}
