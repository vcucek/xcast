 
#include "XCDisplay.hpp"


XCDisplay* XCDisplay::current = NULL;

void XCDisplay::window_egl_makeCurrent(){
	if(!egl_display){
		cerr << "XCDisplay::window_makeCurrent(): Window is not created! " << endl;
		return;
	}
	
	eglMakeCurrent( egl_display, egl_surface, egl_surface, egl_context );
	XCDisplay::current = this;
}

bool XCDisplay::window_egl_isCurrent(){
	return (XCDisplay::current == this);
}

void XCDisplay::window_egl_size(int* width, int* height){
	
	if(!egl_display || !egl_surface){
		*width = 0;
		*height = 0;
		cerr << "XCDisplay::window_size(): Window is not created! Window size is 0" << endl;
		return;
	}
	
	EGLint w,h;
	eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &w);
	eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &h);
	
	*width = (int)w;
	*height = (int)h;
}

void XCDisplay::window_egl_swapBuffers(){
	
	if(!egl_display || !egl_surface){
		cerr << "XCDisplay::window_swapBuffers(): Window is not created!" << endl;
		return;
	}
	eglSwapBuffers ( egl_display, egl_surface );  // get the rendered buffer to the screen
}