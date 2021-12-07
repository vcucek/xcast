
#include "XCDisplay.hpp"
#include "bcm_host.h"
#include "codec/RaspPlayer.hpp"


class XCDisplay_rasp: public XCDisplay{
	
	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	
	EGL_DISPMANX_WINDOW_T nativewindow;
    DISPMANX_TRANSFORM_T transform;
	DISPMANX_RESOURCE_HANDLE_T image; 
	
	RaspPlayer* video_player;
	
	bool window_created;
	bool window_egl_created;
	
public:
	
	XCDisplay_rasp();
	~XCDisplay_rasp();
	
	bool window_create();
	void window_dispose();
	void capture();
	void size(int* width, int* height);
	
	bool window_egl_init();
	
	bool window_decoder_init(int width, int height);
	void window_decoder_data(unsigned char* data, int data_size);
	void window_decoder_dispose();
};


XCDisplay* createDisplay(){
	return new XCDisplay_rasp();
}

XCDisplay_rasp::XCDisplay_rasp(){
	
	bcm_host_init();
	
	window_created = false;
	window_egl_created = false;
	
	size(&image_buffer_width, &image_buffer_height);
	image_buffer_bpp = 4;
	image_buffer_used = image_buffer_width * image_buffer_bpp * image_buffer_height;
	image_buffer_type = XCD_IMAGE_YUV420;
	
	
	uint32_t native_image_handle = 0;
	image = vc_dispmanx_resource_create(VC_IMAGE_YUV420,
										(uint32_t)image_buffer_width, (uint32_t)image_buffer_height,
										&native_image_handle);
	/*
	VC_RECT_T rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = image_buffer_width;
	rect.height = image_buffer_height;
	vc_dispmanx_resource_write_data(image, VC_IMAGE_YUV420, 0, image_buffer, &rect);
	*/
	/*
	if(!native_image_handle){
		cerr << "Native image handle can not be obtained" << endl;
		exit(1);
	}
	
	cout << "Native image handle addr: " << native_image_handle << endl;
	image_buffer = (unsigned char*)native_image_handle;
	*/
}

XCDisplay_rasp::~XCDisplay_rasp(){
	
	if(window_created){
        //TODO: dispose/close window
	}
	 
	vc_dispmanx_resource_delete(image);
	bcm_host_deinit();
}

void XCDisplay_rasp::capture(){

	int success = vc_dispmanx_snapshot(dispman_display, image, transform);
	if ( success < 0 ){
		cerr << "Snapshot can not be obtained";
		exit(1);
	}
}

void XCDisplay_rasp::size(int* width, int* height){
	
	int success = graphics_get_display_size(0 /* LCD */, (unsigned int*)width, (unsigned int*)height);
	if ( success < 0 ){
		cerr << "Display size can not be retrieved";
		exit(1);
	}
}


bool XCDisplay_rasp::window_create(){
	
	if(window_created){
		cout << "Window already created" << endl;
		return true;
	}
	
	cout << "Creating new window" << endl;
	
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	
	uint32_t display_width;
	uint32_t display_height;
	
	// create an EGL window surface, passing context width/height
	int success = graphics_get_display_size(0 /* LCD */, &display_width, &display_height);
	if ( success < 0 )
	{
		cerr << "Display size can not be retrieved";
		exit(1);
	}
	
	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = display_width;
	dst_rect.height = display_height;
	
	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = display_width << 16;
	src_rect.height = display_height << 16;
	
	dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
	dispman_update = vc_dispmanx_update_start( 0 );
	
	
	dispman_element = vc_dispmanx_element_add ( dispman_update, 
												dispman_display, 0/*layer*/, &dst_rect, 
												0/*src*/, &src_rect, 
												DISPMANX_PROTECTION_NONE,
												0 /*alpha*/, 0/*clamp*/, transform);
	
	nativewindow.element = dispman_element;
	nativewindow.width = display_width;
	nativewindow.height = display_height;
	vc_dispmanx_update_submit_sync( dispman_update );
	
	window_created = true;
	return true;
}

bool XCDisplay_rasp::window_egl_init(){
	
	if(!window_created){
		cout << "Can't initialize EGL! Window not created!" << endl;
		return false;
	}
	
	if(window_egl_created){
		cout << "EGL is already initialized" << endl;
		return true;
	}
	
	egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if ( egl_display == EGL_NO_DISPLAY ) {
		cerr << "Got no EGL display." << endl;
		return false;
	}
	
	if ( !eglInitialize( egl_display, NULL, NULL ) ) {
		cerr << "Unable to initialize EGL" << endl;
		return false;
	}
	
	EGLint attr[] = {       // some attributes to set up our egl-interface
		EGL_BUFFER_SIZE, 16,
		EGL_RENDERABLE_TYPE,
		EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	
	EGLConfig  ecfg;
	EGLint     num_config;
	if ( !eglChooseConfig( egl_display, attr, &ecfg, 1, &num_config ) ) {
		cerr << "Failed to choose config (eglError: " << eglGetError() << ")" << endl;
		return false;
	}
	
	if ( num_config != 1 ) {
		cerr << "Didn't get exactly one config, but " << num_config << endl;
		return false;
	}
	
	egl_surface = eglCreateWindowSurface(egl_display, ecfg, (EGLNativeWindowType)(&nativewindow), NULL);
	if ( egl_surface == EGL_NO_SURFACE ) {
		cerr << "Unable to create EGL surface (eglError: " << eglGetError() << ")" << endl;
		return false;
	}
	
	//// egl-contexts collect all state descriptions needed required for operation
	EGLint ctxattr[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	egl_context = eglCreateContext ( egl_display, ecfg, EGL_NO_CONTEXT, ctxattr );
	if ( egl_context == EGL_NO_CONTEXT ) {
		cerr << "Unable to create EGL context (eglError: " << eglGetError() << ")" << endl;
		return false;
	}
	
	window_egl_makeCurrent();
	window_egl_created = true;
	return true;
}

bool XCDisplay_rasp::window_decoder_init(int width, int height){
	
	if(!window_created){
		cout << "Can't initialize EGL! Window not created!" << endl;
		return false;
	}
	
	video_player = new RaspPlayer();
	
	window_decoder_created = true;
	return true;
}

void XCDisplay_rasp::window_decoder_data(unsigned char* data, int data_size){
	
	if(!window_decoder_created){
		cerr << "Can't decode data. Decoder not initialized" << endl;
		return;
	}
	
	video_player->data(data, data_size);
}

void XCDisplay_rasp::window_decoder_dispose(){

	delete video_player;
	window_decoder_created = false;
}

void XCDisplay_rasp::window_dispose(){
	
	if(!window_created){
		return;
	}
	
	cout << "Deleting window" << endl;
	
	if(window_decoder_created){
		window_decoder_dispose();
	}
	
	if(window_egl_created){
		eglMakeCurrent( egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
		
		eglDestroyContext ( egl_display, egl_context );
		eglDestroySurface ( egl_display, egl_surface );
		eglTerminate      ( egl_display );
		
		egl_display = NULL;
		egl_context = NULL;
		egl_surface = NULL;
		window_egl_created = false;
	}
	
	window_created = false;
}
