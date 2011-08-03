#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "datatypes.h"
#include "common.h"
#include "arguments.h"
#include "network.h"
#include "poll.h"





int poll_loop(agent_t *agent) 
{
	int n_events; 
	int timeout = -1; 

	struct epoll_event events; 

	while(1) 
	{
		n_events = epoll_wait(agent->event_pool, &events, 1, timeout); 
		if(n_events < 0)
		{
			perror("epoll_wait"); 
			exit(1); 
		}
		switch( ((event_info_t *) events.data.ptr)->type) { 

			case HOST_SIDE_CONNECT:
				handle_client_side_connect(agent)		
				break; 
			case AGENT_SIDE_CONNECT:
				handle_agent_side_connect(agent); 
				break; 
			case HOST_SIDE_DATA_IN:
				break;
			case AGENT_SIDE_DATA_IN:
				break; 
			default: 
				printf("unknown event_into type!!\n"); 
				exit(1); 



		}
	}
}

