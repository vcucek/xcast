#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "common.hpp"
#include "XCDisplay.hpp" 
#include "codec/av.hpp"

#define SECOND_NS 1000000000
#define BUFFER_SIZE 3000000 //received frame size should not be larger then 3M
#define PORT 10522

using namespace std;

/*
void start_udp(void){
	
	static int display_created = 0;
	struct sockaddr_in si_me, si_other;
	
	unsigned int s, i, slen = sizeof(si_other) , recv_len;
	unsigned char buf[BUFLEN];
	
	//create a UDP socket
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		cerr << "Creating socket failed" << endl;
		exit(1);
	}
	
	// zero out the structure
	memset((char *) &si_me, 0, sizeof(si_me));
	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//set timer for recv_socket
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 400 * 1000;
	if( setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1){
		cerr << "Setting socket timeout failed" << endl;
		exit(1);
	}
	
	//bind socket to port
	if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
	{
		cerr << "Binding socket failed" << endl;
		exit(1);
	}
	
	cout << "Starting server loop" << endl;
	
	unsigned char image[1920*1080*4];
	
	//keep listening for data
	while(1)
	{
		
		recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen);
		
		//try to receive some data, this is a blocking call
		if (recv_len == -1)
		{
			//timeout
			cout << "Timeout receiving data from socket" << endl;
			if(display_created){
				display_close();
				video_dispose();
				display_created = 0;
			}
		}
		else{
			if(!display_created){
				if(display_open()){
					video_init_decode();
					display_created = 1;
				}
			}
			
			int image_used = 0;
			video_decode(buf, recv_len, &image[0], 1920*1080*4, image_used);
			
			if(image_used > 0){
				display_data(image, 0, 0, 1920, 1080);
				display_render();
			}
		}
	}
	
	close(s);
	
	if(display_created){
		display_close();
		video_dispose();
		display_created = 0;
	}
}
*/

bool read_chunk(unsigned int conn_desc, uint8_t* buffer, int size){
	
	int read_size = 0;
	int read_curr = 0;
	
	while(read_size < size){
		if ((read_curr = read(conn_desc, buffer+read_size, size-read_size)) <= 0){
			cerr << "Receiving data from client failed" << endl;
			return false;
		}
		read_size += read_curr;
	}
	return true;
}

void show_screen(unsigned int conn_desc) {

	uint8_t* buffer;
	buffer = (uint8_t*)malloc(BUFFER_SIZE);
	int chunk_size = 0;

    //read client display resolution
    if(!read_chunk(conn_desc, buffer, 8)){
        cout << "Error reading display resolution" << endl;
        free(buffer);
        return;
    }
    
    int width = 0;
    int height = 0;
    memcpy(&width, buffer, 4);
    memcpy(&height, buffer+4, 4);
    
    cout << "Client resolution: " << width << "x" << height << endl;
    
    // Open display
    XCDisplay* display = createDisplay();
    if(!display){
        cout << "Opening connection to xserver failed!" << endl;
        free(buffer);
        return;
    }

    display->window_create();
    display->window_decoder_init(width, height);
    
    cout << "Receiving data..." << endl;
    
    //read data from client
    while(1){
        //read chunk size
        if(!read_chunk(conn_desc, buffer, 4)){
            cout << "Error reading chunk size" << endl;
            break;
        }
        memcpy(&chunk_size, buffer, 4);
        //cout << "Frame size: " << chunk_size << endl;
        
        if(chunk_size > BUFFER_SIZE){
            cerr << "Received chunk size is larger then specified buffer. Size: " << chunk_size << endl;
            break;
        }
        //read chunk
        if(!read_chunk(conn_desc, buffer, chunk_size)){
            cout << "Error reading chunk data" << endl;
            break;
        }
        display->window_decoder_data(buffer, chunk_size);
        //print_buffer("Frame data:", buffer, chunk_size, 100);
    }
    cout << "Client disconnected" << endl;

    free(buffer);
    delete display;
}

void send_screen(unsigned int sock_desc) {

	uint8_t* buffer;
    int buffer_used = 0;
	buffer = (uint8_t*)malloc(BUFFER_SIZE);

    // Open display
    XCDisplay* display = createDisplay();
    if(!display){
        cout << "Creating XCDisplay failed!" << endl;
        free(buffer);
        return;
    }

    int width, height;
    display->size(&width, &height);
    cout << "Screen resolution: " << width << "x" << height << endl;

    //send resoulution
    memcpy(buffer, &width, 4);
    memcpy(buffer+4, &height, 4);
    if(send(sock_desc, buffer, 8, 0)==-1) {
        cerr << "Sending screen resolution failed. Client disconnected..." << endl;
        free(buffer);
        return;
    }
    
    video_init_encode(width, height);

    unsigned long time_last = get_time_nano();
    unsigned long time_fps = get_time_nano();
    long frames = 0;
    
    while(1){
        while(get_time_nano() - time_last < 30 * 1000000){ }
        time_last = get_time_nano();
        
        display->capture();
        //print_buffer("Raw Frame data:", display->image_buffer, display->image_buffer_used, 100);
        
        //encode image
        buffer_used = 0;
        video_encode(display->image_buffer,
                     display->image_buffer_size, buffer+4, buffer_used);
        
        //cout << "Frame size: " << buffer_used << endl;
        
        //first 4 bytes reserved for size
        memcpy(buffer, &buffer_used, 4);
        if(send(sock_desc, buffer, 4, MSG_NOSIGNAL)==-1) {
            //cout << "Sending data chunk size failed" << endl;
            break;
        }
        
        //send frame data
        if(send(sock_desc, buffer+4, buffer_used, MSG_NOSIGNAL)==-1) {
            //cout << "Sending data chunk failed" << endl;
            break;
        }
        //print_buffer("Frame data:", buffer, buffer_used+4, 100);
        
        frames++;
        if(frames%25 == 0){
            if(frames%100 == 0){
                frames = 0;
                time_fps = get_time_nano();
                cout << "Timer reset" << endl;
            }
            double sec_diff = (double)(get_time_nano() - time_fps)/(double)(SECOND_NS);
            cout << "FPS: " << (double)frames/sec_diff << endl;
        }
    }
    free(buffer);
    delete display;
    video_dispose();
}

void start_tcp_client(char* ip, int port, uint8_t type){
	
	struct sockaddr_in addr_other;
	
	unsigned int sock_desc;
	unsigned int addr_other_size = sizeof(addr_other);
	
	if ( (sock_desc = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		cerr << "Creating socket failed" << endl;
		exit(1);
	}
	
	memset((char *) &addr_other, 0, addr_other_size);
	addr_other.sin_family = AF_INET;
	addr_other.sin_port = htons(port);
	
	//Bind ip to address
	if (inet_aton(ip , &addr_other.sin_addr) == 0) 
	{
		cerr << "Binding socket address failed" << endl;
		exit(1);
	}
	
	//Connect to remote server
	if (connect(sock_desc , (struct sockaddr *)&addr_other , addr_other_size) < 0)
	{
		cerr << "Connecting to server failed" << endl;
		exit(1);
	}

	//send request type
	if(send(sock_desc, &type, 1, 0)==-1)
	{
		cerr << "Sending request type failed" << endl;
		exit(1);
	}

    if(type == 1) {
        show_screen(sock_desc);
        close(sock_desc);
    }
    if(type == 2) {
        send_screen(sock_desc);
        close(sock_desc);
    }
}

void start_tcp(){
	
	struct sockaddr_in addr_me, addr_other;
	
	unsigned int sock_desc, conn_desc;
	
	unsigned int addr_me_size = sizeof(addr_me);
	unsigned int addr_other_size = sizeof(addr_other);
	
	//create a TCP socket
	if((sock_desc = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		cerr << "Creating socket failed" << endl;
		exit(1);
	}
	
	// zero out the structure
	memset((char *) &addr_me, 0, addr_me_size);
	
	addr_me.sin_family = AF_INET;
	addr_me.sin_port = htons(PORT);
	addr_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//bind socket to port
	if( bind(sock_desc, (struct sockaddr*)&addr_me, addr_me_size) == -1)
	{
		cerr << "Binding socket failed" << endl;
		exit(1);
	}

    video_codec_register();
	
	listen(sock_desc, 5);
	
	while(1){
		//accept client connection
		cout << "Accepting client connections..." << endl;
		if((conn_desc = accept(sock_desc, (struct sockaddr *)&addr_other, &addr_other_size)) == -1){
			cerr << "Connecting client failed" << endl;
			continue;
		}

		cout << "Client connected" << endl;

        //read client request type
        uint8_t type = 0;
        if(!read_chunk(conn_desc, &type, 1)){
            cout << "Error reading request type" << endl;
        }

        try {
            if(type == 1) {
                cout << "Client requesting screen pull" << endl;
                send_screen(conn_desc);
            }
            if(type == 2) {
                cout << "Client requesting screen push" << endl;
                show_screen(conn_desc);
            }
        } catch (...) {
            std::exception_ptr p = std::current_exception();
            std::clog <<(p ? p.__cxa_exception_type()->name() : "null") << std::endl;
        }
		cout << "Client disconnected" << endl;
	}
	close(sock_desc);
}

int main(int argc, char** argv){

    int arg_error = 0;
	if(argc >= 2){
        if (strcmp("--listen",argv[1]) == 0) {
            cout << "Listening on port: " << PORT << endl;
            start_tcp();
        }
        else if (strcmp("--pull",argv[1]) == 0) {
            if(argc >= 3){
                char* ip = NULL;
                ip = argv[2];
                start_tcp_client(ip, PORT, 1);
            }
            else { arg_error = 1; }
        }
        else if (strcmp("--push",argv[1]) == 0) {
            if(argc >= 3){
                char* ip = NULL;
                ip = argv[2];
                start_tcp_client(ip, PORT, 2);
            }
            else { arg_error = 1; }
        }
        else { 
            arg_error = 1;
        }
    }
    if(arg_error) {
        cout << "Simple xorg program for receiving or sending display image." << endl;
        cout << "" << endl;
        cout << "Start service: xcast --listen" << endl;
        cout << "Show screen on remote machine: xcast --push <service IP>" << endl;
        cout << "Show remote screen on this machine: xcast --pull <service IP>" << endl;
    }

	return 0;
}
