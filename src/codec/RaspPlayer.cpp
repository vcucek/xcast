
#include "RaspPlayer.hpp" 

RaspPlayer::RaspPlayer(){
	
	cout << "Creating raspberry pi player" << endl;
	
	//variables initialization
	video_decode = NULL;
	video_scheduler = NULL;
	video_render = NULL;
	clock = NULL;
	
	status = 0;
	data_len = 0;
	executing = false;
	//////////////////////////
	
	memset(list, 0, sizeof(list));
	memset(tunnel, 0, sizeof(tunnel));
	
	if((client = ilclient_init()) == NULL){
		cerr << "IL client creation failed" << endl;
		exit(1);
	}
	
	if(OMX_Init() != OMX_ErrorNone)
	{
		ilclient_destroy(client);
		cerr << "OpenMAX initialization failed" << endl;
		exit(1);
	}
	
	char video_decode_str[] = "video_decode";
	char video_render_str[] = "video_render";
	char video_scheduler_str[] = "video_scheduler";
	char clock_str[] = "clock";
	
	// create video_decode
	if(ilclient_create_component(client, &video_decode, video_decode_str, 
		(ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS)) != 0){	
		status = -14;
	}
	list[0] = video_decode;
	
	// create video_render
	if(status == 0 && 
		ilclient_create_component(client, &video_render, video_render_str, 
								  ILCLIENT_DISABLE_ALL_PORTS) != 0){
		status = -14;
	}
	list[1] = video_render;
	
	// create clock
	if(status == 0 && 
		ilclient_create_component(client, &clock, clock_str, 
								  ILCLIENT_DISABLE_ALL_PORTS) != 0){
		status = -14;
	}
	list[2] = clock;
	
	memset(&cstate, 0, sizeof(cstate));
	cstate.nSize = sizeof(cstate);
	cstate.nVersion.nVersion = OMX_VERSION;
	cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
	cstate.nWaitMask = 1;
	
	if(clock != NULL && OMX_SetParameter(ILC_GET_HANDLE(clock), 
		OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone){
		status = -13;
	}
	
	// create video_scheduler
	if(status == 0 && 
		ilclient_create_component(client, &video_scheduler, 
								  video_scheduler_str, ILCLIENT_DISABLE_ALL_PORTS) != 0){
		status = -14;
	}
	list[3] = video_scheduler;
	
	set_tunnel(tunnel, video_decode, 131, video_scheduler, 10);
	set_tunnel(tunnel+1, video_scheduler, 11, video_render, 90);
	set_tunnel(tunnel+2, clock, 80, video_scheduler, 12);
	
	// setup clock tunnel first
	if(status == 0 && ilclient_setup_tunnel(tunnel+2, 0, 0) != 0){
		status = -15;
	}
	else{
		ilclient_change_component_state(clock, OMX_StateExecuting);
	}
	
	if(status == 0){
		ilclient_change_component_state(video_decode, OMX_StateIdle);
	}
	
	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = 130;
	//format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	format.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
	
	if(status == 0 &&
		OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) == OMX_ErrorNone &&
		ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) == 0)
	{
		port_settings_changed = 0;
		first_packet = 1;
		
		cout << "Video decoder state executing..." << endl;
		ilclient_change_component_state(video_decode, OMX_StateExecuting);
		
		executing = true;
	}
	else{
		oerr << "Creating RaspPlayer failed with status code: " << status << endl;
		exit(1);
	}
}

RaspPlayer::~RaspPlayer(){
	
	cout << "Disposing raspberry pi player" << endl;
	
	if(executing){
		buf->nFilledLen = 0;
		buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;
		
		if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone){
			status = -20;
		}
		
		// wait for EOS from render
		//TODO: check if this is needed!
		//ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
		//						ILCLIENT_BUFFER_FLAG_EOS, 10000);
		
		// need to flush the renderer to allow video_decode to disable its input port
		ilclient_flush_tunnels(tunnel, 0);
		ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
		executing = false;
	}
	
	ilclient_disable_tunnel(tunnel);
	ilclient_disable_tunnel(tunnel+1);
	ilclient_disable_tunnel(tunnel+2);
	ilclient_teardown_tunnels(tunnel);
	
	ilclient_state_transition(list, OMX_StateIdle);
	ilclient_state_transition(list, OMX_StateLoaded);
	
	ilclient_cleanup_components(list);
	
	OMX_Deinit();
	
	ilclient_destroy(client);
	
	if(status != 0){
		oerr << "Disposing RaspPlayer failed with status code: " << status << endl;
	}
}

void RaspPlayer::data(unsigned char* data, int data_size){
	
	while(data_size){
		
		if((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL)
		{
			// feed data and wait until we get port settings changed
			unsigned char* dest = buf->pBuffer;
			
			int read_size = data_size > buf->nAllocLen ? buf->nAllocLen : data_size;
			memcpy(dest, data, read_size);
			data += read_size;
			data_size -= read_size;
			
			data_len += read_size;
			
			if(port_settings_changed == 0 &&
				((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
				(data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1,
														ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0)))
			{
				port_settings_changed = 1;
				
				if(ilclient_setup_tunnel(tunnel, 0, 0) != 0)
				{
					status = -7;
					break;
				}
				
				ilclient_change_component_state(video_scheduler, OMX_StateExecuting);
				
				// now setup tunnel to video_render
				if(ilclient_setup_tunnel(tunnel+1, 0, 1000) != 0)
				{
					status = -12;
					break;
				}
				
				ilclient_change_component_state(video_render, OMX_StateExecuting);
			}
			if(!data_len)
				break;
			
			buf->nFilledLen = data_len;
			data_len = 0;
			
			buf->nOffset = 0;
			if(first_packet)
			{
				buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
				first_packet = 0;
			}
			else
				buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
			
			if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
			{
				status = -6;
				break;
			}
		}
		else{
			cerr << "Decoder input buffer is NULL" << endl;
			break;
		}
	}
	
	if(status != 0){
		oerr << "RaspPlayer failed with status code: " << status << endl;
		exit(1);
	}
}