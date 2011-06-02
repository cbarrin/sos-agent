
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

#include "common.h"
#include "datatypes.h"
#include "list.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"


/*
	The process that class allowcate_network_server 
	ends up binding on p_conn_sock_server. Then, 
	it makes an outgoing tcp connection to 
	TCP_DESTINATION_ADDRESS. 

	Then it should call parallel_recv_to_tcp_send()
	which reads from the binded parallel socket 
	and puts in the connected tcp socket 


*/ 

int allocate_network_server(options_t *options) 
{ 

	if(options->protocol == SCTP) { 
		return create_sctp_sockets_server(options); 
	} else if(options->protocol == TCP) { 
		// FIXME need tcp option
	} 
	return EXIT_FAILURE; 
} 



int create_sctp_sockets_server(options_t *options) 
{ 
	int count; 
	struct sockaddr_in servaddr; 
	struct sctp_initmsg initmsg; 


	for(count = 0; count < options->num_parallel_sock; count++) { 
		if(( options->p_listen_sock[count] = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) { 
			perror("creating p_listen_sock");   
			exit(1); 
		} 
	}		
	/* Accept connection from any interface */ 	

	bzero( (void *)&servaddr, sizeof(servaddr) ); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY); 

	for(count = 0; count < options->num_parallel_sock; count++)  
	{
		servaddr.sin_port = htons (PORT_START + count); 
		if(bind(options->p_listen_sock[count], (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) 
		{ 
			perror("server: bind"); 
			return EXIT_FAILURE; 
		}
	}

	/* specify that a maximum of 5 streams will be available per socket */ 
	
	memset( &initmsg, 0, sizeof(initmsg)); 
	initmsg.sinit_num_ostreams = NUM_STREAMS; 
	initmsg.sinit_max_instreams = NUM_STREAMS; 
	initmsg.sinit_max_attempts = NUM_STREAMS -1; 

	for( count = 0; count < options->num_parallel_sock; count++) 
	{ 
		if(setsockopt( options->p_listen_sock[count], IPPROTO_SCTP, 
			SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1) 
		{ 
			perror("server setsockopt"); 
			return EXIT_SUCCESS; 
		}
	} 
	


  /*
	* Uncomment this code if you want sctp events to be 
	* returned on sctp_recv 
	*
	
	memset( (void *) &events, 0, sizeof(events)); 
	events.sctp_data_io_event = 1; 

	for( count = 0; count < options->num_parallel_sock; count++) 
	{ 
		if(setsockopt(options->p_listen_sock[count], SOL_SCTP, SCTP_EVENTS, 
			(const void *) &events, sizeof(events)) == -1) 
		{ 	
			perror("server: setscokopt"); 
			return EXIT_FAILURE; 
		} 
	} 
	*/ 
	return EXIT_SUCCESS; 
}

int sctp_sockets_server_listen_accept(options_t *options)  { 

	int count; 
	socklen_t sin_size; 
	struct sockaddr_storage their_addr; 

	for( count = 0; count < options->num_parallel_sock; count++) 
	{ 
		listen( options->p_listen_sock[count], BACKLOG); 
	} 

	if(options->verbose) 
	{
		printf("Waiting for connections\n"); 
	} 

	sin_size = sizeof(their_addr); 
	for(count = 0; count < options->num_parallel_sock; count++) 
	{ 
		if((options->p_conn_sock_server[count] = accept( options->p_listen_sock[count], 
			(struct sockaddr *) &their_addr, &sin_size)) == -1) 
		{
			perror("Accepting sockets!"); 
			return EXIT_FAILURE; 
		}
	}
	if(options->verbose) 
	{
		printf("All connections accepted!\n"); 
	} 
	return EXIT_SUCCESS; 
} 

int create_sctp_sockets_client(options_t *options) 
{ 

	int count; 
	struct sockaddr_in servaddr; 
	struct sctp_initmsg initmsg; 


	for(count = 0; count < options->num_parallel_sock; count++)   
	{ 
		if(( options->poll_p_conn_sock_client[count].fd = 
				socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) 
		{ 
			perror("creating p_listen_sock");   
			exit(1); 
		}
		else { 
			options->poll_p_conn_sock_client[count].events = POLLOUT; 
		} 
	} 

	/* specifiy the maximum number of streams that will be available per socket */ 
	memset( &initmsg, 0, sizeof(initmsg) ); 
	initmsg.sinit_num_ostreams = NUM_STREAMS; 
	initmsg.sinit_max_instreams = NUM_STREAMS; 
	initmsg.sinit_max_attempts = NUM_STREAMS -1; 

	for(count = 0; count < options->num_parallel_sock; count++)   
	{
		if(setsockopt (options->poll_p_conn_sock_client[count].fd, IPPROTO_SCTP,
			SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1 ) 
		{
			perror("client: setsockopt"); 
			return EXIT_SUCCESS; 
		}
	} 
	
	/*info about who is at the other end */ 
	bzero( (void *)&servaddr, sizeof(servaddr) ); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(SCTP_CONNECT_TO_ADDR); 

	for(count = 0; count < options->num_parallel_sock; count++)   
	{ 
		servaddr.sin_port = htons(PORT_START + count); 
		/* connect to server */ 
		if( connect( options->poll_p_conn_sock_client[count].fd, 
			(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) 
		{ 
			perror("client: connect"); 
			return EXIT_FAILURE; 
		}
	} 

	/* 
	*  To enable receipt of SCTP Snd/Rcv data via sctp_recvmsg   
	*  uncomment below
	*/ 

	/*	
	struct sctp_event_subscribe events; 
	memset( (void *) &events, 0, sizeof(events) ); 
	events.sctp_data_io_event = 1; 

	for(count = 0; count < options->num_parallel_sock; count++)   
	{ 
		if(setsockopt( options->poll_p_conn_sock[count], SOL_SCTP, SCTP_EVENTS, 
			(const void *)&events, sizeof(events)) == -1) 
		{ 
			perror("client: connect"); 
			return EXIT_FAILURE; 
		}
	} 

	*/ 

	/* reads and emits the status of the Socket  
	
	struct sctp_status status;
	int in = sizeof(status); 
	if(getsockopt( sctp_conn_sock, SOL_SCTP, SCTP_STATUS, (void *) &status, (socklen_t *) &in) == -1) { 
		perror("client: getsockopt"); 
		return EXIT_FAILURE; 
	}

	printf("assoc id    = %d\n", status.sstat_assoc_id ); 
	printf("state       = %d\n", status.sstat_state ); 
	printf("instrms     = %d\n", status.sstat_instrms ); 
	printf("outstrms    = %d\n", status.sstat_outstrms ); 

	*/ 
	return EXIT_SUCCESS; 
}


int connect_tcp_socket_client(options_t *  options) 
{  
	struct addrinfo hints, *servinfo, *p;
	int ret;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_STREAM;

	/* Make TCP CONNECTION */ 
	if (( ret = getaddrinfo(TCP_DESTINATION_ADDRESS, TCP_PORT, &hints, &servinfo)) != 0) { 
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret)); 
		return EXIT_FAILURE; 
	} 

	// loop though all results and connect to first one we can 
	for( p = servinfo; p != NULL; p = p->ai_next) { 
		if ((options->tcp_client_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
			perror("socket_tcp"); 
			continue; 
		}
		if (connect(options->tcp_client_sock, p->ai_addr, p->ai_addrlen) == -1) { 
			close(options->tcp_client_sock); 
			perror("connect_tcp"); 
			continue; 
		}
		break; 
	}

	if(p == NULL) { 
		fprintf(stderr, "server: failed to connect\n"); 
		return EXIT_FAILURE; 
	}
	return EXIT_SUCCESS; 
}


int create_tcp_server(options_t * options) 
{ 

	struct addrinfo hints, *servinfo, *p; 
	int yes=1; 
	int ret; 
	

	memset(&hints, 0, sizeof(hints)); 
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE; // use my IP

	/** 	
	* Create a tcp socket and bind, listen accept. 
	*/ 

	if (( ret = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(ret)); 
		return EXIT_FAILURE; 
	}

	/* Loop though all the results and vind to first one we can */ 
	for( p = servinfo; p != NULL; p = p->ai_next) { 
		if (( options->tcp_listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
			perror("client: socket_tcp"); 
			continue; 
		}
		if( setsockopt(options->tcp_listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
			perror("client: setsockopt_tcp"); 
			return EXIT_FAILURE; 
		}
		if(bind(options->tcp_listen_sock, p->ai_addr, p->ai_addrlen) == -1) { 
			perror("client: bind_tcp"); 
			continue; 
		}
		break; 
	}

	if( p == NULL) { 
		printf("TCP FAILED TO BIND!!!\n"); 	
		return EXIT_FAILURE; 
	}

	freeaddrinfo(servinfo); 
	return EXIT_SUCCESS; 
}

int tcp_socket_server_listen_accept(options_t * options) { 

	socklen_t sin_size; 
	struct sockaddr_storage their_addr; 

	if(listen(options->tcp_listen_sock, BACKLOG) == -1) { 
		perror("client: listen_tcp"); 
		return EXIT_FAILURE; 
	}
	sin_size = sizeof(their_addr); 

	if((options->tcp_server_sock = accept(options->tcp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
		perror("client: accept tcp"); 	
		return EXIT_FAILURE; 
	}	
	return EXIT_SUCCESS; 
}



int parallel_recv_to_tcp_send(options_t *options) 
{ 

	int size; 
	int count; 
	int flags = 0; 
	char buffer[MAX_BUFFER]; 
	struct sctp_sndrcvinfo sndrcvinfo; 

	while(1) { 

		for(count = 0; count <  options->num_parallel_sock; count++) { 
			if( (size = sctp_recvmsg( options->p_conn_sock_server[count],
				(void *)buffer, sizeof(buffer), 
				(struct sockaddr *) NULL, 0, &sndrcvinfo, &flags)) == -1 )  
			{
				perror("server: sctp_recvmsg"); 
				close_sockets_sctp_server(options); 
				return EXIT_FAILURE; 
			}
			if(size == 0) 
			{ 
				if(options->verbose) 
				{ 
					printf("Other side ended connection!\n"); 
				} 
				close_sockets_sctp_server(options); 
				return EXIT_SUCCESS; 
			}
			else 
			{
				if(send(options->tcp_client_sock, buffer, size, 0) == -1) { 
					perror("tcp send");
				}
			}
		}
	}
}


int recv_to_parallel_send(options_t *options) 
{ 
	int size; 
	int count; 
	char buffer[MAX_BUFFER]; 
	int retry; 

	while(1) { 

		for(count = 0; count <  options->num_parallel_sock; count++) { 
			if(( size = recv(options->tcp_server_sock, buffer, sizeof(buffer), 0)) == -1) { 
				perror("tcp_server_sock: recv");  
				close_sockets_sctp_client(options); 
				return EXIT_FAILURE; 
			}
			if(size == 0) 
			{ 
				if(options->verbose) 
				{ 
					printf("Other side ended connection!\n"); 
				} 
				close_sockets_sctp_client(options); 
				return EXIT_SUCCESS; 
			}
			else 
			{
				if(poll(&options->poll_p_conn_sock_client[count], 1, -1) == -1) { 
					perror("poll"); 
					return EXIT_FAILURE;
				}
				if(options->poll_p_conn_sock_client[count].revents & POLLOUT)
				{
					do { 
						retry = 0; 
						if( (sctp_sendmsg(options->poll_p_conn_sock_client[count].fd, buffer, 
							size, NULL, 0, 0, 0, 0, 0, 0)) == -1)
						{
							if(errno == 12) {
								retry = 1; 
								printf("errno = %d, %s\n", errno, strerror(errno)); 
							}
							else { perror("sctp_send");  }
						}
					}while(retry); 
				}
			}
		}
	}
}

void close_sockets_sctp_server(options_t *options) 
{ 
	int count; 
	close(options->tcp_client_sock); 
	for( count = 0; count < options->num_parallel_sock; count++) { 
		close(options->p_conn_sock_server[count]); 		
	} 
}


void close_sockets_sctp_client(options_t *options) 
{ 
	int count; 

	close(options->tcp_server_sock); 
	for( count = 0; count < options->num_parallel_sock; count++) { 
		close(options->poll_p_conn_sock_client[count].fd); 		
	} 
}

void free_sockets(options_t *options) { 
	free(options->poll_p_conn_sock_client); 
	free(options->p_conn_sock_server); 
}
