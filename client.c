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
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>


#include "common.h"
#include "datatypes.h"
#include "list.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"

int main( int argc, char **argv) { 

	options_t options; 
	get_arguments(&options, argc, argv); 

	if(create_tcp_server(&options) != EXIT_SUCCESS) { 	
		return EXIT_FAILURE; 
	} 	

	if(create_sctp_sockets_client(&options) != EXIT_SUCCESS) 
	{ 
		return EXIT_FAILURE; 
	} 

	recv_to_parallel_send(&options); 

	return EXIT_SUCCESS; 
}

