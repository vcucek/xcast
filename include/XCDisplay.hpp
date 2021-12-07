 
#ifndef __XC_DISPLAY_H__
#define __XC_DISPLAY_H__ 1

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define XCD_IMAGE_BGRA		1
#define XCD_IMAGE_YUV420	2

using namespace std;

struct XCInputEvent {
    int type;
    int key_code;
    int state;
    int mouse_x, mouse_y;
};

class XCDisplay{

protected:

	static XCDisplay* current;

public:

	EGLDisplay  egl_display;
	EGLContext  egl_context;
	EGLSurface  egl_surface;

	bool window_created;
	bool window_egl_created;
	bool window_decoder_created;

	//set these values in derived classes
	int image_buffer_type;
	int image_buffer_width, image_buffer_height;
	int image_buffer_bpp;

	//allocated here
	int image_buffer_size;
	uint8_t* image_buffer;
	
	XCDisplay(){
		window_created = false;
		window_egl_created = false;
		window_decoder_created = false;
	};
	virtual ~XCDisplay(){
		current = NULL;
	};

	//virtual methods
	virtual void size(int* width, int* height) =0;
	virtual void capture() =0;

	virtual bool window_create() =0;
	virtual void window_dispose() =0;

	//feed data to decoder
	virtual bool window_decoder_init(int width, int height) =0;
	virtual void window_decoder_data(unsigned char* data, int data_size) =0;
	virtual void window_decoder_dispose() =0;

	//collect or wait for the input event
	//virtual void collect_input(XCInputEvent *event) =0;

	//egl methods
	virtual bool window_egl_init() =0;
	void window_egl_makeCurrent();
	bool window_egl_isCurrent();
	void window_egl_size(int* width, int* height);
	void window_egl_swapBuffers();
};


extern XCDisplay* createDisplay();

#endif

