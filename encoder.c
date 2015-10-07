#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "misc.h"

#include <math.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT	AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
} OutputStream;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		   av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		   av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		   av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		   pkt->stream_index);
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	log_packet(fmt_ctx, pkt);
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

struct videoinfo {
	int width, height;
	float framerate;
};

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
					   AVCodec **codec,
					   enum AVCodecID codec_id, struct videoinfo *vi)
{
	AVCodecContext *c;
	int i;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",
				avcodec_get_name(codec_id));
		exit(1);
	}

	ost->st = avformat_new_stream(oc, *codec);
	if (!ost->st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams-1;
	c = ost->st->codec;

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt  = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate	= 64000;
		c->sample_rate = 44100;
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}
		c->channels		= av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if ((*codec)->channel_layouts) {
			c->channel_layout = (*codec)->channel_layouts[0];
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels		= av_get_channel_layout_nb_channels(c->channel_layout);
		ost->st->time_base = (AVRational){ 1, c->sample_rate };
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;

		c->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		c->width	= vi->width;
		c->height   = vi->height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
		c->time_base	   = ost->st->time_base;

		c->gop_size	  = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt	   = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
	break;

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
								  uint64_t channel_layout,
								  int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		fprintf(stderr, "Error allocating an audio frame\n");
		exit(1);
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			fprintf(stderr, "Error allocating an audio buffer\n");
			exit(1);
		}
	}

	return frame;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;

	c = ost->st->codec;

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
		exit(1);
	}

	/* init signal generator */
	ost->t	 = 0;
	ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	ost->frame	 = alloc_audio_frame(c->sample_fmt, c->channel_layout,
									   c->sample_rate, nb_samples);
	ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
									   c->sample_rate, nb_samples);

	/* create resampler context */
		ost->swr_ctx = swr_alloc();
		if (!ost->swr_ctx) {
			fprintf(stderr, "Could not allocate resampler context\n");
			exit(1);
		}

		/* set options */
		av_opt_set_int	   (ost->swr_ctx, "in_channel_count",   c->channels,	   0);
		av_opt_set_int	   (ost->swr_ctx, "in_sample_rate",	 c->sample_rate,	0);
		av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",	  AV_SAMPLE_FMT_S16, 0);
		av_opt_set_int	   (ost->swr_ctx, "out_channel_count",  c->channels,	   0);
		av_opt_set_int	   (ost->swr_ctx, "out_sample_rate",	c->sample_rate,	0);
		av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",	 c->sample_fmt,	 0);

		/* initialize the resampling context */
		if ((ret = swr_init(ost->swr_ctx)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			exit(1);
		}
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
	AVFrame *frame = ost->tmp_frame;
	int j, i, v;
	int16_t *q = (int16_t*)frame->data[0];

	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
					  STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
		return NULL;

	for (j = 0; j <frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->st->codec->channels; i++)
			*q++ = v;
		ost->t	 += ost->tincr;
		ost->tincr += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts  += frame->nb_samples;

	return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame;
	int ret;
	int got_packet;
	int dst_nb_samples;

	av_init_packet(&pkt);
	c = ost->st->codec;

	frame = get_audio_frame(ost);

	if (frame) {
		/* convert samples from native format to destination codec format, using the resampler */
			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
											c->sample_rate, c->sample_rate, AV_ROUND_UP);
			av_assert0(dst_nb_samples == frame->nb_samples);

		/* when we pass a frame to the encoder, it may keep a reference to it
		 * internally;
		 * make sure we do not overwrite it here
		 */
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);

			/* convert to destination format */
			ret = swr_convert(ost->swr_ctx,
							  ost->frame->data, dst_nb_samples,
							  (const uint8_t **)frame->data, frame->nb_samples);
			if (ret < 0) {
				fprintf(stderr, "Error while converting\n");
				exit(1);
			}
			frame = ost->frame;

		frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
		ost->samples_count += dst_nb_samples;
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error while writing audio frame: %s\n",
					av_err2str(ret));
			exit(1);
		}
	}

	return (frame || got_packet) ? 0 : 1;
}

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->st->codec;
	AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
		exit(1);
	}

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	ost->tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!ost->tmp_frame) {
			fprintf(stderr, "Could not allocate temporary picture\n");
			exit(1);
		}
	}
}


/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;

	c = ost->st->codec;

	ost->frame->pts = ost->next_pts++;
	frame = ost->frame;

	if (oc->oformat->flags & AVFMT_RAWPICTURE) {
		/* a hack to avoid data copy with some raw video muxers */
		AVPacket pkt;
		av_init_packet(&pkt);

		if (!frame)
			return 1;

		pkt.flags		|= AV_PKT_FLAG_KEY;
		pkt.stream_index  = ost->st->index;
		pkt.data		  = (uint8_t *)frame;
		pkt.size		  = sizeof(AVPicture);

		pkt.pts = pkt.dts = frame->pts;
		av_packet_rescale_ts(&pkt, c->time_base, ost->st->time_base);

		ret = av_interleaved_write_frame(oc, &pkt);
	} else {
		AVPacket pkt = { 0 };
		av_init_packet(&pkt);

		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
		if (ret < 0) {
			fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
			exit(1);
		}

		if (got_packet) {
			ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		} else {
			ret = 0;
		}
	}

	if (ret < 0) {
		fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
		exit(1);
	}

	return (frame || got_packet) ? 0 : 1;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	avcodec_close(ost->st->codec);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */

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


	OutputStream video_st = { 0 }, audio_st = { 0 };
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVCodec *audio_codec, *video_codec;
	int ret;
	int have_video = 0, have_audio = 0;
	int encode_video = 0, encode_audio = 0;
	AVDictionary *opt = NULL;

	/* Initialize libavcodec, and register all codecs and formats. */
	av_register_all();

	if (0) {
		printf("usage: <prog> output_file\n"
			   "API example program to output a media file with libavformat.\n"
			   "This program generates a synthetic audio and video stream, encodes and\n"
			   "muxes them into a file named output_file.\n"
			   "The output format is automatically guessed according to the file extension.\n"
			   "Raw images can also be output by using '%%d' in the filename.\n"
			   "\n");
		return 1;
	}
//	if (argc > 3 && !strcmp(argv[2], "-flags")) {
//		av_dict_set(&opt, argv[2]+1, argv[3], 0);
//	}

	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	if (!oc)
		return 1;

	fmt = oc->oformat;

	struct videoinfo vi;
	vi.width = width;
	vi.height = height;
	vi.framerate = framerate;

	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		add_stream(&video_st, oc, &video_codec, fmt->video_codec, &vi);
		have_video = 1;
		encode_video = 1;
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec, 0);
		have_audio = 1;
		encode_audio = 1;
	}

	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if (have_video)
		open_video(oc, video_codec, &video_st, opt);

av_dict_set(&opt, "strict", "-2", 0);
	if (have_audio)
		open_audio(oc, audio_codec, &audio_st, opt);

	av_dump_format(oc, 0, filename, 1);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open '%s': %s\n", filename,
					av_err2str(ret));
			return 1;
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file: %s\n",
				av_err2str(ret));
		return 1;
	}

	int framecount = 0;
	while (encode_video /*|| encode_audio*/) {
		/* select the stream to encode */
		if (encode_video &&
			(!encode_audio || av_compare_ts(video_st.next_pts, video_st.st->codec->time_base,
											audio_st.next_pts, audio_st.st->codec->time_base) <= 0))
		{
			aframe.pts = (framecount/framerate);
			draw_frame(&aframe);
			AVFrame *frame = video_st.frame;
			int y;
			int w2 = width/2;
			for(y=0;y<height;++y)
			{
				int yoff = y*width;
				memcpy(frame->data[0] + yoff, aframe.data[0] + yoff, width);
				if(~y&1)
				{
					int y2 = y>>1;
					int uvoff = y2*w2;
					memcpy(frame->data[1] + uvoff, aframe.data[1] + uvoff, w2);
					memcpy(frame->data[2] + uvoff, aframe.data[2] + uvoff, w2);
				}
			}

			encode_video = !write_video_frame(oc, &video_st);
			++framecount;
			if(framecount >= numframes) encode_video = 0;
		} else
		{
			encode_audio = !write_audio_frame(oc, &audio_st);
		}
	}

	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(oc);

	/* Close each codec. */
	if (have_video)
		close_stream(oc, &video_st);
	if (have_audio)
		close_stream(oc, &audio_st);

	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&oc->pb);

	/* free the stream */
	avformat_free_context(oc);

	return 0;
}

