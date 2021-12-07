 
#include "BGRARender.hpp"

const char vertex_src [] = "\
precision mediump float;\
precision mediump int;\
\
attribute vec4 vertex_coord;\
attribute vec2 texture_coord;\
\
uniform int texture_width;\
uniform int texture_height;\
uniform int width;\
uniform int height;\
\
uniform vec2 scale;\
uniform vec2 position;\
\
varying vec2 texture_uv;\
\
void main()\
{\
texture_uv = texture_coord;\
float screen_aspect_ratio = float(height) / float(width);\
float texture_aspect_ratio = float(texture_height) / float(texture_width);\
vec2 vertex = vec2(vertex_coord.x, vertex_coord.y * texture_aspect_ratio / screen_aspect_ratio);\
gl_Position = vec4( (vertex.xy + position.xy) * scale.xy, 0.0, 1.0 );\
}\
";

const char fragment_src [] =
"\
precision mediump float;\
precision mediump int;	\
\
uniform sampler2D texture;\
uniform int texture_width;\
uniform int texture_height;\
\
varying vec2 texture_uv;\
\
void main()\
{\
vec3 color = texture2D(texture, texture_uv).rgb;\
gl_FragColor = vec4(color.b, color.g, color.r, 1.0);\
}\
";

//float pixel_w = 1.0 / texure_width;
//float pixel_h = 1.0 / texure_height;
//float y_scale = clamp(float(height)/float(texture_height) * scale, 0.0, 1.0);\
//float x_scale = clamp(float(width)/float(texture_width) * scale, 0.0, 1.0);\

const float vertexArray[] = {
	-1.0,-1.0,  0.0,
	 1.0,-1.0,  0.0,
	-1.0, 1.0,  0.0,
	 1.0, 1.0,  0.0
};

const float textureArray[] = {
	0.0,  1.0, 
	1.0,  1.0, 
	0.0,  0.0, 
	1.0,  0.0 
};
 
static void print_shader_info_log (GLuint  shader){
	GLint  length;
	
	glGetShaderiv ( shader , GL_INFO_LOG_LENGTH , &length );
	
	if ( length ) {
		char* buffer  =  new char [ length ];
		glGetShaderInfoLog ( shader , length , NULL , buffer );
		cout << "Shader info: " <<  buffer << flush << endl;
		delete [] buffer;
		
		GLint success;
		glGetShaderiv( shader, GL_COMPILE_STATUS, &success );
		if ( success != GL_TRUE )   exit ( 1 );
	}
}


static GLuint load_shader (const char *shader_source, GLenum type){
	GLuint  shader = glCreateShader( type );
	
	glShaderSource  ( shader , 1 , &shader_source , NULL );
	glCompileShader ( shader );
	
	print_shader_info_log ( shader );
	
	return shader;
}

BGRARender::BGRARender(XCDisplay* d){
	
	display = d;
	
	if(!display->window_egl_init()){
		cerr << "GLContext::GLContext(XCDisplay* d): Can't initialize! Can not create window!" << endl;
		return;
	}
	
	validateDisplay();

	GLuint vertexShader   = load_shader ( vertex_src , GL_VERTEX_SHADER  );     // load vertex shader
	GLuint fragmentShader = load_shader ( fragment_src , GL_FRAGMENT_SHADER );  // load fragment shader
	
	GLuint shaderProgram  = glCreateProgram ();                 // create program object
	glAttachShader ( shaderProgram, vertexShader );             // and attach both...
	glAttachShader ( shaderProgram, fragmentShader );           // ... shaders to it
	
	glLinkProgram ( shaderProgram );    // link the program
	glUseProgram  ( shaderProgram );    // and select it for usage
	
	//// now get the locations (kind of handle) of the shaders variables
	vertex_coord_loc  = glGetAttribLocation  ( shaderProgram , "vertex_coord" );
	if ( vertex_coord_loc < 0) {
		cerr << "Unable to get uniform location" << endl;
		exit(1);
	}
	
	texture_coord_loc  = glGetAttribLocation  ( shaderProgram , "texture_coord" );
	if ( texture_coord_loc < 0) {
		cerr << "Unable to get uniform location" << endl;
		exit(1);
	}
	
	texture_loc = glGetUniformLocation(shaderProgram, "texture");
	
	texture_width_loc = glGetUniformLocation(shaderProgram, "texture_width");
	texture_height_loc = glGetUniformLocation(shaderProgram, "texture_height");
	display_width_loc = glGetUniformLocation(shaderProgram, "width");
	display_height_loc = glGetUniformLocation(shaderProgram, "height");

	scale_loc = glGetUniformLocation(shaderProgram, "scale");
	position_loc = glGetUniformLocation(shaderProgram, "position");

	glClearColor ( 0.0 , 0.0 , 0.0 , 0.0);    // background color
	
	int width, height;
	display->window_egl_size(&width, &height);
	
	glViewport ( 0 , 0 , width , height );
	glClear ( GL_COLOR_BUFFER_BIT );
	
	glVertexAttribPointer ( vertex_coord_loc, 3, GL_FLOAT, false, 0, vertexArray );
	glEnableVertexAttribArray ( vertex_coord_loc );
	
	glVertexAttribPointer ( texture_coord_loc, 2, GL_FLOAT, false, 0, textureArray );
	glEnableVertexAttribArray ( texture_coord_loc );
}

BGRARender::~BGRARender(){
	display = NULL;
}

void BGRARender::texture_init(int width, int height){
	
	validateDisplay();
	
	// Create one OpenGL texture
	glGenTextures(1, &textureID);
	
	// "Bind" the newly created texture : all future texture functions will modify this texture
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(texture_loc, 0);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	texture_width = width;
	texture_height = height;
	
	glUniform1i(texture_width_loc, texture_width);
	glUniform1i(texture_height_loc, texture_height);
}

void BGRARender::texture_render(){

	validateDisplay();
	glGenerateMipmap(GL_TEXTURE_2D);

	int width, height;
	display->window_egl_size(&width, &height);

	glUniform1i(display_width_loc, width);
	glUniform1i(display_height_loc, height);

	GLfloat scale[2] = {1.0f,1.0f};
	GLfloat position[2] = {0.0f,0.0f};
	glUniform2fv(scale_loc, 1, &scale[0]);
	glUniform2fv(position_loc, 1, &position[0]);

	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 );
}

void BGRARender::texture_fill(unsigned char* data, int x, int y, int width, int height){
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

void BGRARender::render(){

	validateDisplay();
	texture_render();
}
