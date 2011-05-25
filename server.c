#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#include "common.h"


int main( int argc, char **argv) { 
	
		int sctp_listen_sock, sctp_conn_sock; 
		struct sockaddr_in servaddr; 
		struct sctp_initmsg initmsg; 
		char buffer[MAX_BUFFER]; 
		struct sockaddr_storage their_addr; 
		socklen_t sin_size; 


		/* creates SCTP Socket */ 
		if((sctp_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) { 
			perror("server: socket"); 
			return EXIT_FAILURE; 
		}
		/* Accept connection from any interface */ 	
		bzero( (void *)&servaddr, sizeof(servaddr) ); 
		servaddr.sin_family = AF_INET; 
		servaddr.sin_addr.s_addr = htonl (INADDR_ANY); 
		servaddr.sin_port = htons (SCTP_PORT); 

		if(bind(sctp_listen_sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { 
			perror("server: bind"); 
			return EXIT_FAILURE; 
		}

		/* specify that a maximum of 5 streams will be available per socket */ 
		
		memset( &initmsg, 0, sizeof(initmsg)); 
		initmsg.sinit_num_ostreams = 5; 
		initmsg.sinit_max_instreams = 5; 
		initmsg.sinit_max_attempts = 4; 

		if(setsockopt( sctp_listen_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1) { 
			perror("server setsockopt"); 
			return EXIT_SUCCESS; 
		}

		/* place the server socket into the listen state */ 

		listen (sctp_listen_sock, 5); 
	
		sin_size = sizeof(their_addr); 
		while(1) { 
			printf("Waiting for new connection\n"); 
			if((sctp_conn_sock = accept( sctp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
				perror("server: accept"); 
				return EXIT_SUCCESS; 
			}
			strcpy(buffer, "Hello world!"); 
			if(sctp_sendmsg(sctp_conn_sock, (void *)buffer, strlen(buffer), NULL, 0, 0, 0, STREAM0, 0, 0) == -1) { 
				perror("server: sctp_sendmsg"); 
			}
		} 
	close(sctp_conn_sock); 
	return EXIT_SUCCESS; 
}
