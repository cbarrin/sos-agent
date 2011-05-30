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
#include <pthread.h>

#include "common.h"
#include "datatypes.h"
#include "list.h"

extern pthread_mutex_t stream_mutex;
extern pthread_cond_t stream_data_mutex; 


void * connect_send_tcp_data(void *ptr) { 
	stream_list_t *stream_list = (stream_list_t *) ptr; 
	stream_data_t *data; 

	int tcp_conn_sock;
	struct addrinfo hints, *servinfo, *p;
	int ret;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
	int stream; 

	/* Make TCP CONNECTION */ 
	if (( ret = getaddrinfo(TCP_DESTINATION_ADDRESS, TCP_PORT, &hints, &servinfo)) != 0) { 
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret)); 
		return NULL; 
	} 

	// loop though all results and connect to first one we can 
	for( p = servinfo; p != NULL; p = p->ai_next) { 
		if ((tcp_conn_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
			perror("server: socket_tcp"); 
			continue; 
		}
		if (connect(tcp_conn_sock, p->ai_addr, p->ai_addrlen) == -1) { 
			close(tcp_conn_sock); 
			perror("server: connect_tcp"); 
			continue; 
		}
		break; 
	}

	if(p == NULL) { 
		fprintf(stderr, "server: failed to connect\n"); 
		return NULL; 
	}
	/* End of TCP */ 


	while(1)  { 
		for(stream = 0; stream < NUM_STREAMS; stream++) { 
			/* this maybe to aggressive, perhaps add usleep later? */ 

			pthread_mutex_lock(&stream_mutex); 

			while (stream_empty( &stream_list->stream[stream]) == 0 )    
				pthread_cond_wait(&stream_data_mutex, &stream_mutex); 
				
			data = pop_head_stream_data(&stream_list->stream[stream]); 	
			pthread_mutex_unlock(&stream_mutex); 

			if(send(tcp_conn_sock, data->data, data->len,0) == -1) { 
				perror("send_tcp_data: send"); 
			}
			free(data->data); 
			free(data); 
			
		} 
	}

} 
