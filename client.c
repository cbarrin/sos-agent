#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#include "common.h"

int main( int argc, char **argv) { 
	int sctp_conn_sock, flags; 	
	struct sctp_status status; 
	struct sockaddr_in servaddr; 
	struct sctp_sndrcvinfo sndrcvinfo; 
	struct sctp_event_subscribe events; 
	struct sctp_initmsg initmsg; 
	char buffer[MAX_BUFFER]; 
	int stream; 


	if(argc != 2 ) { 
		printf("Usage: ./client <endhost> \n"); 
		return EXIT_FAILURE; 
	}

	/* create SCTP Socket */ 
	sctp_conn_sock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP ); 

	/* specifiy the maximum number of streams that will be available per socket */ 
	memset( &initmsg, 0, sizeof(initmsg) ); 
	initmsg.sinit_num_ostreams = 5; 
	initmsg.sinit_max_instreams = 5; 
	initmsg.sinit_max_attempts = 4; 

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
	int in = sizeof(status); 
	if(getsockopt( sctp_conn_sock, SOL_SCTP, SCTP_STATUS, (void *) &status, (socklen_t *) &in) == -1) { 
		perror("client: getsockopt"); 
		return EXIT_FAILURE; 
	}

	printf("assoc id    = %d\n", status.sstat_assoc_id ); 
	printf("state       = %d\n", status.sstat_state ); 
	printf("instrms     = %d\n", status.sstat_instrms ); 
	printf("outstrms    = %d\n", status.sstat_outstrms ); 


	if((stream = sctp_recvmsg( sctp_conn_sock, (void *)buffer, sizeof(buffer), 
					(struct sockaddr *) NULL, 0, &sndrcvinfo, &flags)) == -1) { 
					
		perror("client: sctp_recvmsg"); 
	}
	printf("buffer [%s] stream [%d]\n", buffer, stream); 


	close(sctp_conn_sock); 

	return EXIT_SUCCESS; 
}
