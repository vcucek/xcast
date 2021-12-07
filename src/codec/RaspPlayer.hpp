 
#ifndef __XC_CODEC_RASPPLAYER_H__
#define __XC_CODEC_RASPPLAYER_H__ 1

#include <iostream>
#include <cstdlib>
#include <cstring>

extern "C" {
	#include "bcm_host.h"
	#include "ilclient.h"
}

using namespace std;

class RaspPlayer{
	
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
	OMX_BUFFERHEADERTYPE *buf;
	
	COMPONENT_T* video_decode;
	COMPONENT_T* video_scheduler;
	COMPONENT_T* video_render;
	COMPONENT_T* clock;
	
	COMPONENT_T* list[5];
	
	TUNNEL_T tunnel[4];
	ILCLIENT_T* client;
	
	int status;
	unsigned int data_len;
	
	int port_settings_changed;
	int first_packet;
		
	bool executing;
	
public:
	RaspPlayer();
	~RaspPlayer();
	
	void data(unsigned char* data, int data_size);
};


#endif