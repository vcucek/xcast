 
#ifndef __RENDER_BGRARENDER_H__
#define __RENDER_BGRARENDER_H__ 1

#include <iostream>
#include <cstdlib>
#include <GLES2/gl2.h>

#include "XCDisplay.hpp"

using namespace std;

class BGRARender{

	XCDisplay* display;
	
	GLuint textureID;

	GLint vertex_coord_loc;
	GLint texture_coord_loc;
	GLint texture_loc;
	
	GLint texture_width_loc;
	GLint texture_height_loc;

	GLint display_width_loc;
	GLint display_height_loc;

	GLint scale_loc;
	GLint position_loc;

	int texture_width;
	int texture_height;

	void validateDisplay(){
		if(display == NULL){
			cerr << "GLContext: Display is null" << endl;
			exit(1);
		}
		if(!display->window_egl_isCurrent()){
			cerr << "GLContect: Display is not current!" << endl;
			exit(1);
		}
	}

	void texture_render();

public:

	BGRARender(XCDisplay* d);
	~BGRARender();

	void texture_init(int width, int height);
	void texture_fill(unsigned char* data, int x, int y, int width, int height);
	void render();
};

#endif