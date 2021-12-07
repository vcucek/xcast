
#ifndef __XC_CODEC_AV_H__
#define __XC_CODEC_AV_H__ 1

#include "common.hpp"

extern "C" {

#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

static AVCodecContext* context = NULL;
static AVCodec *codec;

static AVFrame* frame = NULL;
static AVFrame* frame_bgra = NULL;

static SwsContext* swsEncode = NULL; 
static SwsContext* swsDecode = NULL; 

static int frame_count = 0;

#define CODEC_ID AV_CODEC_ID_H264
#define PIX_FMT AV_PIX_FMT_YUV420P

//register codecs

#define REGISTER_HWACCEL(X, x)                                      \
    {                                                               \
        extern AVHWAccel ff_##x##_hwaccel;                          \
        av_register_hwaccel(&ff_##x##_hwaccel);                     \
    }

#define REGISTER_ENCODER(X, x)                                      \
    {                                                               \
        extern AVCodec ff_##x##_encoder;                            \
        avcodec_register(&ff_##x##_encoder);                        \
    }

#define REGISTER_DECODER(X, x)                                      \
    {                                                               \
        extern AVCodec ff_##x##_decoder;                            \
        avcodec_register(&ff_##x##_decoder);                        \
    }

#define REGISTER_ENCDEC(X, x) REGISTER_ENCODER(X, x); REGISTER_DECODER(X, x)

#define REGISTER_PARSER(X, x)                                       \
    {                                                               \
        extern AVCodecParser ff_##x##_parser;                       \
        av_register_codec_parser(&ff_##x##_parser);                 \
    }

static void video_codec_register(){
	
#ifdef SYSTEM_RASP
	avcodec_register_all();
#else
	avcodec_register_all();
	/*
    REGISTER_HWACCEL(MPEG4_VAAPI,       mpeg4_vaapi);
    REGISTER_HWACCEL(MPEG4_VDPAU,       mpeg4_vdpau);
	
    REGISTER_ENCDEC (MPEG4,             mpeg4);
    //REGISTER_DECODER(MPEG4_CRYSTALHD,   mpeg4_crystalhd);
    REGISTER_DECODER(MPEG4_VDPAU,       mpeg4_vdpau);
    REGISTER_PARSER(MPEG4VIDEO,         mpeg4video);
    */
#endif
	
	/*
	REGISTER_HWACCEL(H263_VAAPI,        h263_vaapi);
	REGISTER_HWACCEL(H263_VDPAU,        h263_vdpau);
	REGISTER_ENCDEC (H263P,             h263p);
	REGISTER_PARSER(H263,               h263);
	
    REGISTER_ENCDEC (ZLIB,              zlib);
	
	REGISTER_HWACCEL(MPEG1_XVMC,        mpeg1_xvmc);
	REGISTER_HWACCEL(MPEG1_VDPAU,       mpeg1_vdpau);
	REGISTER_HWACCEL(MPEG2_XVMC,        mpeg2_xvmc);
	//REGISTER_HWACCEL(MPEG2_DXVA2,       mpeg2_dxva2);
	REGISTER_HWACCEL(MPEG2_VAAPI,       mpeg2_vaapi);
	REGISTER_HWACCEL(MPEG2_VDPAU,       mpeg2_vdpau);
	REGISTER_HWACCEL(MPEG4_VAAPI,       mpeg4_vaapi);
	REGISTER_HWACCEL(MPEG4_VDPAU,       mpeg4_vdpau);
	REGISTER_ENCDEC (JPEG2000,          jpeg2000);
	REGISTER_PARSER(MJPEG,              mjpeg);
	*/
}

static void video_frame_copy(unsigned char* in, int in_rowsize, 
							 unsigned char* out, int out_rowsize, int count){
	
	for(int h=0; h<count; h++){
		memcpy(out, in, out_rowsize);
		in += in_rowsize;
		out += out_rowsize;
	}
}

static void video_init_encode(int width, int height){
	
	/* find the mpeg1 video encoder */
	codec = avcodec_find_encoder(CODEC_ID);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}
	
	context = avcodec_alloc_context3(codec);
	context->width = width;
	context->height = height;
	context->time_base = (AVRational){1,25};
	context->framerate = (AVRational){25, 1};

	context->pix_fmt = PIX_FMT;

    AVDictionary *d = NULL;
    av_dict_set(&d, "vprofile", "baseline", 0);
    av_dict_set(&d, "tune", "zerolatency", 0);
    av_dict_set(&d, "preset","ultrafast", 0);

	/* open it */
	if (avcodec_open2(context, codec, &d) < 0) {
		for(int i=0; i<sizeof(codec->pix_fmts)/sizeof(AV_PIX_FMT_NONE); i++){
			printf("SUPPORTED PIX_FMT: %i \n", codec->pix_fmts[i]);
		}
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}
	
	//allocate frame
	frame = av_frame_alloc();
	frame->format = context->pix_fmt;
	frame->width  = context->width;
	frame->height = context->height;

	av_image_alloc(frame->data, frame->linesize,
			context->width, context->height, context->pix_fmt, 1);

	
	swsEncode = sws_getContext(context->width, context->height, AV_PIX_FMT_BGRA,
			context->width, context->height, context->pix_fmt,
			SWS_POINT, NULL, NULL, NULL);
}

static void video_encode(uint8_t* in, int in_size, uint8_t* out, int& out_size){
	
	//allocate package
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;
	
	out_size = 0;
	
	uint8_t *inData[1]     = { in };
	int      inLinesize[1] = { 4 * context->width };
	
	sws_scale(swsEncode, inData, inLinesize, 0, context->height, frame->data, frame->linesize);

    //print_buffer("Scaled frame:", *frame->data, *frame->linesize, *frame->linesize);
	
	frame->pts = frame_count;
	frame_count++;

    avcodec_send_frame(context, frame);
    avcodec_receive_packet(context, &packet);
    memcpy(out, packet.data, packet.size);
    out_size = packet.size;
    av_free_packet(&packet);
}

static void video_init_decode(int width, int height){
	
	codec = avcodec_find_decoder(CODEC_ID);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	context = avcodec_alloc_context3(codec);
	if (avcodec_open2(context, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}
	
	frame = av_frame_alloc();
	
	frame_bgra = av_frame_alloc();
	frame_bgra->format = AV_PIX_FMT_BGRA;
	frame_bgra->width  = width;
	frame_bgra->height = height;
	
	av_image_alloc(frame_bgra->data, frame_bgra->linesize,
			width, height, AV_PIX_FMT_BGRA, 32);
	
	swsDecode = sws_getContext(width, height, PIX_FMT,
			width, height, AV_PIX_FMT_BGRA,
			SWS_POINT, NULL, NULL, NULL);
	if(sws_init_context(swsDecode, NULL, NULL) < 0) {
		fprintf(stderr, "Could not initialize sws context\n");
		exit(1);
    }
}

static void video_decode_yuv(uint8_t* in, const int in_size, 
		uint8_t* out, int out_size, int* out_used){ 
	
	int got_frame = 0;
	*out_used = 0;
	
	AVPacket packet;
	av_init_packet(&packet);
	packet.size = in_size;
	packet.data = in;

    avcodec_send_packet(context, &packet);
    avcodec_receive_frame(context, frame);

    //fill data
    int y_size = frame->width * frame->height;
    int u_size = frame->width * frame->height / 4;
    int v_size = frame->width * frame->height / 4;
    
    *out_used = y_size + u_size + v_size;
    
    if(*out_used > out_size){
        printf("Can't docode frame. Specified buffer to small: has %i bytes, decoded frame has %i bytes", out_size, *out_used);
    }
    else {
        video_frame_copy(frame->data[0], frame->linesize[0], out, frame->width, frame->height);
        video_frame_copy(frame->data[1], frame->linesize[1], out+y_size, frame->width/2, frame->height/2);
        video_frame_copy(frame->data[2], frame->linesize[2], out+y_size+u_size, frame->width/2, frame->height/2);
    }
    av_packet_unref(&packet);
}

static void video_decode_bgra(uint8_t* in, const int in_size, 
		uint8_t* out, int out_size, int* out_used){ 
	
	int got_frame = 0;
	*out_used = 0;
	
	AVPacket packet;
	av_init_packet(&packet);
	packet.size = in_size;
	packet.data = in;

    avcodec_send_packet(context, &packet);
    avcodec_receive_frame(context, frame);

    //convert
    sws_scale(swsDecode, 
            frame->data, frame->linesize,
            0, context->height,
            frame_bgra->data, frame_bgra->linesize);

    //set output line by line (workaround for the sws_scale problem)
    int linesize = 4*frame_bgra->width;

    unsigned char* iter_in  = frame_bgra->data[0];
    unsigned char* iter_out  = out;

	if(frame_bgra->height * linesize > out_size) {
    	*out_used = 0;
		av_packet_unref(&packet);
		return;
	}

    for(int i=0; i<frame_bgra->height; i++){
        memcpy(iter_out, iter_in, linesize);
        iter_in += linesize;
        iter_out += linesize;
    }
    *out_used = linesize * frame_bgra->height;
	av_packet_unref(&packet);
}

static void video_dispose(){

	if(frame){
		av_frame_free(&frame);
		frame = NULL;
	}
	if(frame_bgra){
		av_frame_free(&frame_bgra);
		frame_bgra = NULL;
	}
	if(swsEncode) {
		sws_freeContext(swsEncode);
		swsEncode = NULL;
	}
	if(swsDecode) {
		sws_freeContext(swsDecode);
		swsDecode = NULL;
	}
	av_free(context);
	frame_count = 0;
}

}

#endif
