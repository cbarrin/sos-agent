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

int main( int argc, char **argv) { 

	int sctp_conn_sock ; 	
	struct sockaddr_in servaddr; 
	struct sctp_event_subscribe events; 
	struct sctp_initmsg initmsg; 
	char buffer[MAX_BUFFER]; 

	int tcp_listen_sock, tcp_conn_sock; 
	struct sockaddr_storage their_addr; 
	struct addrinfo hints, *servinfo, *p; 
	socklen_t sin_size; 
	//struct sigaction sa; 
	int yes=1; 
	//char s[INET6_ADDRSTRLEN]; 
	int ret; 
	int stream_rotate; 

	if(argc != 2 ) { 
		printf("Usage: ./client <endhost> \n"); 
		return EXIT_FAILURE; 
	}


	

	memset(&hints, 0, sizeof(hints)); 
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE; // use my IP


	/** 	
	* Create a tcp socket and bind, listen accept. 
	* After accept start SCTP to end host										 
	*/ 

	if (( ret = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(ret)); 
		return EXIT_FAILURE; 
	}

	/* Loop though all the results and vind to first one we can */ 
	for( p = servinfo; p != NULL; p = p->ai_next) { 
		if (( tcp_listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
			perror("client: socket_tcp"); 
			continue; 
		}
		if( setsockopt(tcp_listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
			perror("client: setsockopt_tcp"); 
			return EXIT_FAILURE; 
		}
		if(bind(tcp_listen_sock, p->ai_addr, p->ai_addrlen) == -1) { 
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

	if(listen(tcp_listen_sock, 10) == -1) { 
		perror("client: listen_tcp"); 
		return EXIT_FAILURE; 
	}
	sin_size = sizeof(their_addr); 
	if((tcp_conn_sock = accept(tcp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
		perror("client: accept tcp"); 	
		/* FIXME This shouldn't die if it fails here */ 
		return EXIT_FAILURE; 
	}
	


	/* create SCTP Socket */ 
	sctp_conn_sock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP ); 

	/* specifiy the maximum number of streams that will be available per socket */ 
	memset( &initmsg, 0, sizeof(initmsg) ); 
	initmsg.sinit_num_ostreams = NUM_STREAMS; 
	initmsg.sinit_max_instreams = NUM_STREAMS; 
	initmsg.sinit_max_attempts = NUM_STREAMS -1; 

	if(setsockopt (sctp_conn_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1 ) {
		perror("client: setsockopt"); 
		return EXIT_SUCCESS; 
	}
	
	/*info about who is at the other end */ 
	bzero( (void *)&servaddr, sizeof(servaddr) ); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_port = htons(SCTP_PORT); 
	servaddr.sin_addr.s_addr = inet_addr(argv[1]); 

	/* connect to server */ 
	if( connect( sctp_conn_sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { 
		perror("client: connect"); 
		return EXIT_FAILURE; 
	}

	/* enable receipt of SCTP Snd/Rcv data via sctp_recvmsg */ 
	memset( (void *) &events, 0, sizeof(events) ); 
	events.sctp_data_io_event = 1; 

	if(setsockopt( sctp_conn_sock, SOL_SCTP, SCTP_EVENTS, (const void *)&events, sizeof(events)) == -1) { 
		perror("client: connect"); 
		return EXIT_FAILURE; 
	}

	/* reads and emits the status of the Socket */ 
#ifdef DEBUG 
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

#endif 

	/* Just loop and keep spitting data */ 
	ssize_t size; 
	stream_rotate = 0; 
	while (1) { 
		memset(buffer, 0, sizeof(buffer)); 
		if((size = recv(tcp_conn_sock, buffer, MAX_BUFFER, 0)) == -1) { 
			perror("client: recv_tcp"); 
			/* FIXME don't need to exit here  on failure */ 
			return EXIT_FAILURE; 
		}
		if(size == 0) { 
			printf("Otherside has closed tcp connection\n"); 
			/* FIXME */ 
			return EXIT_SUCCESS; 

		} 

		
		if(sctp_sendmsg(sctp_conn_sock, (void *)buffer, size, NULL, 0, 0, 0, stream_rotate, 0, 0) == -1) { 
			perror("client: sctp_sendmsg"); 
		}
#ifdef DEBUG 
	printf("send [%s] size [%zd] stream [%d]\n", buffer, size, stream_rotate); 
#endif 

		stream_rotate++; 
		stream_rotate %=NUM_STREAMS; 

	}

/*
	if((stream = sctp_recvmsg( sctp_conn_sock, (void *)buffer, sizeof(buffer), 
					(struct sockaddr *) NULL, 0, &sndrcvinfo, &flags)) == -1) { 
		perror("client: sctp_recvmsg"); 
	}
	printf("buffer [%s] stream [%d]\n", buffer, stream); 
*/ 


	close(sctp_conn_sock); 
	close(tcp_conn_sock); 

	return EXIT_SUCCESS; 
}
