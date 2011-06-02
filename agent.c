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

#include "common.h"
#include "datatypes.h"
#include "list.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"


int main( int argc, char **argv) { 

	
	options_t options; 	
	get_arguments(&options, argc, argv); 
	int i ; 

	i = fork(); 

	// Create sctp binded sockets and binded tcp connection 	
	if(i == 0) { 
		if(allocate_network_server(&options) != EXIT_SUCCESS) 
		{
			return EXIT_FAILURE; 
		} 
	} 	
	else { 
		if(create_tcp_server(&options) != EXIT_SUCCESS) { 	
			return EXIT_FAILURE; 
		} 	
	}	

	while(1) 
	{ 
		if(i == 0) 
		{ 	
			if(sctp_sockets_server_listen_accept(&options) != EXIT_SUCCESS) { 
				return EXIT_FAILURE; 
			} 		
			if(!fork()) 
			{ 
				if(connect_tcp_socket_client(&options) != EXIT_SUCCESS) 
				{ 	
					return EXIT_FAILURE; 
				} 
				parallel_recv_to_tcp_send(&options); 
				free_sockets(&options); 	
				return EXIT_SUCCESS; 
			}
		}
		else 
		{ 
			if(tcp_socket_server_listen_accept(&options) != EXIT_SUCCESS) 
			{
				return EXIT_SUCCESS; 
			} 
			if(!fork()) { 
				if(create_sctp_sockets_client(&options) != EXIT_SUCCESS) 
				{ 
					return EXIT_FAILURE; 
				} 
		
				recv_to_parallel_send(&options); 
				free_sockets(&options); 	
				return EXIT_SUCCESS; 
			} 
		}
	}
	free_sockets(&options); 
	return EXIT_SUCCESS; 
}
