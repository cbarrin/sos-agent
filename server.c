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
#include "tcp_thread.h"

pthread_mutex_t stream_mutex;
pthread_cond_t stream_data_mutex; 


int main( int argc, char **argv) { 

		int sctp_listen_sock, sctp_conn_sock, flags=0; 
		struct sockaddr_in servaddr; 
		struct sctp_initmsg initmsg; 
		struct sctp_event_subscribe events;
		char buffer[MAX_BUFFER]; 
		struct sockaddr_storage their_addr; 
		struct sctp_sndrcvinfo sndrcvinfo; 
		socklen_t sin_size; 
		int size; 


		pthread_t tcp_list_thread; 
		pthread_mutex_init(&stream_mutex, NULL); 
		pthread_cond_init(&stream_data_mutex, NULL); 

		


		/************** Set up linked list *************/ 
		stream_list_t *stream_list = create_stream_list(NUM_STREAMS); 


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
		initmsg.sinit_num_ostreams = NUM_STREAMS; 
		initmsg.sinit_max_instreams = NUM_STREAMS; 
		initmsg.sinit_max_attempts = NUM_STREAMS -1; 

		if(setsockopt( sctp_listen_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1) { 
			perror("server setsockopt"); 
			return EXIT_SUCCESS; 
		}


		/* place the server socket into the listen state */ 

	
		memset( (void *) &events, 0, sizeof(events)); 
		events.sctp_data_io_event = 1; 

		if(setsockopt(sctp_listen_sock, SOL_SCTP, SCTP_EVENTS, (const void *) &events, sizeof(events)) == -1) { 	
			perror("server: setscokopt"); 
			return EXIT_FAILURE; 
		} 

		listen (sctp_listen_sock, 5); 
	
		sin_size = sizeof(their_addr); 
		while(1) { 
			printf("Waiting for new connection\n"); 

			if((sctp_conn_sock = accept( sctp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
				perror("server: accept"); 
				return EXIT_FAILURE; 
			}

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
			printf("outstrms = %d\n", status.sstat_outstrms); 
#endif 


			pthread_create(&tcp_list_thread, NULL, connect_send_tcp_data, (void *) stream_list); 


			while (1) { 
				//memset(buffer, 0, sizeof(buffer)); 
				if(( size = sctp_recvmsg( sctp_conn_sock, (void *)buffer, sizeof(buffer), 
						(struct sockaddr *) NULL, 0, &sndrcvinfo, &flags)) == -1) { 
						perror("server: sctp_recvmsg"); 
						/* FIXME probably don't need to exit */ 
						return EXIT_FAILURE; 
				}
				if(size == 0) { 
					printf("Other side ended connection!\n"); 
					/* FIXME */ 
					return EXIT_SUCCESS; 
				} 
#ifdef DEBUG
				printf("added [%s] size [%d] from stream %d\n", (char *) buffer, size, sndrcvinfo.sinfo_stream); 
#endif 




				pthread_mutex_lock(&stream_mutex); 
				add_data_to_list(&stream_list->stream[ (sndrcvinfo.sinfo_stream) ], buffer, size); 
				pthread_cond_signal(&stream_data_mutex); 	
				pthread_mutex_unlock(&stream_mutex); 

				//stream_index++; 
				
		/*		for(count = 0; count < stream_index; count++) { 
					if(list_of_stream_data[count] == last_stream) {  	
						pthread_cond_signal(&stream_data_mutex); 
						list_of_stream_data[count] = -1; 	
						last_stream++; 
						last_stream %=NUM_STREAMS; 
						break; 
					}
 				}
		*/ 
				
			}
			/*
			if(sctp_sendmsg(sctp_conn_sock, (void *)buffer, strlen(buffer), NULL, 0, 0, 0, STREAM0, 0, 0) == -1) { 
				perror("server: sctp_sendmsg"); 
			}
			*/
		} 
	close(sctp_conn_sock); 
	return EXIT_SUCCESS; 
}
