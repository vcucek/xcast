 
#include "XCDisplay.hpp"

#include <X11/X.h>
#include <X11/Xlib.h>

#include "codec/av.hpp"
#include "render/BGRARender.hpp"


class XCDisplay_X11: public XCDisplay {
	
	Display    *dis;
	Window      win_root;
	Window      win;
	
	XImage image;
	uint8_t* decoder_buffer;
	int decoder_buffer_size;
	
	BGRARender* bgra_render;

	int decoder_img_width;
	int decoder_img_height;
	
public:
	
	XCDisplay_X11();
	~XCDisplay_X11();
	
	void size(int* width, int* height);
	void capture();
	
	bool window_create();
	void window_dispose();
	
	bool window_decoder_init(int width, int height);
	void window_decoder_data(unsigned char* data, int data_size);
	void window_decoder_dispose();
	
	bool window_egl_init();
};


XCDisplay* createDisplay(){
    try {
        return new XCDisplay_X11();
    }
    catch (std::runtime_error err) {
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

XCDisplay_X11::XCDisplay_X11() 
	: XCDisplay(){

	bgra_render = NULL;

	if(!(dis = XOpenDisplay(NULL))){
        throw std::runtime_error("Opening connection to xserver failed!");
	}
	if(!(win_root = DefaultRootWindow(dis))){
        throw std::runtime_error("Opening xserver root window failed!");
	}

	size(&image_buffer_width, &image_buffer_height);

	image_buffer_bpp = 4;
	image_buffer_size = image_buffer_width*image_buffer_height*image_buffer_bpp;
	image_buffer = (uint8_t*)malloc(image_buffer_size);
	image_buffer_type = XCD_IMAGE_BGRA;

	// Clear image struct
	image = {0};

	image.width = image_buffer_width;
	image.height = image_buffer_height;
	image.bytes_per_line = image_buffer_width * image_buffer_bpp;
	image.bits_per_pixel = image_buffer_bpp*8;
	image.xoffset = 0;
	image.byte_order = LSBFirst;
	image.bitmap_bit_order = LSBFirst;
	image.bitmap_pad = 32;
	image.depth = 32;

	image.format = ZPixmap;
	image.data = (char*)image_buffer;

	if(!XInitImage(&image)) {
		free(image_buffer);
        throw std::runtime_error("Initializing xorg image failed!");
	}
}

XCDisplay_X11::~XCDisplay_X11(){

	cout << "Disposing window" << endl;
	if(window_created) {
		window_dispose();
	}
	cout << "Closing display" << endl;
	XCloseDisplay(dis);

	free(image_buffer);
}

void XCDisplay_X11::capture(){
	XGetSubImage(dis, win_root, 0, 0, image_buffer_width, image_buffer_height, AllPlanes, ZPixmap, &image, 0, 0);
}

void XCDisplay_X11::size(int* width, int* height){
	XWindowAttributes  gwa;
	XGetWindowAttributes ( dis , win_root , &gwa );
	*width = gwa.width;
	*height = gwa.height;
	
	//scale to multiple of 4
	if(gwa.width%4 != 0){
		*width = 4* (int)(gwa.width/4);
	}
	if(gwa.height%4 != 0){
		*height = 4* (int)(gwa.height/4);
	}
}

bool XCDisplay_X11::window_create(){
	
	if(window_created){
		cout << "Window already created" << endl;
		return true;
	}
	
	cout << "Creating new window" << endl;
	
	XWindowAttributes  root_gwa;
	XGetWindowAttributes ( dis , win_root, &root_gwa );
	
	win = XCreateSimpleWindow(dis, RootWindow(dis, 0), 0, 0, root_gwa.width, root_gwa.height,
								0, BlackPixel (dis, 0), BlackPixel(dis, 0));
	
	
	Atom wm_state = XInternAtom(dis, "_NET_WM_STATE", False);
	Atom fullscreen = XInternAtom(dis, "_NET_WM_STATE_FULLSCREEN", True);
	
	XEvent xev;
	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = fullscreen;
	xev.xclient.data.l[2] = 0;
	
	XMapWindow(dis, win);
	
	XSendEvent (dis, DefaultRootWindow(dis), False,
				SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	
	window_created = true;
	return true;
}

bool XCDisplay_X11::window_egl_init(){
	
	if(!window_created){
		cout << "Can't initialize EGL! Window not created!" << endl;
		return false;
	}
	
	if(window_egl_created){
		cout << "EGL is already initialized" << endl;
		return true;
	}
	
	egl_display  =  eglGetDisplay( (EGLNativeDisplayType) dis );
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
	
	egl_surface = eglCreateWindowSurface ( egl_display, ecfg, win, NULL );
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

bool XCDisplay_X11::window_decoder_init(int width, int height){

	if(!window_created){
		cout << "Can't initialize EGL! Window not created!" << endl;
		return false;
	}

	decoder_buffer = (uint8_t*)malloc(width*height*image_buffer_bpp);
	decoder_buffer_size = width*height*image_buffer_bpp;
	decoder_img_width = width;
	decoder_img_height = height;

	//create opengl context
	cout << "Creating BGRA renderer..." << endl;
	bgra_render = new BGRARender(this);
	bgra_render->texture_init(decoder_img_width, decoder_img_height);

	cout << "initializing video decoder..." << endl;
	video_init_decode(decoder_img_width, decoder_img_height);

	window_decoder_created = true;
	return true;
}

void XCDisplay_X11::window_decoder_data(unsigned char* data, int data_size){

	if(!window_decoder_created){
		cerr << "Can't decode data. Decoder not initialized" << endl;
		return;
	}

	image_buffer_type = XCD_IMAGE_BGRA;

	int image_used;
	video_decode_bgra(data, data_size, decoder_buffer, decoder_buffer_size, &image_used);

	if(image_used){
		bgra_render->texture_fill(decoder_buffer, 0, 0, decoder_img_width, decoder_img_height);
		bgra_render->render();
		window_egl_swapBuffers();
	}
}

void XCDisplay_X11::window_decoder_dispose(){
	if(window_decoder_created){
		video_dispose();
		delete bgra_render;
		window_decoder_created = false;
	}
}

void XCDisplay_X11::window_dispose(){
	
	if(!window_created){
		return;
	}

	if(window_decoder_created){
		cout << "Disposing decoder" << endl;
		window_decoder_dispose();
	}

	if(window_egl_created){
		cout << "Disposing EGL" << endl;
		eglMakeCurrent( egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	
		eglDestroyContext ( egl_display, egl_context );
		eglDestroySurface ( egl_display, egl_surface );
		eglTerminate      ( egl_display );
	
		egl_display = NULL;
		egl_context = NULL;
		egl_surface = NULL;
		window_egl_created = false;
	}

	cout << "Destroy X window" << endl;
	XDestroyWindow(dis, win);
	window_created = false;
}
