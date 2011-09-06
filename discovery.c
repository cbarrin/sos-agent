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
#include <uuid/uuid.h>


#include "uthash.h"
#include "common.h"
#include "packet.pb-c.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"


int init_discovery(discovery_t * discovery) 
{

	struct addrinfo hints, *servinfo; 
	int rv; 
	
	memset(&hints, 0, sizeof(hints)); 
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_DGRAM; 
	
	if(( rv = getaddrinfo(DISCOVERY_DEST_ADDR, DISCOVERY_PORT, &hints, &servinfo)) != 0) 	
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
		exit(1); 
	}  
	for( discovery->dest = servinfo; discovery->dest != NULL; discovery->dest = discovery->dest->ai_next) 
	{
		if((discovery->sock = socket(discovery->dest->ai_family, discovery->dest->ai_socktype, 
			discovery->dest->ai_protocol)) == -1) 
		{
			perror("Discovery: socket"); 
			continue;
		}
		break; 	
	}
	if(discovery->dest == NULL) 
	{
		fprintf(stderr, "discovery failed to bind\n"); 
		exit(1); 
	} 
	return EXIT_SUCCESS; 
}


int send_discovery_message(discovery_t * discovery) 
{
	char *msg = "<sos><agent>arosen</agent><status>active</status></sos>"; 
					
	if((sendto(discovery->sock, msg, strlen(msg), 0, 
		discovery->dest->ai_addr, discovery->dest->ai_addrlen)) == -1) 
	{ 

		perror("Discovery: sendto\n"); 
	}
	return EXIT_SUCCESS; 
}
