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
#include <omp.h>
#include <netinet/in.h>

#include "common.h"
#include "datatypes.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"
#include "controller.h"


int init_controller_listener(controller_t * controller) 
{

	struct addrinfo hints, *servinfo; 
	int rv; 
	
	memset(&hints, 0, sizeof(hints)); 
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_DGRAM; 
	hints.ai_flags = AI_PASSIVE; 

	if(( rv = getaddrinfo(NULL, CONTROLLER_MSG_PORT, &hints, &servinfo)) != 0) 	
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
		exit(1); 
	}  
	for( controller->dest = servinfo; controller->dest != NULL; controller->dest = controller->dest->ai_next) 
	{
		if((controller->sock = socket(controller->dest->ai_family, controller->dest->ai_socktype, 
			controller->dest->ai_protocol)) == -1) 
		{
			perror("Discovery: socket"); 
			continue;
		}
		if( bind(controller->sock, controller->dest->ai_addr, controller->dest->ai_addrlen) == -1)
		{
			close(controller->sock); 
			perror("controller bind"); 
			continue; 
		}
		break; 	
	}
	if(controller->dest == NULL) 
	{
		fprintf(stderr, "controller failed to bind\n"); 
		exit(1); 
	} 
	return EXIT_SUCCESS; 
}


int get_controller_message(controller_t *controller) 
{ 
	socklen_t addr_len; 	
//	struct sockaddr_storage their_addr; 
	struct sockaddr_in their_addr; 
//	sockaddr_in client; 
	int size; 
	char buf[MAX_BUFFER]; 
	
	addr_len = sizeof(their_addr); 
	if( (size = recvfrom(controller->sock, buf, sizeof(buf), 0, 
			(struct sockaddr *) &their_addr, &addr_len)) == -1) 
	{
		perror("recvfrom get_controller_message\n"); 
		exit(1); 
	}
	
	inet_ntop(their_addr.sin_family, 
			get_in_addr((struct sockaddr *) &their_addr), 
			controller->send_ip, sizeof(controller->send_ip)); 

	controller->port = ntohs(their_addr.sin_port); 
	printf("%s %d\n", controller->send_ip, ntohs(their_addr.sin_port)); 
	return EXIT_SUCCESS; 
} 

