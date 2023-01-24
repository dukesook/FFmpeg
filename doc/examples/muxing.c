/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define STREAM_DURATION   1
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_RGB24 

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *codec_ctx;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    AVPacket *tmp_pkt;

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

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_frame(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx,
                       AVStream *stream, AVFrame *frame, AVPacket *pkt)
{
    int ret;

    // send the frame to the encoder
    ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame to the encoder: %s\n", av_err2str(ret));
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding a frame: %s\n", av_err2str(ret));
            exit(1);
        }

        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;

        /* Write the compressed frame to the media file. */
        log_packet(fmt_ctx, pkt);
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        /* pkt is now blank (av_interleaved_write_frame() takes ownership of
         * its contents and resets pkt), so that no unreferencing is necessary.
         * This would be different if one used av_write_frame(). */
        if (ret < 0) {
            fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
            exit(1);
        }
    }

    return ret == AVERROR_EOF ? 1 : 0;
}

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       const AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *codec_ctx;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);

    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

    ost->tmp_pkt = av_packet_alloc();
    if (!ost->tmp_pkt) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        exit(1);
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    codec_ctx = avcodec_alloc_context3(*codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->codec_ctx = codec_ctx;


    codec_ctx->codec_id = codec_id;

    codec_ctx->bit_rate = 400000;
    /* Resolution must be a multiple of two. */
    codec_ctx->width    = 128;
    codec_ctx->height   = 128;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
        * of which frame timestamps are represented. For fixed-fps content,
        * timebase should be 1/framerate and timestamp increments should be
        * identical to 1. */
    ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
    codec_ctx->time_base       = ost->st->time_base;

    codec_ctx->gop_size      = 12; /* emit one intra frame every twelve frames at most */
    codec_ctx->pix_fmt       = STREAM_PIX_FMT;
    if (codec_ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B-frames */
        codec_ctx->max_b_frames = 2;
    }
    if (codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
            * This does not happen with normal video, it just happens here as
            * the motion of the chroma plane does not match the luma plane. */
        codec_ctx->mb_decision = 2;
    }


    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

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
    ret = av_frame_get_buffer(picture, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return picture;
}

static void open_video(AVFormatContext *oc, const AVCodec *codec,
                       OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *codec_context = ost->codec_ctx;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(codec_context, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(codec_context->pix_fmt, codec_context->width, codec_context->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (codec_context->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, codec_context->width, codec_context->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, codec_context);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

static void fill_yuv_image(AVFrame *pict, int frame_index,
                           int width, int height)
{
    int x, y;
    int index = 0;
    const int CHANNEL_COUNT = 3;
    const int LINE_SIZE = pict->linesize[0]; //in bytes

    //PLANAR
    for (int band = 0; band < CHANNEL_COUNT; band++) {
        for (y = 0; y < height; y++)
            for (x = 0; x < width; x++) {
                switch (band) {
                case 0: //RED
                    pict->data[0][index++] = frame_index+1;
                    break;
                case 1: //GREEN
                    pict->data[0][index++] = 0xEE;
                    break;
                case 2: //BLUE
                    pict->data[0][index++] = 0xFF;
                    break;
                }
            }
    }

}

static AVFrame *get_video_frame(OutputStream *ost)
{
    AVCodecContext *c = ost->codec_ctx;

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) > 0)
        return NULL;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);


    //GET PIXEL DATA
    fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);

    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->codec_ctx);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    av_packet_free(&ost->tmp_pkt);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}


int main(int argc, char **argv) {
    OutputStream video_st = { 0 };
    const AVOutputFormat *fmt;
    const char *filename;
    AVFormatContext *av_fmt_ctx;
    const AVCodec *video_codec;
    int ret;
    int is_finished = 0;
    AVDictionary *opt = NULL;
    int i;

    //ARGUMENMTS
    filename = argv[1];
    for (i = 2; i+1 < argc; i+=2) {
        if (!strcmp(argv[i], "-flags") || !strcmp(argv[i], "-fflags"))
            av_dict_set(&opt, argv[i]+1, argv[i+1], 0);
    }


    //INITIALIZE FORMAT CONTEXT
    avformat_alloc_output_context2(&av_fmt_ctx, NULL, NULL, filename);
    if (!av_fmt_ctx) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&av_fmt_ctx, NULL, "mpeg", filename);
    }
    if (!av_fmt_ctx) return 1;
    fmt = av_fmt_ctx->oformat;


    //ADD STREAM - Add the video stream using the default format codecs and initialize the codecs.
    if (fmt->video_codec == AV_CODEC_ID_NONE) {
        printf("ERROR! - No codec found\n"); exit (1);
    }
    add_stream(&video_st, av_fmt_ctx, &video_codec, fmt->video_codec);


    /* Now that all the parameters are set, we can open the video codec and allocate the necessary encode buffers. */
    open_video(av_fmt_ctx, video_codec, &video_st, opt);
    av_dump_format(av_fmt_ctx, 0, filename, 1);


    //OPEN FILE
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&av_fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename,
                    av_err2str(ret));
            return 1;
        }
    }


    //WRITE HEADER
    ret = avformat_write_header(av_fmt_ctx, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return 1;
    }

    //PROCESS
    int nframe = 0;
    // generate pixel data & metadata
    while (!is_finished) {
        // encode_video = !write_video_frame(fmt_ctx, &video_st);
        AVFrame* frame = get_video_frame(&video_st);
        is_finished = write_frame(av_fmt_ctx, video_st.codec_ctx, video_st.st, frame, video_st.tmp_pkt);
        printf("wrote frame #%d\n", nframe++);
    }
    
    //WRITE TO DISK
    av_write_trailer(av_fmt_ctx);


    //CLOSE
    close_stream(av_fmt_ctx, &video_st);
    if (!(fmt->flags & AVFMT_NOFILE))
        avio_closep(&av_fmt_ctx->pb);
    avformat_free_context(av_fmt_ctx);

    printf("finished #4\n");
    return 0;
}
