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

#include "common.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "controller.h"
#include "discovery.h"
#include "packet.pb-c.h"



int init_agent(agent_t *agent) 
{
	
	init_poll(agent); 
	create_listen_sockets(agent); 
   init_discovery(&agent->discovery); 

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


	agent->listen_fds.agent_listen_sock = 
		malloc(sizeof(int) * agent->options.num_parallel_connections); 

	if(agent->listen_fds.agent_listen_sock == NULL)
	{
		printf("Failed to malloc agent_listen_socks\n"); 
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
		
		agent->listen_fds.event_host.data.ptr = (void *) HOST_SIDE_CONNECT;
		agent->listen_fds.event_host.events =  EPOLLIN; 

		if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
			agent->listen_fds.host_listen_sock, &agent->listen_fds.event_host))
		{
			perror("epoll_ctl: host_listen_sock"); 
			exit(1); 
		}
		

		for(i = 0; i < agent->options.num_parallel_connections; i++) 
		{
			if(( agent->listen_fds.agent_listen_sock[i] = 
				socket(AF_INET, SOCK_STREAM, 0)) == -1) 
			{
				perror("socket: agent_listen_sock"); 
				exit(1); 
			}
			servaddr.sin_port = htons(PARALLEL_PORT_START + i); 
			if( bind(agent->listen_fds.agent_listen_sock[i],
				(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
			{
				perror("bind: host_listen_sock"); 
				exit(1); 
			}
			listen(agent->listen_fds.agent_listen_sock[i], BACKLOG); 

		}

		agent->listen_fds.event_agent.events = EPOLLIN; 
		agent->listen_fds.event_agent.data.ptr = (void *) AGENT_SIDE_CONNECT; 

		if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
			agent->listen_fds.agent_listen_sock[0], &agent->listen_fds.event_agent))
		{
			perror("epoll_ctl: agent_listen_sock"); 
			exit(1); 
		}
	}
	return EXIT_SUCCESS; 	
}

/* get sockaddr, IPv4 or IPv6: */ 
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
 * This funtion is called when client connets to an agent
 *
 */

client_t *  handle_host_side_connect(agent_t *agent) 
{
	char data=0; 
	client_t * new_client = init_new_client(agent); 
	

	if(!agent->options.nonOF && new_client != NULL)
	{
		get_controller_message(&agent->controller); 
	}

	
	if(new_client != NULL) 
	{
		accept_host_side(agent, new_client); 


      if(write(agent->message_fd[CHILD], &data, 1) < 1) 
	   {
		   printf("Failed to signal parent\n"); 
		   exit(1); 
	   }
	   close(agent->message_fd[CHILD]); 
      close(agent->message_fd[PARENT]); 

		connect_agent_side(agent, new_client); 
	}
	
	return new_client; 
}


int accept_host_side(agent_t *agent, client_t *new_client) 
{
	socklen_t sin_size; 
	struct sockaddr_storage their_addr; 
	sin_size = sizeof(their_addr); 

	if(( new_client->host_sock = accept(agent->listen_fds.host_listen_sock, 
		(struct sockaddr *) &their_addr, &sin_size)) == -1)
	{
		perror("accept: host_sock"); 
		exit(1);
	}
	setnonblocking(new_client->host_sock); 

	new_client->event.data.ptr = &new_client->host_side_event_info; 
	new_client->host_side_event_info.type = HOST_SIDE_DATA_IN;  
	new_client->host_side_event_info.fd = new_client->host_sock; 
	new_client->host_side_event_info.client = new_client; 
	new_client->event.events = EPOLLIN; 

	if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
		new_client->host_sock, &new_client->event))
	{
		perror("epoll_ctl new_client->event"); 
		exit(1); 
	}
	return EXIT_SUCCESS; 
}


int accept_agent_side( agent_t *agent, client_t *new_client) 
{
   int count; 
   socklen_t sin_size; 
   struct sockaddr_storage their_addr; 
   sin_size = sizeof(their_addr); 

   for( count = 0; count < agent->options.num_parallel_connections; count++) 
   {
      if((  new_client->agent_sock[count] = accept(agent->listen_fds.agent_listen_sock[count], 
         (struct sockaddr *) &their_addr, &sin_size)) == -1)
      {
         perror("accept: agent_sock"); 
         exit(1); 
      }
      setnonblocking(new_client->agent_sock[count]); 
      new_client->event.data.ptr = &new_client->agent_side_event_info[count]; 
	   new_client->agent_side_event_info[count].fd = new_client->agent_sock[count]; 
	   new_client->agent_side_event_info[count].type = AGENT_SIDE_DATA_IN;  
	   new_client->agent_side_event_info[count].client = new_client; 

   	if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
   		new_client->agent_sock[count], &new_client->event))
   	{
   		perror("epoll_ctl new_client->event"); 
   		exit(1); 
   	}
   }
   return EXIT_SUCCESS; 
}


int connect_host_side(agent_t *agent, client_t *new_client)
{
   struct sockaddr_in servaddr; 
   if(( new_client->host_sock =  socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("socket: host_sock"); 
      exit(1); 
   }
   bzero( (void *) &servaddr, sizeof(servaddr)); 
   servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(agent->controller.send_ip);
   servaddr.sin_port = agent->controller.port; 
   if(agent->options.verbose_level)
   {
      printf("connecting to server [%s]\n", agent->controller.send_ip); 
   }
   setnonblocking(new_client->host_sock); 
   new_client->event.data.ptr = &new_client->host_side_event_info; 
   new_client->host_side_event_info.fd = new_client->host_sock; 
   new_client->host_side_event_info.type = HOST_SIDE_DATA_IN; 
   new_client->host_side_event_info.client = new_client; 
   
   if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
      new_client->host_sock, &new_client->event))
   {
   		perror("epoll_ctl new_client->event (host)"); 
   		exit(1); 
   }
   return EXIT_SUCCESS;   
}

int connect_agent_side(agent_t *agent, client_t *new_client) 
{
	int count; 
	struct sockaddr_in servaddr; 
	
	for(count = 0; count < agent->options.num_parallel_connections; count++) 
	{
		if(agent->options.protocol == TCP)
		{
			if(( new_client->agent_sock[count] = 
				socket(AF_INET, SOCK_STREAM, 0)) == -1)
			{
				perror("socket: agent_sock"); 
				exit(1); 
			}
		}
	}

	bzero( (void *) &servaddr, sizeof(servaddr) ); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(agent->controller.send_ip);

	if(agent->options.verbose_level)
	{
		printf("Connection parallel to [%s]\n", agent->controller.send_ip); 
	}

	new_client->event.events = EPOLLIN; 

	for (count = 0; count < agent->options.num_parallel_connections; count++)
	{
		servaddr.sin_port = htons(PARALLEL_PORT_START + count); 
		if( connect(new_client->agent_sock[count], 
			(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
		{
			perror("client to agent connect\n"); 
			exit(1); 
		}
      setnonblocking(new_client->agent_sock[count]); 
	   new_client->event.data.ptr = &new_client->agent_side_event_info[count]; 
	   new_client->agent_side_event_info[count].fd = new_client->agent_sock[count]; 
	   new_client->agent_side_event_info[count].type = AGENT_SIDE_DATA_IN;  
	   new_client->agent_side_event_info[count].client = new_client; 


   	if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
   		new_client->agent_sock[count], &new_client->event))
   	{
   		perror("epoll_ctl new_client->event (agent)"); 
   		exit(1); 
   	}

   }


	return EXIT_SUCCESS; 
}





/*
 * This function is called when an agent is connecting to 
 * another agent.
 *
 */

client_t * handle_agent_side_connect(agent_t *agent)
{
   char data=0; 
   client_t *new_client = init_new_client(agent); 

	if(!agent->options.nonOF && new_client != NULL)
	{
		get_controller_message(&agent->controller); 
	}

   if(new_client != NULL)
   {
      accept_agent_side(agent, new_client); 

      if(write(agent->message_fd[CHILD], &data, 1) < 1)
      {
         printf("Failed to signal parent\n"); 
         exit(1); 
      }
	   close(agent->message_fd[CHILD]); 
      close(agent->message_fd[PARENT]); 

      connect_host_side(agent, new_client); 
   }


	return new_client; 
}


client_t * init_new_client(agent_t *agent ) 
{ 
	client_t *new_client; 	
	new_client = malloc(sizeof(client_t)); 
	new_client->agent_sock = malloc(sizeof(int) * agent->options.num_parallel_connections); 
	new_client->agent_side_event_info = malloc(sizeof(struct epoll_event)); 
	if(new_client == NULL || new_client->agent_sock == NULL || new_client->agent_side_event_info == NULL) 
	{
		printf("Failed to malloc new client!\n"); 
		return NULL; 
	}

	new_client->client_event_pool = epoll_create(1); 
	if(new_client->client_event_pool < 0)
	{
		perror("client_event_pool"); 
		return NULL; 
	}

	return new_client; 
}


int create_packet(event_info_t *event, char *payload, char *serialized_data) { 

	Packet packet = PACKET__INIT; 

	packet.seq_num = event->client->send_seq; 
	packet.payload = payload; 
	packet__pack(&packet, (uint8_t *)serialized_data); 	

	event->client->send_seq++; 


	return EXIT_SUCCESS; 
}


int read_host_send_agent(agent_t * agent, event_info_t *event)
{
	int size; 
	int ret; 
	int unsuccessful = 1; 
	int n_size; 

	char buf[MAX_BUFFER]; 

	/*  seq_num + payload */ 
	char serialized_data[ sizeof(int) + MAX_BUFFER ]; 

	if(( size = recv(event->fd, buf, sizeof(buf), 0)) == -1) 
	{
		perror("recv: read_host_send_agent"); 
		exit(1); 
	}
	// close connection 
	if(!size) {
		
	}
	
	create_packet(event, buf, serialized_data); 
	size = packet__get_packed_size((void *)serialized_data); 	

	/* send size of data and then serialized data */ 

	n_size = htonl(size); 
	while(unsuccessful)
	{
		ret = send(event->client->agent_sock[event->client->last_fd_sent],
			&n_size, sizeof(n_size), 0); 

		if(ret == -1) 
		{ 
			if(errno == EAGAIN) 
			{ 
				event->client->last_fd_sent++; 
			}
		}
		else 
		{
			unsuccessful = 0; 

			/* we need to loop till this data get's sent size
			 * we already put the size on this fd 
			 */
			while(1) 
			{
				ret = send(event->client->agent_sock[event->client->last_fd_sent],
					serialized_data, size, 0); 
				if(ret == -1) 
				{
					if(errno != EAGAIN)
					{
						perror("send: serialzed_data"); 
						exit(1); 
					}
				}
				else 
				{
					break;
				}
			}
		}
	}
	event->client->last_fd_sent++; 
			

   return EXIT_SUCCESS; 
}



int read_agent_send_host(agent_t * agent, event_info_t *event)
{


   return EXIT_SUCCESS; 
}











