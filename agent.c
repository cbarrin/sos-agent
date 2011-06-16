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
#include <poll.h>
#include <sys/epoll.h>
#include <omp.h>

#include "common.h"
#include "datatypes.h"
#include "list.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"


int main( int argc, char **argv) { 

	
	options_t options; 	
	get_arguments(&options, argc, argv); 
	
	// start tcp_server 	
	create_tcp_server_listen(&options); 
	create_parallel_server_listen(&options); 
	int i, ret; 

	// do our polling stuff 
	while(1) { 
		if((ret = epoll_connections(&options)) )  { 
			i = fork(); 
			if(i == 0) { 
				if(ret == TCP_SOCK_LISTEN) { 
					if(!tcp_socket_server_accept(&options)) { 
						if(!create_sctp_sockets_client(&options)) { 
							printf("TCP Socket created and accepted client on...SCTP connects finished!\n"); 	
						}
					}
				} 	
				else if(ret == PARALLEL_SOCK_LISTEN) { 
					if(!parallel_server_accept(&options)) { 
						if(!create_tcp_socket_client(&options)) { 
							printf("Other end accepted our TCP connection\n"); 	
						}
					}
	
				} 
				configure_epoll(&options); 
				epoll_data_transfer(&options); 
			} 
		}
	} 
	return EXIT_SUCCESS; 
}
