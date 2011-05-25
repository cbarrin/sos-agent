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

#include "common.h"


int main( int argc, char **argv) { 
	
		int sctp_listen_sock, sctp_conn_sock; 
		struct sockaddr_in servaddr; 
		struct sctp_initmsg initmsg; 
		char buffer[MAX_BUFFER]; 
		struct sockaddr_storage their_addr; 
		socklen_t sin_size; 


		int tcp_listen_sock, tcp_conn_sock; 
		struct addrinfo hints, *servinfo, *p; 
		//struct sigaction sa; 
		int yes=1; 
		//char s[INET6_ADDRSTRLEN]; 
		int ret; 

		memset(&hints, 0, sizeof(hints)); 
		hints.ai_family = AF_UNSPEC; 
		hints.ai_socktype = SOCK_STREAM; 
		hints.ai_flags = AI_PASSIVE; // use my IP


		/*************** TCP STUFF ********************/ 
		if (( ret = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo)) != 0) {
			printf("getaddrinfo: %s\n", gai_strerror(ret)); 
			return EXIT_FAILURE; 
		}
	
		/* Loop though all the results and vind to first one we can */ 
		for( p = servinfo; p != NULL; p = p->ai_next) { 
			if (( tcp_listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
				perror("server: socket_tcp"); 
				continue; 
			}
			if( setsockopt(tcp_listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
				perror("server: setsockopt_tcp"); 
				return EXIT_FAILURE; 
			}
			if(bind(tcp_listen_sock, p->ai_addr, p->ai_addrlen) == -1) { 
				perror("server: bind_tcp"); 
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
			perror("server: listen_tcp"); 
			return EXIT_FAILURE; 
		}

		/*************** SCTP STUFF ********************/ 


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
		
			
			if((tcp_conn_sock = accept(tcp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
				perror("server: accept tcp"); 	
				/* FIXME This shouldn't kill the server if accept fails */ 
				return EXIT_FAILURE; 
			}

			if(recv(tcp_conn_sock, buffer, MAX_BUFFER, 0) == -1) { 
				perror("server: recv_tcp"); 
				/* FIXME: don't need to kill server here ! */ 
				return EXIT_FAILURE; 
			}
			printf("TCP %s\n", buffer); 


			if((sctp_conn_sock = accept( sctp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
				perror("server: accept"); 
				return EXIT_SUCCESS; 
			}
		//	strcpy(buffer, "Hello world!"); 
			if(sctp_sendmsg(sctp_conn_sock, (void *)buffer, strlen(buffer), NULL, 0, 0, 0, STREAM0, 0, 0) == -1) { 
				perror("server: sctp_sendmsg"); 
			}
		} 
	close(sctp_conn_sock); 
	return EXIT_SUCCESS; 
}
