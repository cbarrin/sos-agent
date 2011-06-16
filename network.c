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


#include "common.h"
#include "datatypes.h"
#include "list.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"



int handle_tcp_accept(options_t *options) 
{
	if(!tcp_socket_server_accept(options)) 
	{ 
		if(!create_sctp_sockets_client(options)) 
		{ 
			if(options->verbose) 
			{ 
				printf("TCP Socket created and accepted client on...SCTP connects finished!\n"); 	
			}
		}
	}
	return EXIT_SUCCESS; 
}

int handle_parallel_accept(options_t *options) 
{ 
	if(!parallel_server_accept(options)) 
	{ 
		if(!create_tcp_socket_client(options)) 
		{ 
			if(options->verbose) 
			{ 
				printf("Other end accepted our TCP connection\n"); 	
			}
		}
	}
	return EXIT_SUCCESS; 
}



int create_parallel_server_listen(options_t *options) 
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
		if(( options->parallel_listen_socks[count] = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) { 
			perror("creating p_listen_sock");   
			exit(1); 
		}
	}		
	// we only poll on socket 0 because if we get a hit here the other fd should
	// fire as well. Perhaps this isn't idle FIXME
	/* Accept connection from any interface */ 	

	bzero( (void *)&servaddr, sizeof(servaddr) ); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY); 

	for(count = 0; count < options->num_parallel_sock; count++)  
	{
		servaddr.sin_port = htons (PORT_START + count); 
		if(bind(options->parallel_listen_socks[count], (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) 
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
		if(setsockopt( options->parallel_listen_socks[count], IPPROTO_SCTP, 
			SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1) 
		{ 
			perror("server setsockopt"); 
			return EXIT_SUCCESS; 
		}
	} 
	
	for( count = 0; count < options->num_parallel_sock; count++) 
	{ 
		listen( options->parallel_listen_socks[count], BACKLOG); 
	} 


	options->parallel_ev.events = EPOLLIN; 	
	options->parallel_ev.data.ptr = (void *)PARALLEL_SOCK_LISTEN;

	if(epoll_ctl(options->epfd_accept, EPOLL_CTL_ADD, options->parallel_listen_socks[0], 
			&options->parallel_ev))   { 
			perror("epoll_ctl sctp"); 
			exit(1); 
		} 

	return EXIT_SUCCESS; 
}


int parallel_server_accept(options_t *options)  
{ 

	int count; 
	socklen_t sin_size; 
	struct sockaddr_storage their_addr; 
	
	
	if(options->verbose) 
	{
		printf("Waiting for SCTP  connections\n"); 
	} 


	sin_size = sizeof(their_addr); 

	for(count = 0; count < options->num_parallel_sock; count++) 
	{ 
		if((options->parallel_sock[count] = accept( options->parallel_listen_socks[count], 
			(struct sockaddr *) &their_addr, &sin_size)) == -1) 
		{
			perror("Accepting sockets!"); 
			return EXIT_FAILURE; 
		}


	}


	if(options->verbose) 
	{
		printf("All SCTP connections accepted!\n"); 
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
		if(( options->parallel_sock[count] = 
				socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) 
		{ 
			perror("creating p_listen_sock");   
			exit(1); 
		}
	} 


	/* specifiy the maximum number of streams that will be available per socket */ 
	memset( &initmsg, 0, sizeof(initmsg) ); 
	initmsg.sinit_num_ostreams = NUM_STREAMS; 
	initmsg.sinit_max_instreams = NUM_STREAMS; 
	initmsg.sinit_max_attempts = NUM_STREAMS -1; 

	for(count = 0; count < options->num_parallel_sock; count++)   
	{
		if(setsockopt (options->parallel_sock[count], IPPROTO_SCTP,
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
		if( connect( options->parallel_sock[count], 
			(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) 
		{ 
			perror("client: connect"); 
			return EXIT_FAILURE; 
		}
	} 
	return EXIT_SUCCESS; 
}


int create_tcp_socket_client(options_t *  options) 
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
		if ((options->tcp_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
			perror("socket_tcp"); 
			continue; 
		}
		if (connect(options->tcp_sock, p->ai_addr, p->ai_addrlen) == -1) { 
			close(options->tcp_sock); 
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


int create_tcp_server_listen(options_t * options) 
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
	
	if(listen(options->tcp_listen_sock, BACKLOG) == -1) { 
		perror("client: listen_tcp"); 
		return EXIT_FAILURE; 
	} 	

	//data_hints->type = TCP_SOCK_LISTEN; 
	options->tcp_listen_ev.events = EPOLLIN; 
	options->tcp_listen_ev.data.ptr = (void *)TCP_SOCK_LISTEN; 

	if(epoll_ctl(options->epfd_accept,
			 EPOLL_CTL_ADD, options->tcp_listen_sock,
			 &options->tcp_listen_ev)) 
	{ 
		perror("epoll_ctl create_tcp_server_listen"); 
		exit(1); 
	}
	freeaddrinfo(servinfo); 
	return EXIT_SUCCESS; 
}

int tcp_socket_server_accept(options_t * options) 
{ 


	socklen_t sin_size; 
	struct sockaddr_storage their_addr; 
	sin_size = sizeof(their_addr); 

	if((options->tcp_sock = accept
		(options->tcp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) { 
		perror("accept_tcp"); 	
		return EXIT_FAILURE; 
	}	

	return EXIT_SUCCESS; 
}

int close_data(options_t * options) 
{ 
	int count; 
	close(options->tcp_sock); 
	for(count = 0; count < options->num_parallel_sock; count++) 
	{ 
		close(options->parallel_sock[count]); 
	} 
	return EXIT_SUCCESS; 
}

int configure_epoll(options_t *options) 
{ 
	int count; 
	options->epfd_data = epoll_create(options->num_parallel_sock + 1); 
	if(options->epfd_data < 0) 
	{ 
		perror("epoll_data"); 	
		exit(1); 
	}
	options->last_read_fd = 0; 
	options->last_write_fd = 0 ;

	options->tcp_ev.data.ptr = (void *)TCP_SOCK_DATA; 
	options->tcp_ev.events = EPOLLIN; 

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD, 
			options->tcp_sock, 
			&options->tcp_ev)) 
	{ 
		perror("epoll_ctl tcp_socket_server_accept");  
		exit(1); 
	} 

	options->parallel_ev.events = EPOLLIN; 
	options->parallel_ev.data.ptr = (void *) PARALLEL_SOCK_DATA; 

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD,
			options->parallel_sock[0], 
			&options->parallel_ev)) 
	{ 
		perror("epoll_ctl  create_sctp_sockets_client"); 
		exit(1); 
	}

	// close listen sockets (child doesn't need to know about these) 
	for(count = 0; count < options->num_parallel_sock; count++) 
	{ 
		close(options->parallel_listen_socks[count]); 
	} 
	free(options->parallel_listen_socks); 	
	close(options->tcp_listen_sock); 

	return EXIT_SUCCESS; 		

} 




int epoll_connections(options_t *options) 
{ 
	int nr_events; 
	struct epoll_event events;
	
	nr_events = epoll_wait(options->epfd_accept, &events,  1 , -1); 
	if(nr_events < 0) { 
		perror("epoll_wait"); 
		exit(1); 
	}
	// accept tcp 
	if( events.data.ptr == (void *)TCP_SOCK_LISTEN) {
		return TCP_SOCK_LISTEN; 
	} 
	// accept new parallel 
	else if( events.data.ptr == (void *)PARALLEL_SOCK_LISTEN) 
	{ 
		return PARALLEL_SOCK_LISTEN; 
	}
	// shouldn't happen blocking epoll_wait 
	return EXIT_FAILURE; 
}	

int epoll_data_transfer(options_t *options) 
{ 
	int nr_events, i; 
	int ret; 
	

	struct epoll_event events[2];

	while(1) { 
		nr_events = epoll_wait(options->epfd_data, events, 2 , -1); 
	
		if(nr_events < 0) { 
			perror("epoll_wait"); 
			exit(1); 
		}
		
		for(i = 0; i < nr_events; i++) { 
			if( events[i].data.ptr == (void *)TCP_SOCK_DATA) { 
				ret = read_tcp_send_parallel(options); 		
				if(ret == CLOSE_CONNECTION) { 
					remove_client(options); 		 	
					exit(1) ; 
				}
	
			} 
			else if( events[i].data.ptr == (void *) PARALLEL_SOCK_DATA) { 
				ret = read_parallel_send_tcp(options); 
				if(ret == CLOSE_CONNECTION) { 
					remove_client(options); 		 	
					exit(1) ; 
				} 
			} 
			else { 
				printf("AHHH???? invalid ptr?? [%p] events [%d] read[%d]write[%d]\n",  events[i].data.ptr, nr_events,  
							options->last_read_fd, 	options->last_write_fd); 
				exit(1); 
			}
		}
	}

}


void remove_client(options_t *options) { 
	int count; 

	// close fd and free memory
	close(options->tcp_sock); 
	for(count = 0; count < options->num_parallel_sock; count++) {
		close(options->parallel_sock[count]); 
	} 
	free(options->parallel_sock); 	
	if(options->verbose) 
	{ 
		printf("CONNECTION CLOSED!\n"); 
	}
}

int read_parallel_send_tcp(options_t *options) 
{ 
	int size; 
	int flags = 0; 
	char buffer[MAX_BUFFER]; 
	struct sctp_sndrcvinfo sndrcvinfo; 
	

	if( (size = sctp_recvmsg( options->parallel_sock[options->last_read_fd], 	
		(void *)buffer, sizeof(buffer), (struct sockaddr*) NULL, 
		0, &sndrcvinfo, &flags)) == -1) 
	{ 
		perror("sctp_recvmsg"); 
		return EXIT_FAILURE; 
	} 
	if(!size) { 
		return CLOSE_CONNECTION; 	
	} 


	if(options->data_verbose) { 
		printf("sctp_recv [%d] --> [%s]\n", options->last_read_fd, buffer); 
	} 

	if(send(options->tcp_sock, buffer, size, 0) == -1) { 
		perror("send"); 
		return EXIT_FAILURE; 
	} 	

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_DEL, 
		options->parallel_sock[options->last_read_fd], 
		&options->parallel_ev)) { 
		perror("epoll_ctl read_parallel_send_tcp EPOLL_CTL_DEL");  
		exit(1); 
	}

	increment_index(options, READ_FD); 

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD, 
		options->parallel_sock[options->last_read_fd], 
		&options->parallel_ev)) { 
		perror("epoll_ctl read_parallel_send_tcp EPOLL_CTL_ADD");  
		exit(1); 
	} 

	return EXIT_SUCCESS; 
}

int read_tcp_send_parallel(options_t *options) 
{ 
	int size; 
	char buffer[MAX_BUFFER];

	if( (size = recv(options->tcp_sock, buffer, sizeof(buffer), 0)) == -1) { 
		perror("read_tcp_send_parallel, recv"); 
		return EXIT_FAILURE; 
	} 
	if(!size) { 
		// tcp client closed connection ; 	
		return CLOSE_CONNECTION; 	
	} 

	if(options->data_verbose) { 
		printf(" TCP read -> [%s]\n", buffer); 
	} 
			
	if( (sctp_sendmsg(options->parallel_sock[options->last_write_fd], buffer, size, 
			NULL, 0, 0, 0, 0, 0, 0)) == -1) 
	{
		perror("read_tcp_send_parallel sctp_sendmsg"); 		
		return EXIT_FAILURE; 
	} 
	if(epoll_ctl(options->epfd_data, EPOLL_CTL_DEL, 
			options->parallel_sock[options->last_write_fd], 
			&options->parallel_ev)) { 
		perror("epoll_ctl read_tcp_send_parallel EPOLL_CTL_DEL"); 	
		exit(1); 
	}

	increment_index(options, WRITE_FD); 

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD, 
			options->parallel_sock[options->last_write_fd], 
			&options->parallel_ev)) { 
		perror("epoll_ctl read_tcp_send_parallel EPOLL_CTL_DEL"); 	
		exit(1); 
	} 
	
	return EXIT_SUCCESS; 
}

void increment_index(options_t *options, int type) 
{ 

	if(type == WRITE_FD) { 	
		options->last_write_fd++; 
		options->last_write_fd %=options->num_parallel_sock;  
	}
	else if (type == READ_FD) { 
		options->last_read_fd++; 
		options->last_read_fd%=options->num_parallel_sock; 
	} 
	else { 
		printf("Invalid type!\n"); 
	}
}



int init_sockets(options_t *options) 
{
	// man pages that size doesnt matter here... 
	options->epfd_accept = epoll_create(100); 

	if(options->epfd_accept < 0 ) { 
		perror("epoll_create"); 
	}

	options->parallel_listen_socks = malloc(sizeof(int) * 
			options->num_parallel_sock); 

	options->parallel_sock = malloc(sizeof(int) * 
			options->num_parallel_sock); 
	
	if( 
		options->parallel_listen_socks == NULL ||
		options->parallel_sock == NULL 
	) { 
		printf("init_socks failed to malloc something\n"); 
		exit(-1); 
	}


	return EXIT_SUCCESS; 
}


void setnonblocking(int sock)
{
   int opts;

   opts = fcntl(sock,F_GETFL);
   if (opts < 0) {
      perror("fcntl(F_GETFL)");
      exit(EXIT_FAILURE);
   }
   opts = (opts | O_NONBLOCK);
   if (fcntl(sock,F_SETFL,opts) < 0) {
      perror("fcntl(F_SETFL)");
      exit(EXIT_FAILURE);
   }
   return;
}

