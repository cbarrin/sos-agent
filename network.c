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
#include <sys/time.h>

#include "datatypes.h"
#include "common.h"
#include "arguments.h"
#include "network.h"



int init_agent(agent_t *agent) 
{
	
	init_poll(agent); 
	create_listen_sockets(agent); 

	return EXIT_SUCCESS; 
}


int init_poll(agent_t *agent)
{
	agent->event_pool = epoll_create(1); 
	if(agent->event_pool < 0)
	{
		perror("epoll_create"); 
		exit(1); 
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


int create_listen_sockets(agent_t *agent)  
{ 

	int i; 
	struct sockaddr_in servaddr; 

	agent->listen_fds.parallel_listen_sock = 
		malloc(sizeof(int) * agent->options.num_parallel_connections); 

	if(agent->listen_fds.parallel_listen_sock == NULL)
	{
		printf("Failed to malloc parallel_listen_socks\n"); 
		exit(1); 	
	}


	/*
	 *  create all listen sockets, bind, add to event poll
	 *
	 */

	if(agent->options.protocol == TCP) 
	{
		bzero( (void *) &servaddr, sizeof(servaddr) ); 
		if(agent->options.bind_ip) 
		{
			servaddr.sin_addr.s_addr = inet_addr(agent->options.bind_ip);  
		}
		else { 
			servaddr.sin_addr.s_addr = htonl (INADDR_ANY); 
		}

		servaddr.sin_family = AF_INET; 
		/* FIXME make option to specify bind ip address */ 
		if(( agent->listen_fds.host_listen_sock = 
			socket(AF_INET, SOCK_STREAM, 0)) == -1) 
		{
			perror("socket: host_listen_sock");
			exit(1); 
		}


		servaddr.sin_port = htons(TCP_PORT); 
		if( bind(agent->listen_fds.host_listen_sock,
			(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
		{
			perror("bind: host_listen_sock"); 
			exit(1); 
		}

		listen(agent->listen_fds.host_listen_sock, BACKLOG); 
		

		agent->listen_fds.event_host.events =  EPOLLIN; 
//		agent->listen_fds.event_host.data.ptr =  ?

		if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
			agent->listen_fds.host_listen_sock, &agent->listen_fds.event_host))
		{
			perror("epoll_ctl: host_listen_sock"); 
			exit(1); 
		}
		

		for(i = 0; i < agent->options.num_parallel_connections; i++) 
		{
			if(( agent->listen_fds.parallel_listen_sock[i] = 
				socket(AF_INET, SOCK_STREAM, 0)) == -1) 
			{
				perror("socket: parallel_listen_sock"); 
				exit(1); 
			}
			servaddr.sin_port = htons(PARALLEL_PORT_START + i); 
			if( bind(agent->listen_fds.parallel_listen_sock[i],
				(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
			{
				perror("bind: host_listen_sock"); 
				exit(1); 
			}
			listen(agent->listen_fds.parallel_listen_sock[i], BACKLOG); 

		}

		agent->listen_fds.event_agent.events = EPOLLIN; 
//		agent->listen_fds.event_agent.data.ptr =? 
		if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
			agent->listen_fds.parallel_listen_sock[0], &agent->listen_fds.event_agent))
		{
			perror("epoll_ctl: agent_listen_sock"); 
			exit(1); 
		}
		




	}
	return EXIT_SUCCESS; 	
}

