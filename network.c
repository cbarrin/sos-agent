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
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"
#include "discovery.h"



int handle_tcp_accept(options_t *options) 
{
	close(options->controller.sock); 
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
	close(options->controller.sock); 
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


	for(count = 0; count < options->num_parallel_sock; count++) 
	{ 
		if(( options->parallel_listen_socks[count] = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) 
		{ 
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


	options->parallel_ev_in.events = EPOLLIN; 	
	options->parallel_ev_in.data.ptr = (void *)PARALLEL_SOCK_LISTEN;

	if(epoll_ctl(options->epfd_accept, EPOLL_CTL_ADD, options->parallel_listen_socks[0], 
			&options->parallel_ev_in))   
	{ 
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
	//servaddr.sin_addr.s_addr = inet_addr(SCTP_CONNECT_TO_ADDR); 
	servaddr.sin_addr.s_addr = inet_addr(options->controller.send_ip); 
	
	if(options->verbose)
	{
		printf("connectioning parallel to [%s]\n", options->controller.send_ip); 
	}

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
	char port[5]; 
	memset(port, 0, sizeof(port)); 
	sprintf(port, "%hi", options->controller.port);
	printf("AHAHAH %s\n", port); 


	/* Make TCP CONNECTION */ 
	//servaddr.sin_addr.s_addr = inet_addr(options->controller.send_ip); 
	if (( ret = getaddrinfo(options->controller.send_ip, port, &hints, &servinfo)) != 0) 
	{ 
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret)); 
		return EXIT_FAILURE; 
	} 

	// loop though all results and connect to first one we can 
	for( p = servinfo; p != NULL; p = p->ai_next) 
	{ 
		if ((options->tcp_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{ 
			perror("socket_tcp"); 
			continue; 
		}
		if (connect(options->tcp_sock, p->ai_addr, p->ai_addrlen) == -1) 
		{ 
			close(options->tcp_sock); 
			perror("connect_tcp"); 
			continue; 
		}
		break; 
	}

	if(p == NULL) 
	{ 
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

	
	if (( ret = getaddrinfo(options->tcp_bind_ip, TCP_PORT, &hints, &servinfo)) != 0) 
	{
		printf("getaddrinfo: %s\n", gai_strerror(ret)); 
		return EXIT_FAILURE; 
	}

	/* Loop though all the results and vind to first one we can */ 
	for( p = servinfo; p != NULL; p = p->ai_next) 
	{ 
		if (( options->tcp_listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{ 
			perror("client: socket_tcp"); 
			continue; 
		}
		if( setsockopt(options->tcp_listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
		{ 
			perror("client: setsockopt_tcp"); 
			return EXIT_FAILURE; 
		}
		if(bind(options->tcp_listen_sock, p->ai_addr, p->ai_addrlen) == -1) 
		{ 
			perror("client: bind_tcp"); 
			continue; 
		}
		break; 
	}

	if( p == NULL) 
	{ 
		printf("TCP FAILED TO BIND!!!\n"); 	
		exit(1); 
	}
	
	if(listen(options->tcp_listen_sock, BACKLOG) == -1) 
	{ 
		perror("client: listen_tcp"); 
		return EXIT_FAILURE; 
	} 	

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
		(options->tcp_listen_sock, (struct sockaddr *) &their_addr, &sin_size)) == -1) 
	{ 
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

	options->buf_tcp_size = 0; 
	options->buf_parallel_size = 0; 

	options->last_read_fd = 0; 
	options->last_write_fd = 0 ;
	options->epfd_data = epoll_create(options->num_parallel_sock + 1); 
	options->epfd_data_out_tcp = epoll_create(1); 
	options->epfd_data_out_parallel = epoll_create(1);  

	if(options->epfd_data < 0 || 
		options->epfd_data_out_tcp < 0 || 
		options->epfd_data_out_parallel < 0) 
	{ 
		perror("epoll_data"); 	
		exit(1); 
	}

	/* setup tcp polling */ 
	options->tcp_ev_in.data.ptr = (void *)TCP_SOCK_DATA; 
	options->tcp_ev_in.events = EPOLLIN; 
	setnonblocking(options->tcp_sock);
	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD, 
			options->tcp_sock, 
			&options->tcp_ev_in)) 
	{ 
		perror("epoll_ctl tcp_configure epoll_in");  
		exit(1); 
	} 

	options->tcp_ev_out.events = EPOLLOUT; 
	if(epoll_ctl(options->epfd_data_out_tcp, 
			EPOLL_CTL_ADD, 
			options->tcp_sock, &options->tcp_ev_out))
	{
		perror("epoll_ctl tcp_configure epoll_out"); 
		exit(1); 
	} 


	/* setup parallel polling */ 

	options->parallel_ev_in.events = EPOLLIN; 
	options->parallel_ev_in.data.ptr = (void *) PARALLEL_SOCK_DATA; 

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD,
			options->parallel_sock[0], 
			&options->parallel_ev_in)) 
	{ 
		perror("epoll_ctl  configure_epoll parallel_in"); 	
		exit(1); 
	}

	options->parallel_ev_out.events = EPOLLOUT; 
	if(epoll_ctl(options->epfd_data_out_parallel, 
			EPOLL_CTL_ADD, options->parallel_sock[0], 
			&options->parallel_ev_out)) 
	{  
		perror("epoll_ctl  configure_epoll parallel_out"); 	
		exit(1); 
	} 



	// close listen sockets (child doesn't need to know about these) 
	// And set parallel sockets to nonblocking
	for(count = 0; count < options->num_parallel_sock; count++) 
	{ 
		close(options->parallel_listen_socks[count]); 
		setnonblocking(options->parallel_sock[count]);
	} 
	free(options->parallel_listen_socks); 	
	close(options->tcp_listen_sock); 
	

	return EXIT_SUCCESS; 		

} 




int epoll_connections(options_t *options) 
{ 
	int nr_events; 
	struct epoll_event events;
	int	timeout = 1000; // 10 seconds 	

	while(1) 
	{ 
		nr_events = epoll_wait(options->epfd_accept, &events,  1 , timeout); 
		if(nr_events < 0) { 
			perror("epoll_wait"); 
			exit(1); 
		}
		// no connections came in timeout se we send discovery! 
		if(nr_events == 0) 
		{ 
			send_discovery_message(&options->discovery); 
		} 
		// accept tcp 
		else if( events.data.ptr == (void *)TCP_SOCK_LISTEN) 
		{
			return TCP_SOCK_LISTEN; 
		} 
		// accept new parallel 
		else if( events.data.ptr == (void *)PARALLEL_SOCK_LISTEN) 
		{ 
			return PARALLEL_SOCK_LISTEN; 
		}
	}
	return EXIT_FAILURE; 
}		
	
/*
* This function is responsible for tranfering data
* between all of the sockets. Once a side closes we 
* set a timeout of 1 second for a reponse on the 
* other socket end. If it doesn't have data in that time
* we close both endpoints. 
* (((is 1 second enough?)))
*/ 

int epoll_data_transfer(options_t *options) 
{ 
	int nr_events, i; 
	int ret; 
	
	int timeout = -1; 
	struct epoll_event events[2];
	struct epoll_event temp;

	while(1) 
	{ 
		nr_events = epoll_wait(options->epfd_data, events, 2 , timeout); 
	
		if(nr_events < 0) 
		{ 
			perror("epoll_wait"); 
			exit(1); 
		}
		
		for(i = 0; i < nr_events; i++) 
		{ 
			if( events[i].data.ptr == (void *)TCP_SOCK_DATA) 
			{ 
				if((epoll_wait(options->epfd_data_out_parallel, &temp, 1, 0)) > 0) 
				{
					ret = read_tcp_send_parallel(options); 		
					if(ret == CLOSE_CONNECTION) 
					{ 
						timeout = 1000; // 1 second 
					}
				} 
			} 
			else if( events[i].data.ptr == (void *) PARALLEL_SOCK_DATA) 
			{ 
				if( (epoll_wait(options->epfd_data_out_tcp, &temp, 1, 0)) > 0) 
				{ 
					ret = read_parallel_send_tcp(options); 
					if(ret == CLOSE_CONNECTION) 
					{ 
						timeout = 1000;  // 1 second
					} 
				}
			} 
			else 
			{ 
				printf("Error [%p] events [%d] read[%d]write[%d]\n",  events[i].data.ptr, nr_events,  
							options->last_read_fd, 	options->last_write_fd); 
				exit(1); 
			}
		}
		if(!nr_events) 
		{ 
			remove_client(options); 		 	
			exit(1) ; 
		} 
	}
}


void remove_client(options_t *options) 
{ 
	int count; 

	// close fd and free memory
	close(options->tcp_sock); 
	for(count = 0; count < options->num_parallel_sock; count++) 
	{
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
	int ret; 
	int flags = 0; 
	struct sctp_sndrcvinfo sndrcvinfo; 
	

	if( (!options->buf_tcp_size) && ((options->buf_tcp_size = 
		sctp_recvmsg( options->parallel_sock[options->last_read_fd], 	
		(void *)options->buf_tcp_data, sizeof(options->buf_tcp_data), 
		(struct sockaddr*) NULL, 0, &sndrcvinfo, &flags)) == -1))
	{ 
		perror("sctp_recvmsg"); 
		return EXIT_FAILURE; 
	} 
	if(!options->buf_tcp_size) 
	{ 
		if(epoll_ctl(options->epfd_data, EPOLL_CTL_DEL, 
			options->parallel_sock[options->last_read_fd], 
			&options->parallel_ev_in)) 
		{ 
			perror("epoll_ctl read_parallel_send_tcp EPOLL_CTL_DEL in close");  
			exit(1); 
		}
		return CLOSE_CONNECTION; 	
	} 

	if(options->data_verbose) 
	{ 
		printf("sctp_recv [%d] --> [%s]\n",
				 options->last_read_fd, options->buf_tcp_data); 
	} 

	ret = send(options->tcp_sock, options->buf_tcp_data, 
			options->buf_tcp_size, 0); 

	if(ret == EAGAIN ||  EWOULDBLOCK == ret)  
	{ 
		// we need to hold on to data to send next time
		if(options->verbose) { 
			printf("send buffer full\n"); 
		} 
		return EXIT_SUCCESS; 
	} 	
	else if(ret == -1) 
	{
		perror("tcp_send"); 
		exit(1); 

	} 
	options->buf_tcp_size = 0; 
	if(epoll_ctl(options->epfd_data, EPOLL_CTL_DEL, 
		options->parallel_sock[options->last_read_fd], 
		&options->parallel_ev_in)) 
	{ 
		perror("epoll_ctl read_parallel_send_tcp EPOLL_CTL_DEL");  
		exit(1); 
	}

	increment_index(options, READ_FD); 

	if(epoll_ctl(options->epfd_data, EPOLL_CTL_ADD, 
		options->parallel_sock[options->last_read_fd], 
		&options->parallel_ev_in)) 
	{ 
		perror("epoll_ctl read_parallel_send_tcp EPOLL_CTL_ADD");  
		exit(1); 
	} 

	return EXIT_SUCCESS; 
}

int read_tcp_send_parallel(options_t *options) 
{ 

	int ret; 
	
	if( (!options->buf_parallel_size) && 
			((options->buf_parallel_size = 
			recv(options->tcp_sock, options->buf_parallel_data, 
			sizeof(options->buf_parallel_data), 0)) == -1)) 
	{ 
		perror("read_tcp_send_parallel, recv"); 
		return EXIT_FAILURE; 
	} 
	if(!options->buf_parallel_size) 
	{ 	
		// tcp client closed connection ; 	
		if(epoll_ctl(options->epfd_data, EPOLL_CTL_DEL, 
			options->tcp_sock, 
			&options->tcp_ev_in)) 
		{ 
				perror("epoll_ctl read_parallel_send_tcp EPOLL_CTL_DEL");  
				exit(1); 
		}
		return CLOSE_CONNECTION; 	
	} 

	if(options->data_verbose) 
	{ 
		printf(" TCP read -> [%s]\n", options->buf_parallel_data); 
	} 
			
	ret = sctp_sendmsg(options->parallel_sock[options->last_write_fd], 
			options->buf_parallel_data, options->buf_parallel_size, 
			NULL, 0, 0, 0, 0, 0, 0); 

		
	// send buffer is full we did not send
	if(ret == EAGAIN ||  EWOULDBLOCK == ret)  
	{ 
		if(options->verbose) { 
			printf("send buffer full\n"); 
		} 
		return EXIT_SUCCESS; 
		// We  need to hold on to data to send again		
	} 
	else if (ret  == -1) 
	{
		perror("read_tcp_send_parallel sctp_sendmsg"); 		
		return EXIT_FAILURE; 
	} 

	options->buf_parallel_size = 0; 
	if(epoll_ctl(options->epfd_data_out_parallel, EPOLL_CTL_DEL, 
			options->parallel_sock[options->last_write_fd], 
			&options->parallel_ev_out)) 
	{ 
		perror("epoll_ctl read_tcp_send_parallel EPOLL_CTL_DEL"); 	
		exit(1); 
	}

	increment_index(options, WRITE_FD); 

	if(epoll_ctl(options->epfd_data_out_parallel, EPOLL_CTL_ADD, 
			options->parallel_sock[options->last_write_fd], 
			&options->parallel_ev_out)) 
	{ 
		perror("epoll_ctl read_tcp_send_parallel EPOLL_CTL_ADD"); 	
		exit(1); 
	} 

	return EXIT_SUCCESS; 
}

void increment_index(options_t *options, int type) 
{ 

	if(type == WRITE_FD) 
	{ 	
		options->last_write_fd++; 
		options->last_write_fd %=options->num_parallel_sock;  
	}
	else if (type == READ_FD) 
	{ 
		options->last_read_fd++; 
		options->last_read_fd%=options->num_parallel_sock; 
	} 
	else 
	{ 
		printf("Invalid type!\n"); 
	}
}



int init_sockets(options_t *options) 
{
	// man pages that size doesnt matter here... 
	options->num_clients = 0; 
	options->epfd_accept = epoll_create(100); 

	if(options->epfd_accept < 0 ) 
	{ 
		perror("epoll_create"); 
	}

	options->parallel_listen_socks = malloc(sizeof(int) * 
			options->num_parallel_sock); 

	options->parallel_sock = malloc(sizeof(int) * 
			options->num_parallel_sock); 
	
	if( 
		options->parallel_listen_socks == NULL ||
		options->parallel_sock == NULL 
	) 
	{ 
		printf("init_socks failed to malloc something\n"); 
		exit(-1); 
	}


	return EXIT_SUCCESS; 
}


void setnonblocking(int sock)
{
   int opts;

   opts = fcntl(sock,F_GETFL);
   if (opts < 0) 
	{
      perror("fcntl(F_GETFL)");
      exit(EXIT_FAILURE);
   }
   opts = (opts | O_NONBLOCK);
   if (fcntl(sock,F_SETFL,opts) < 0) 
	{
      perror("fcntl(F_SETFL)");
      exit(EXIT_FAILURE);
   }
   return;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
