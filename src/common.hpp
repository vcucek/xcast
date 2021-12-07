
#ifndef __XC_COMMON_H__
#define __XC_COMMON_H__ 1

#include <cstdlib>
#include <cstring>

static const long get_time_nano(){
	
	timespec time;
	time.tv_sec = 0;
	time.tv_nsec = 0;
	
	clock_gettime(CLOCK_REALTIME, &time);
	static long sec = (long)time.tv_sec;
	
	return (time.tv_sec-sec)*1000000000 + time.tv_nsec;
}


static const void print_buffer(const char* name, uint8_t* buffer, int buffer_size, int count) {
    printf("%s\n", name);
    for(int i=0; i<buffer_size && i<100; i++) {
        printf("%x", buffer[i]);
    }
    printf("\n");
}

#endif
