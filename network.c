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
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <uuid/uuid.h>


#include "uthash.h"
#include "packet.pb-c.h"
#include "common.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "controller.h"
#include "discovery.h"




int configure_stats(client_t * client) { 
	int i ; 
	client->stats.total_sent_bytes = 0;
	client->stats.total_recv_bytes = 0;
	client->stats.sent_bytes = malloc(sizeof(uint64_t) *client->num_parallel_connections);
	client->stats.recv_bytes = malloc(sizeof(uint64_t) *client->num_parallel_connections);
	client->stats.sent_packets = malloc(sizeof(uint64_t) *client->num_parallel_connections);
	client->stats.recv_packets = malloc(sizeof(uint64_t) *client->num_parallel_connections);

	if(client->stats.sent_bytes == NULL) { 
		printf("Malloc failed\n"); 
		exit(1); 
	}
	if(client->stats.recv_bytes == NULL) { 
		printf("Malloc failed\n"); 
		exit(1); 
	}
	if(client->stats.sent_packets == NULL) { 
		printf("Malloc failed\n"); 
		exit(1); 
	}
	if(client->stats.recv_packets == NULL) { 
		printf("Malloc failed\n"); 
		exit(1); 
	}


	for(i = 0; i < client->num_parallel_connections; i++) { 
		client->stats.sent_bytes[i] = 0 ; 
		client->stats.recv_bytes[i] = 0 ; 
		client->stats.sent_packets[i] = 0 ; 
		client->stats.recv_packets[i] = 0 ; 
	}
	return EXIT_SUCCESS;
}

int configure_poll(client_t * client)
{
	int i, j; 
	client->header_size = malloc(sizeof(uint32_t) * client->num_parallel_connections); 
  	client->buffered_packet = malloc(sizeof(packet_hash_t *) * client->num_parallel_connections); 
   client->agent_packet_index_in = malloc(sizeof(int) *client->num_parallel_connections); 
   client->agent_packet_queue_count =  malloc(sizeof(int)*client->num_parallel_connections); 
	client->agent_needed_header_size = malloc(sizeof(int)*client->num_parallel_connections); 
  	client->packet =  malloc(sizeof(serialized_data_t) * client->num_parallel_connections);  
	printf("buf %d queu %d \n", client->transfer_request->buffer_size, client->transfer_request->queue_size);
	
	for(i = 0; i < client->num_parallel_connections; i++) { 

		client->packet[i].serialized_data = malloc(sizeof(uint8_t) * client->transfer_request->buffer_size); 
		if(client->packet[i].serialized_data == NULL) { 
			printf("Malloc failed\n"); 
			exit(1); 
		}

		client->buffered_packet[i] = malloc(sizeof(packet_hash_t)*client->transfer_request->queue_size); 
		if(client->buffered_packet[i] == NULL) { 
			printf("Malloc failed\n"); 
			exit(1); 
		}

		client->agent_packet_queue_count[i] = 0; 
		client->packet[i].host_packet_size = 0; 	
      client->agent_packet_index_in[i] = EMPTY; 
		client->agent_needed_header_size[i] = 0; 

	   for(j = 0; j < client->transfer_request->queue_size; j++) 
      { 
      	client->buffered_packet[i][j].size = EMPTY ; 
         client->buffered_packet[i][j].in_use = 0 ; 
  			client->buffered_packet[i][j].serialized_data = malloc(sizeof(uint8_t)*client->transfer_request->buffer_size);
			if(client->buffered_packet[i][j].serialized_data == NULL) { 
				printf("malloc failed\n"); 
				exit(1); 
			}
  		} 
	}


	client->event_poll_out_host = epoll_create(1); 
	client->event_poll_out_agent = epoll_create(1); 

	client->client_event_pool = epoll_create(1); 
	if(client->client_event_pool < 0 || client->event_poll_out_agent < 0)
	{
		perror("client_event_pool"); 
		return EXIT_FAILURE; 
	}


	client->event.data.ptr = &client->host_side_event_info; 
	client->host_side_event_info.type = HOST_SIDE_DATA;  
	client->host_side_event_info.fd = client->host_sock; 
	client->host_side_event_info.client = client; 
	client->event.events = EPOLLIN; 
   client->host_fd_poll = IN; 

	if( epoll_ctl(client->client_event_pool, EPOLL_CTL_ADD, 
		client->host_sock, &client->event))
	{
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 
	}

	client->event.events = EPOLLOUT; 

	if( epoll_ctl(client->event_poll_out_host, EPOLL_CTL_ADD, 
		client->host_sock, &client->event))
	{
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 
	}


	for(i = 0; i < client->num_parallel_connections; i++)
	{ 
	 	client->event.events = EPOLLIN; 
	   client->event.data.ptr = &client->agent_side_event_info[i]; 
	   client->agent_side_event_info[i].fd = client->agent_sock[i]; 
	   client->agent_side_event_info[i].agent_id = i; 
	   client->agent_side_event_info[i].type = AGENT_SIDE_DATA;  
	   client->agent_side_event_info[i].client = client; 
      
      client->agent_fd_poll[i] =  IN; 


   	if( epoll_ctl(client->client_event_pool, EPOLL_CTL_ADD, 
   		client->agent_sock[i], &client->event))
   	{
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 
   	}

	   client->event.events = EPOLLOUT; 


   	if( epoll_ctl(client->event_poll_out_agent, EPOLL_CTL_ADD, 
   		client->agent_sock[i], &client->event))
   	{
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
   		exit(1); 
   	}

	} 

	return EXIT_SUCCESS; 
}



int init_agent(agent_t *agent) 
{
	init_poll(agent); 
	create_listen_sockets(agent); 
	init_controller_listener(&agent->controller); 

   /* add controller socket to POLL set */     
   agent->listen_fds.event_controller.events = POLLIN; 
   agent->listen_fds.event_controller.data.ptr = &agent->listen_fds.controller_message_event; 
   agent->listen_fds.controller_message_event.type = CONTROLLER_MESSAGE;
   agent->listen_fds.controller_message_event.fd = agent->controller.port;
   
   if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
      agent->controller.sock, &agent->listen_fds.event_controller)) 
   { 
      perror("");
		printf("%s %d\n", __FILE__, __LINE__); 
   	exit(1); 
   } 


   init_discovery(&agent->discovery); 



	return EXIT_SUCCESS; 
}


int init_poll(agent_t *agent)
{
   int count; 
	agent->event_pool = epoll_create(1); 
   agent->clients_hashes = NULL; 
   
   for(count = 0; count < MAX_AGENT_CONNECTIONS; count++) 
   {
      agent->agent_fd_pool[count] = EMPTY; 

   } 
   
	if(agent->event_pool < 0)
	{
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
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
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
   	exit(1); 
   }
   opts = (opts | O_NONBLOCK);
   if (fcntl(sock,F_SETFL,opts) < 0) 
	{
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
   	exit(1); 
   }
   return;
}


int create_listen_sockets(agent_t *agent)  
{ 

	int i; 
	struct sockaddr_in servaddr; 
   int yes = 1; 


	agent->listen_fds.agent_listen_sock = 
		calloc(sizeof(int) , agent->options.num_parallel_connections); 

	agent->listen_fds.agent_side_listen_event = 
			malloc(sizeof(event_info_t)*agent->options.num_parallel_connections); 

	if(agent->listen_fds.agent_listen_sock == NULL)
	{
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 	
	}


	/*
	 *  create all listen sockets, bind, add to event poll
	 *
	 */

	if(agent->options.protocol == TCP) 
	{
		bzero( (void *) &servaddr, sizeof(servaddr) ); 
		if(strlen(agent->options.bind_ip))
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
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 	
		}

		servaddr.sin_port = htons(TCP_PORT); 
		if( bind(agent->listen_fds.host_listen_sock,
			(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
		{
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 	
		}
   
      if(setsockopt(agent->listen_fds.host_listen_sock, 
         SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
      { 
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 	
      }

		listen(agent->listen_fds.host_listen_sock, BACKLOG); 
				
		agent->listen_fds.event_host.events =  EPOLLIN; 
		agent->listen_fds.event_host.data.ptr  = &agent->listen_fds.host_side_listen_event; 
		agent->listen_fds.host_side_listen_event.type = HOST_SIDE_CONNECT;

		if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
			agent->listen_fds.host_listen_sock, &agent->listen_fds.event_host))
		{
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 	
		}

		for(i = 0; i < agent->options.num_parallel_connections; i++) 
		{
			if(( agent->listen_fds.agent_listen_sock[i] = 
				socket(AF_INET, SOCK_STREAM, 0)) == -1) 
			{
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
				exit(1); 	
			}
			servaddr.sin_port = htons(PARALLEL_PORT_START + i); 
			if( bind(agent->listen_fds.agent_listen_sock[i],
				(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
			{
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
				exit(1); 	
			}

         if(setsockopt(agent->listen_fds.agent_listen_sock[i], 
            SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
         { 
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
				exit(1); 	
         }

			listen(agent->listen_fds.agent_listen_sock[i], BACKLOG); 

			agent->listen_fds.event_agent.events = EPOLLIN; 
	
			agent->listen_fds.event_agent.data.ptr =  &agent->listen_fds.agent_side_listen_event[i]; 
			agent->listen_fds.agent_side_listen_event[i].type =  AGENT_SIDE_CONNECT; 
			agent->listen_fds.agent_side_listen_event[i].fd =  i; 

			if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
				agent->listen_fds.agent_listen_sock[i], &agent->listen_fds.event_agent))
			{
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 	
			}
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

client_t * handle_host_side_connect(agent_t *agent) 
{

 	client_t *new_client = init_new_client(agent, NULL); 
//	if(!agent->options.nonOF && new_client != NULL)
//	{
			
		//get_controller_message(&agent->controller); 
//	}

	
	if(new_client != NULL) 
	{
		accept_host_side(agent, new_client); 

		if(check_for_transfer_request(agent, new_client, "CLIENT")) 
		{
	   	connect_agent_side(agent, new_client); 
		}else { 
         printf("DID NOT FIND!\n"); 
      } 
	}
	
	return new_client; 
}


int accept_host_side(agent_t *agent, client_t *new_client) 
{
	socklen_t sin_size; 
	//struct sockaddr_storage their_addr; 
	struct sockaddr_in their_addr; 
	struct epoll_event event; 

	sin_size = sizeof(their_addr); 


	if(( new_client->host_sock = accept(agent->listen_fds.host_listen_sock, 
		(struct sockaddr *) &their_addr, &sin_size)) == -1)
	{
      perror(""); 
	   printf("%s %d\n", __FILE__, __LINE__); 
      exit(1); 
	}
   
	
	strcpy(new_client->source_ip, inet_ntoa(their_addr.sin_addr)); 
	new_client->source_port = htons(their_addr.sin_port); 
	

   new_client->host_fd_poll = OFF; 
	setnonblocking(new_client->host_sock); 
	event.events = EPOLLOUT; 
   event.data.ptr =  &new_client->host_side_event_info; 
   new_client->host_side_event_info.type = HOST_CONNECTED;  
	new_client->host_side_event_info.client = new_client; 

	if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
	   new_client->host_sock, &event)) 
	{ 
	   perror("");
	   printf("%s %d\n", __FILE__, __LINE__); 
	   exit(1); 
	}


	return EXIT_SUCCESS; 
}

int find_empty_agent_sock(agent_t *agent)
{
   int count; 
   for(count = 0; count < MAX_AGENT_CONNECTIONS; count++)   
   { 
      if(agent->agent_fd_pool[count] == EMPTY) {
         return count; 
      } 
   } 
   return EMPTY; 
}

/*
   accept then add to  agent->event_pool and wait for uuid 
*/ 

int accept_agent_side( agent_t *agent, event_info_t *event_info) 
{
   socklen_t sin_size; 
   struct sockaddr_storage their_addr; 
   sin_size = sizeof(their_addr); 
   int index = find_empty_agent_sock(agent); 
	struct epoll_event event; 

   if(index < 0) 
   { 
      printf("agent sock table full!!!\n"); 
      exit(1); 
   } 

   if(( agent->agent_fd_pool[index] = 
		accept(agent->listen_fds.agent_listen_sock[event_info->fd], 
   	(struct sockaddr *) &their_addr, &sin_size)) == -1)
	{
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 	
   }

   setnonblocking(agent->agent_fd_pool[index]); 

	event.events = EPOLLOUT; 
	event.data.ptr =  &agent->agent_fd_pool_event[index]; 
	agent->agent_fd_pool_event[index].type =	AGENT_CONNECTED_UUID;  
	agent->agent_fd_pool_event[index].fd = index; 	

	if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
	   agent->agent_fd_pool[index], &event)) 
	{ 
	   perror("");
	   printf("%s %d\n", __FILE__, __LINE__); 
	   exit(1); 
	}


   return EXIT_SUCCESS; 
}


int handle_host_connected(agent_t *agent, client_t * client) 
{
   int optval; 
	socklen_t val = sizeof(optval); 

	getsockopt(client->host_sock,SOL_SOCKET, SO_ERROR, &optval, &val);   
	if(!optval)
	{
      client->host_fd_poll = IN; 

		if( epoll_ctl(agent->event_pool, EPOLL_CTL_DEL, 
	   	client->host_sock, NULL)) 
		{ 
				//printf("%d\n", client->host_sock); 
	   		perror("");
	   		printf("%s %d\n", __FILE__, __LINE__); 
	   		exit(1); 
   	}
		/* all sockets have been connected go go go!!! */
		//if(client->num_parallel_connections == agent->options.num_parallel_connections
   
		if(client->allowed_connections && client->num_parallel_connections == client->allowed_connections
				&& client->host_fd_poll == IN)
		{
			client->client_hash.accept_start.tv_sec -= TIMEOUT;
		}

   } 
   else 
   {
   
      printf("Failed to connect to host fd= %d %d\n", client->host_sock, client->host_fd_poll); 
      perror(""); 
	   printf("%s %d\n", __FILE__, __LINE__); 
     	if( epoll_ctl(agent->event_pool, EPOLL_CTL_DEL, 
	   	client->host_sock, NULL)) 
		{ 
	   		perror("");
	   		printf("%s %d\n", __FILE__, __LINE__); 
	   		exit(1); 
   	}
      client->host_fd_poll = OFF; 
      client->client_hash.accept_start.tv_sec -= TIMEOUT; 
   } 

	 



   return EXIT_SUCCESS; 	
}

int connect_host_side(agent_t *agent, client_t *new_client)
{

   struct sockaddr_in servaddr; 
	struct epoll_event event; 

	if(!agent->options.nonOF && new_client != NULL)
	{
//		get_controller_message(&agent->controller); 
	}

   if(( new_client->host_sock =  socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
		perror(""); 
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 	
   }
   bzero( (void *) &servaddr, sizeof(servaddr)); 
   servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(new_client->agent_ip);
   servaddr.sin_port = htons(new_client->agent_port); 

   if(agent->options.verbose_level)
   {
      printf("connecting to server [%s:%d]\n", new_client->agent_ip, new_client->agent_port); 
   }


   setnonblocking(new_client->host_sock); 
	event.events = EPOLLOUT; 
   event.data.ptr =  &new_client->host_side_event_info; 
   new_client->host_side_event_info.type = HOST_CONNECTED;  
	new_client->host_side_event_info.client = new_client; 

	if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
	   new_client->host_sock, &event)) 
	{ 
	   perror("");
	   printf("%s %d\n", __FILE__, __LINE__); 
	   exit(1); 
	}

   new_client->host_fd_poll = OFF; 

   if(connect(new_client->host_sock, 
      (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
   {
      if(errno != EINPROGRESS)
      { 
		   perror(""); 
	      printf("%s %d\n", __FILE__, __LINE__); 
         exit(1); 
      } 
   }   

   return EXIT_SUCCESS;   
}

int connect_agent_side(agent_t *agent, client_t *new_client) 
{
	int count; 
	struct sockaddr_in servaddr; 
	struct epoll_event event; 

	bzero( (void *) &servaddr, sizeof(servaddr) ); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(new_client->agent_ip);


	for(count = 0; count < new_client->allowed_connections; count++) 
	{
		if(agent->options.protocol == TCP)
		{
			if(( new_client->agent_sock[count] = 
				socket(AF_INET, SOCK_STREAM, 0)) == -1)
			{
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
				exit(1); 	
			}
			setnonblocking(new_client->agent_sock[count]); 		
			event.events = EPOLLOUT; 
			event.data.ptr =  &new_client->agent_side_event_info[count]; 
			new_client->agent_side_event_info[count].type =	AGENT_CONNECTED;  
			new_client->agent_side_event_info[count].fd = count; 	
			new_client->agent_side_event_info[count].client = new_client; 

			if( epoll_ctl(agent->event_pool, EPOLL_CTL_ADD, 
				new_client->agent_sock[count], &event)) 
			{ 
				perror("");
				printf("%s %d\n", __FILE__, __LINE__); 
				exit(1); 
			}

			servaddr.sin_port = htons(PARALLEL_PORT_START + count); 
			if( connect(new_client->agent_sock[count], 
				(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
			{
//				perror("client to agent connect"); 
//				exit(1); 
			}
			new_client->agent_fd_poll[count] = OFF;  
		}
	}
	new_client->num_parallel_connections = 0; 
	return EXIT_SUCCESS; 
}


int send_uuid(int fd, uuid_t uuid)
{
   uint8_t sent_bytes=0; 
   int size; 
   while(1) 
   {
      size = send(fd, uuid + sent_bytes, sizeof(uuid_t)-sent_bytes, 0); 
      if(size == -1) 
		{
         perror("");
		   if(errno != EAGAIN) 
         {
			   perror("");
			   printf("%s %d\n", __FILE__, __LINE__); 
			   exit(1); 
         }
       }
       else 
       {
         sent_bytes +=size; 
       } 
       if(sent_bytes == sizeof(uuid_t)) 
       {
           break;
       }
   }
   return EXIT_SUCCESS; 
}

int get_uuid(int fd, uuid_t * uuid) 
{
   uint8_t recv_bytes=0; 
   int size; 
   while(1) 
   {
      size = recv(fd, uuid + recv_bytes, sizeof(uuid_t)-recv_bytes, 0); 
      if(size == -1) 
		{
	      if(errno != EAGAIN) 
         {
			   perror("");
			   printf("%s %d\n", __FILE__, __LINE__); 
			  	exit(1); 
         }
      }
      else 
      {
         recv_bytes +=size; 
      } 
      if(recv_bytes == sizeof(uuid_t)) 
      {
         break;
      }
   }

#ifdef DEBUG
   char buf[100]; 
   uuid_unparse(*uuid, buf); 
   printf("%s\n", buf); 
#endif 

   return EXIT_SUCCESS;   
}

int get_uuid_and_confirm_client(agent_t *agent, int fd) 
{
   client_t * new_client; 
   struct client_hash_struct *client_hash;
   uuid_t uuid;
   
   get_uuid(agent->agent_fd_pool[fd], &uuid); 
     
   HASH_FIND_INT(agent->clients_hashes, uuid, client_hash); 

   if(client_hash == NULL) 
   {
 	   new_client = init_new_client(agent, &uuid); 
      new_client->agent_sock[new_client->num_parallel_connections] = 
         agent->agent_fd_pool[fd]; 
      new_client->agent_fd_poll[new_client->num_parallel_connections] = IN; 
      new_client->num_parallel_connections++; 

		if(check_for_transfer_request(agent, new_client, "AGENT")) {  
      	connect_host_side(agent, new_client); 
		}else { 
#ifdef DEBUG
         printf("DID NOT FIND!\n");  
#endif
      } 
      new_client->host_fd_poll  = OFF; 

   } 
   else 
   { 
      client_hash->client->agent_sock[client_hash->client->num_parallel_connections] = 
         agent->agent_fd_pool[fd]; 

      client_hash->client->agent_fd_poll[client_hash->client->num_parallel_connections] = IN; 
      client_hash->client->num_parallel_connections++; 
  
//     if(client_hash->client->num_parallel_connections == agent->options.num_parallel_connections 
     if(client_hash->client->allowed_connections && client_hash->client->num_parallel_connections == client_hash->client->allowed_connections 
         && client_hash->client->host_fd_poll == IN)
     {
       client_hash->accept_start.tv_sec -=  TIMEOUT; 
     }
   }

 

   if(epoll_ctl(agent->event_pool, EPOLL_CTL_DEL, 
     agent->agent_fd_pool[fd], NULL)) 
   {
      perror("");
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 
   }

   agent->agent_fd_pool[fd] = EMPTY;  

   return EXIT_SUCCESS; 
}



int agent_connected_event(agent_t *agent, event_info_t *event_info)
{ 
   
	int optval; 
	socklen_t val = sizeof(optval); 
   client_t *new_client = event_info->client; 

	getsockopt(new_client->agent_sock[event_info->fd],SOL_SOCKET, SO_ERROR, &optval, &val);   
	if(!optval)
	{
  		new_client->num_parallel_connections++; 
		new_client->agent_fd_poll[event_info->fd] = IN;  
      send_uuid(new_client->agent_sock[event_info->fd], new_client->uuid); 
	}
	else 
	{
      printf("%d\n", errno); 
		perror(""); 
	//	printf("failed %d\n", event_info->fd); 
	   printf("%s %d\n", __FILE__, __LINE__); 
      //exit(1); 
	}
	if(epoll_ctl(agent->event_pool, EPOLL_CTL_DEL, 
		new_client->agent_sock[event_info->fd], NULL))
	{
		perror("");
		printf("%s %d\n", __FILE__, __LINE__); 
		exit(1); 
	}	
//   if(new_client->num_parallel_connections == agent->options.num_parallel_connections
   if(new_client->allowed_connections && new_client->num_parallel_connections == new_client->allowed_connections 
      && new_client->host_fd_poll == IN)
   { 
	   //set time back so it will be expired next time time is checked
	   new_client->client_hash.accept_start.tv_sec -=   TIMEOUT; 

   }
   return EXIT_SUCCESS; 
}


int clean_up_unconnected_parallel_sockets(agent_t *agent, client_t *client)
{
	/* do some clean up for sockets that didn't connect */ 
   int count; 
	for(count = 0; count < agent->options.num_parallel_connections; count++)
	{ 
		if(client->agent_fd_poll[count] == OFF)
		{ 
			close(client->agent_sock[count]); 
		} 
	} 
	return EXIT_SUCCESS; 
}

client_t * init_new_client(agent_t *agent, uuid_t * uuid) 
{
//   int i,j; 
	client_t *new_client; 	
	new_client = calloc(sizeof(client_t),1); 
   if(new_client == NULL) 
   { 
      printf("Malloc failed!\n"); 
      exit(1); 
   } 
   new_client->buffered_packet_table = NULL; 
   new_client->allowed_connections = 0; 
	new_client->agent_sock = calloc(sizeof(int) , agent->options.num_parallel_connections); 
	new_client->agent_side_event_info = calloc(sizeof(struct event_info_struct) ,agent->options.num_parallel_connections); 
   new_client->send_seq = 0; 
   new_client->recv_seq = 0; 


   new_client->agent_fd_poll = calloc(sizeof(char) , agent->options.num_parallel_connections); 
   new_client->num_parallel_connections = 0; 


   new_client->client_hash.client =  new_client; 
   gettimeofday(&new_client->client_hash.accept_start, NULL);
   if(uuid == NULL)
   {
      uuid_generate(new_client->client_hash.id);  
   } 
   else 
   {
      memcpy(new_client->client_hash.id, *uuid, sizeof(uuid_t)); 
   } 
      HASH_ADD_INT(agent->clients_hashes, id, (&new_client->client_hash)); 

	return new_client; 
}


int serialize_packet(Packet *packet, event_info_t *event, uint8_t *payload, size_t  size, uint8_t *serialized_data) { 
	packet->seq_num = event->client->send_seq; 
	packet->payload.data = payload; 
	packet->payload.len = size; 
	packet__pack(packet, serialized_data); 	

	event->client->send_seq++; 


	return EXIT_SUCCESS; 
}


int read_host_send_agent(agent_t * agent, event_info_t *event_host, event_info_t *event_agent)
{
	int size, ret; 
	uint32_t n_size=0; 
   int size_count; 


   if(!event_host->client->packet[event_agent->agent_id].host_packet_size)
   {
	   if(( size = recv(event_host->fd, 
			event_host->client->packet[event_agent->agent_id].serialized_data + 2*sizeof(int), 
			event_host->client->transfer_request->buffer_size - 2*sizeof(int), 0)) == -1) 
	   {
		   if(errno == EAGAIN) { 
				printf("Eagain?? %d\n", event_agent->agent_id); 
				return EXIT_SUCCESS; 
			}   

         if (errno == ESHUTDOWN || errno == ECONNRESET) 
         {
            
            printf("Reset23\n"); 
            if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_DEL, 
               event_host->fd, NULL)) 
            {  
			      perror(""); 
			      printf("%s %d\n", __FILE__, __LINE__); 
			      exit(1); 	
            }
           event_host->client->host_fd_poll = CLOSED; 
           return CLOSE; 
         }
           
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 	
	   }
	   if(!size) {
         if(event_host->client->host_fd_poll != OFF)
         {
            if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_DEL, 
               event_host->fd, NULL))  
            {
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
               exit(1); 
            }
            event_host->client->host_fd_poll = CLOSED; 
         } 
         else { printf("LKJLKJL!!!!!!!!!!!!\n");  } 

         return CLOSE; 
	   }
		/*STATS UPDATE */ 



		event_host->client->stats.total_recv_bytes +=size; 
	   /* send size of data and then serialized data */
      n_size = htonl(size + sizeof(int));  
      memcpy(event_host->client->packet[event_agent->agent_id].serialized_data, &n_size, sizeof(size)); 
      memcpy(event_host->client->packet[event_agent->agent_id].serialized_data + sizeof(int), &event_host->client->send_seq, sizeof(int)); 
//      printf("%d %d\n", (int)*(event_host->client->packet[event_agent->agent_id].serialized_data +sizeof(int)), event_host->client->send_seq); 
		event_host->client->send_seq++; 
//      printf("test === %d %d\n",(int) event_host->client->packet[event_agent->agent_id].serialized_data, size) ; 
      //size +=sizeof(size);  
		size += sizeof(int) *2; 
      event_host->client->packet[event_agent->agent_id].host_packet_size =  size; 
      size_count = 0; 

//		event_host->client->stats.total_recv_bytes +=size -sizeof(int)*2; 
		event_host->client->stats.sent_bytes[event_agent->agent_id] +=  size; 
		event_host->client->stats.sent_packets[event_agent->agent_id]++; 

   } 
   else 
   {
      size_count = event_host->client->packet[event_agent->agent_id].host_sent_size; 
      size = event_host->client->packet[event_agent->agent_id].host_packet_size; 

      /* we want to remove POLLOUT */ 
      if(event_host->client->agent_fd_poll[event_agent->agent_id] == INAndOut) 
      {
	      event_host->client->event.events = EPOLLIN; 
	      event_host->client->event.data.ptr = &event_host->client->agent_side_event_info[event_agent->agent_id]; 

         if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_MOD, 
            event_host->client->agent_sock[event_agent->agent_id], 
            &event_host->client->event))
         {
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
            exit(1); 
         }
         event_host->client->agent_fd_poll[event_agent->agent_id] = IN;  
      }
      else if(event_host->client->agent_fd_poll[event_agent->agent_id] == OUT)
      {
         if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_DEL, 
            event_host->client->agent_sock[event_agent->agent_id], 
            NULL))
         {
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
            exit(1); 
         }
         event_host->client->agent_fd_poll[event_agent->agent_id] = OFF;  
      }
      else 
      {
         printf("ERROROROROR111\n"); 
         exit(1); 
      } 

   }
   while(1)  
   {
      ret = send(event_host->client->agent_sock[event_agent->agent_id],
         event_host->client->packet[event_agent->agent_id].serialized_data + size_count, size - size_count, 0); 
		
      if(ret == -1) 
		{
		   if(errno == EAGAIN)
			{

            event_host->client->packet[event_agent->agent_id].host_sent_size = size_count;               
            event_host->client->packet[event_agent->agent_id].host_packet_size = size;               
            
#ifdef DEBUG
				printf("removed %d\n", event_agent->agent_id); 
#endif 

	         event_host->client->event.data.ptr = &event_host->client->agent_side_event_info[event_agent->agent_id]; 

            /* we need to pollout on this FD  */ 
            if(event_host->client->agent_fd_poll[event_agent->agent_id] == IN )
            {
	            event_host->client->event.events = EPOLLOUT | EPOLLIN;

               if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_MOD, 
                  event_host->client->agent_sock[event_agent->agent_id], 
                  &event_host->client->event))
               {
						perror(""); 
						printf("%s %d\n", __FILE__, __LINE__); 
            		exit(1); 

               } 
               event_host->client->agent_fd_poll[event_agent->agent_id] = INAndOut; 
            }
            else if(event_host->client->agent_fd_poll[event_agent->agent_id] == OFF)
            {
	            event_host->client->event.events = EPOLLOUT;

               if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_ADD, 
                  event_host->client->agent_sock[event_agent->agent_id], 
                  &event_host->client->event))
               {
						perror(""); 
						printf("%s %d\n", __FILE__, __LINE__); 
            		exit(1); 
               }
               event_host->client->agent_fd_poll[event_agent->agent_id] = OUT; 
            }
           
				return EXIT_SUCCESS; 
         }
         else 
         { 
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
            
            if (errno == ESHUTDOWN || errno == ECONNRESET) 
            {
               printf("%d reset \n", event_agent->agent_id); 
               if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_DEL, 
                event_host->client->agent_sock[event_agent->agent_id], NULL)) 
               {
						perror(""); 
						printf("%s %d\n", __FILE__, __LINE__); 
            		exit(1); 
               }
               event_host->client->agent_fd_poll[event_agent->agent_id] = OFF; 
               close(event_host->client->agent_sock[event_agent->agent_id]); 
               return CLOSE; 
            }
            else 
            {
               exit(1); 
            }
			}
		}
      else 
      {
         size_count +=ret; 
      } 
      if(size_count == size)
      {
         break; 
      } 
	}
   event_host->client->packet[event_agent->agent_id].host_packet_size = 0; 

			

   return EXIT_SUCCESS; 
}

int get_free_packet_index(agent_t * agent, event_info_t * event)
{
   int i; 
   for(i = 0; i < MAX_QUEUE_SIZE; i++) 
   {
      if(event->client->buffered_packet[event->agent_id][i].in_use == 0)  
      {  
         event->client->buffered_packet[event->agent_id][i].in_use = 1; 
#ifdef DEBUG
         printf("index=%d agent=%d\n", i, event->agent_id); 
#endif
         return i; 
      }
   } 
   return -1; 
   printf("NO FREE INDEXES HELP! -- agent %d\n", event->agent_id); 
   exit(1); 
}


int read_agent_send_host(agent_t * agent, event_info_t *event)
{
	int  size; 
	int size_count = 0; 
	uint32_t packet_size; 
   int agent_id;  
   int get_header = 0; 
   int packet_index; 

#ifdef DEBUG
   int i; 

  for(i = 0; i < 10; i++) {     
   printf("%d ", event->client->agent_packet_queue_count[i]); 
   }   
   printf("\n"); 
#endif

   if(event->client->agent_packet_index_in[event->agent_id] != EMPTY  &&   
		event->client->agent_needed_header_size[event->agent_id]) 
    {
      packet_index =  event->client->agent_packet_index_in[event->agent_id]; 
      size_count =  event->client->agent_needed_header_size[event->agent_id]; 
      get_header=1; 
    }
   else if(event->client->agent_packet_index_in[event->agent_id] == EMPTY && 
		event->client->agent_needed_header_size[event->agent_id] == 0)	
   {
      get_header=1; 
      packet_index = get_free_packet_index(agent, event); 
      if(packet_index < 0) { 
         printf("I SHOULDNT HAVE BEEN TRIGGERED!! %d \n", event->agent_id); 
         exit(1); 
      } 
      event->client->agent_packet_index_in[event->agent_id] = packet_index; 
    }
    if(get_header) { 
		
		event->client->agent_packet_index_in[event->agent_id]  = packet_index; 

	   while(1) { 
		   if(( size = recv(event->fd, (uint8_t *)&event->client->header_size[event->agent_id] +size_count, 
		   //if(( size = recv(event->fd, event->client->header_size[event->agent_id] +size_count, 
				sizeof(uint32_t) - size_count, 0)) == -1)  
		   { 
            if (errno == ESHUTDOWN || errno == ECONNRESET) 
            {
               printf("REST!!\n"); 
               if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
                event->fd, NULL)) 
             {  
			      perror(""); 
			      printf("%s %d\n", __FILE__, __LINE__); 
			      exit(1); 	
             }
//               event->client->host_fd_poll = OFF; 
            	event->client->agent_fd_poll[event->agent_id] = CLOSED; 
               close(event->fd); 
               return CLOSE; 
            }
			   else if(errno == EAGAIN)  { 
				   if(!size_count) 
				   {
					   printf("Weird false fire %d %d %d\n", event->client->recv_seq, event->agent_id, event->client->host_fd_poll); 	
                  exit(1); 
					  // return EXIT_SUCCESS; 
				   }
               else {
#ifdef DEBUG
						printf("blocked with size(%d) index = (%d)\n", size_count, packet_index); 
#endif
               	event->client->agent_packet_index_in[event->agent_id] = packet_index; 
      				event->client->agent_needed_header_size[event->agent_id] = size_count;  
		
               	return  EXIT_SUCCESS; 
               }
			   } 
			   else 
			   { 
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
               exit(1); 
			   }
		   }
         if(size == 0) { 
            if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
                  event->fd, NULL))
               {
						perror(""); 
						printf("%s %d\n", __FILE__, __LINE__); 
            		exit(1); 
               }
				event->client->agent_fd_poll[event->agent_id] = CLOSED; 
         	event->client->buffered_packet[event->agent_id][packet_index].in_use = 0; 
            return CLOSE; 
         }
         else if(size > 0) 
         {
		      size_count +=size; 
         }

      //printf("%d this is the sizen",  ntohl(event->client->header_size[event->agent_id])); 

		   if(size_count == sizeof(uint32_t))
		   {
				event->client->agent_needed_header_size[event->agent_id] = 0;	

			   break; 
		   }
	   }
	
	   packet_size = ntohl(event->client->header_size[event->agent_id]); 
      if(packet_size == 0) { 
         printf("PACKET_SIZE 0 ??? \n"); 
         exit(1); 
      } 

		/* UPDATE STATS */ 
		event->client->stats.recv_packets[event->agent_id]++; 
		event->client->stats.recv_bytes[event->agent_id] +=  sizeof(uint32_t) + packet_size;  
      event->client->buffered_packet[event->agent_id][packet_index].size = packet_size; 

      //if(packet_size > sizeof(event->client->buffered_packet[event->agent_id][packet_index].serialized_data))  /* FIX ME */
      if(packet_size > event->client->transfer_request->buffer_size)  /* FIX ME */
  	   {
         printf(" size: %d count: %d agent %d\n",
				packet_size, event->client->recv_seq, event->agent_id); 
   	   printf("BUFFER TO SMALL AH!\n");     
         exit(1); 
      }
	   size_count = 0; 
	

   }
   else 
   {
      packet_index = event->client->agent_packet_index_in[event->agent_id]; 
      packet_size = event->client->buffered_packet[event->agent_id][packet_index].size; 

      size_count = event->client->buffered_packet[event->agent_id][packet_index].host_sent_size;  
   }
	while(1) 
	{
		if(( size = recv(event->fd, event->client->buffered_packet[event->agent_id][packet_index].serialized_data + size_count, 
           packet_size - size_count, 0)) == -1) 		
		{
         if(errno == EAGAIN)
         {
            event->client->buffered_packet[event->agent_id][packet_index].host_sent_size = size_count; 
            return EXIT_SUCCESS; 
         }
         else 
         {
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
           	exit(1); 
         }
		}
		else 
		{
		   size_count +=size; 
		}

      if(size == 0)
      {
         if(event->client->agent_fd_poll[event->agent_id] != OFF) 
         {
            if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
               event->fd, NULL))
            {
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
           		exit(1); 
            }
        //    event->client->agent_fd_poll[event->agent_id] = OFF; 
         }
         event->client->agent_fd_poll[event->agent_id] = CLOSED; 
         return CLOSE; 

      }
	 	else if(size_count == packet_size)
		{
         event->client->agent_packet_queue_count[event->agent_id]++;  
         event->client->agent_packet_index_in[event->agent_id] = EMPTY; 
      //   event->client->buffered_packet[event->agent_id][packet_index].size = 0; 
			break;
		}
	}

//  event->client->buffered_packet[event->agent_id][packet_index].packet = packet__unpack(NULL, 
 //     packet_size, event->client->buffered_packet[event->agent_id][packet_index].serialized_data);   

	
	memcpy(&event->client->buffered_packet[event->agent_id][packet_index].seq_num, 
      event->client->buffered_packet[event->agent_id][packet_index].serialized_data,  sizeof(int)); 

	/*
	if(event->client->buffered_packet[event->agent_id][packet_index].packet == NULL)  
   {
		printf("protobuf error\n"); 
      exit(1); 
	} 
	*/ 


   agent_id = event->agent_id; 
#ifdef DEBUG
   printf("got %d--- %d\n", event->client->buffered_packet[agent_id][packet_index].seq_num, event->client->recv_seq); 
#endif
   

      if(event->client->agent_packet_queue_count[event->agent_id] == MAX_QUEUE_SIZE)
      { 
#ifdef DEBUG
       printf("del %d\n", agent_id); 
#endif
         if(event->client->agent_fd_poll[agent_id] == INAndOut)
         {
            event->client->event.events =  EPOLLOUT; 
		      event->client->event.data.ptr = &event->client->agent_side_event_info[event->agent_id]; 
            if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
            event->fd, &event->client->event))
            {
				   perror(""); 
				   printf("%s %d\n", __FILE__, __LINE__); 
               exit(1); 
            }
            event->client->agent_fd_poll[agent_id] = OUT; 
         }
         else if(event->client->agent_fd_poll[agent_id] == IN)
         {
            if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL,  
               event->fd, NULL))
            {
				   perror(""); 
				   printf("%s %d\n", __FILE__, __LINE__); 
               exit(1); 
            }          
            event->client->agent_fd_poll[agent_id] = OFF; 
         }
      } 
      if(event->client->recv_seq == event->client->buffered_packet[agent_id][packet_index].seq_num) 
      {
            event->client->agent_packet_index_out = packet_index; 
		      send_data_host(agent, event, 0); 
      }         
      else 
      { 
           /* Now we store this packet in a hash table for lookup when id is needed */       
            event->client->buffered_packet[event->agent_id][packet_index].agent_id =  event->agent_id; 
            event->client->buffered_packet[event->agent_id][packet_index].packet_index =  packet_index; 
	         event->client->buffered_packet[event->agent_id][packet_index].id = 
            event->client->buffered_packet[event->agent_id][packet_index].seq_num; 
   	      HASH_ADD_INT(event->client->buffered_packet_table, id, (&event->client->buffered_packet[event->agent_id][packet_index])); 
      }

	return EXIT_SUCCESS; 
}


int send_data_host(agent_t *agent,  event_info_t *event, int remove_fd) 
{
   
	int size, size_count; 
   packet_hash_t *send_packet; 
	int agent_id = event->agent_id;
   int packet_index; 

   packet_index = event->client->agent_packet_index_out; 

	if(remove_fd)
	{
      if(event->client->host_fd_poll == INAndOut)
      {
		   event->client->event.events = EPOLLIN; 
		   event->client->event.data.ptr = &event->client->host_side_event_info; 

  		   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
  			   event->client->host_sock, 
  			   &event->client->event))
  		   {
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
            exit(1); 
		   }
         event->client->host_fd_poll = IN; 
      }
      else if(event->client->host_fd_poll == OUT)
      {
         if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
  			   event->client->host_sock, 
  			   NULL))
  		   {
				perror(""); 
				printf("%s %d\n", __FILE__, __LINE__); 
            exit(1); 
		   }
         event->client->host_fd_poll = OFF; 
      }
		size_count = event->client->buffered_packet[event->agent_id][packet_index].host_sent_size; 
	}
	else 
	{
		size_count = 0; 
	} 
	while(1)
	{
		while(1) 
		{
			size = send(event->client->host_sock,(uint8_t *) 
            //event->client->buffered_packet[agent_id][packet_index].packet->payload.data + size_count,  
            event->client->buffered_packet[agent_id][packet_index].serialized_data +sizeof(int) + size_count,  
     		event->client->buffered_packet[agent_id][packet_index].size -sizeof(int) - size_count, 0);  

			if(size == -1) 
			{ 
				if(errno == EAGAIN) 
         	{	
#ifdef DEBUG
               printf("EAGAIN %d\n", event->client->recv_seq); 
#endif 
					event->client->buffered_packet[agent_id][packet_index].host_sent_size = size_count; 	
               event->client->agent_packet_index_out =  packet_index; 
               /* Got blocked writing on host_sock need to now poll out! */ 

               event->client->host_side_event_info.agent_id = agent_id; 
               if(event->client->host_fd_poll == IN) 
               {
            
	         	   event->client->event.events = EPOLLOUT | EPOLLIN;
	         	   event->client->event.data.ptr = &event->client->host_side_event_info; 

           		   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
            		   event->client->host_sock, 
            		   &event->client->event))
            	   {
							perror(""); 
							printf("%s %d\n", __FILE__, __LINE__); 
            			exit(1); 
            	   }
                  event->client->host_fd_poll = INAndOut; 
               }
               else if(event->client->host_fd_poll == OFF) 
               {
                  event->client->event.events = EPOLLOUT;
	         	   event->client->event.data.ptr = &event->client->host_side_event_info; 
           		   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_ADD, 
            		   event->client->host_sock, 
            		   &event->client->event))
            	   {
							perror(""); 
							printf("%s %d\n", __FILE__, __LINE__); 
            			exit(1); 
            	   }
                  event->client->host_fd_poll = OUT; 
               }
               else {   
                  printf(" AIHLKJKj %d %d\n", event->client->host_fd_poll, remove_fd) ; exit(1); 
               } 

               return EXIT_SUCCESS; 
            }
				else 
				{ 

               if (errno == ESHUTDOWN || errno == ECONNRESET || errno == EPIPE) 
               {
                 if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
                     event->client->host_sock, NULL)) 
                  {  
			            perror(""); 
			            printf("%s %d\n", __FILE__, __LINE__); 
			            exit(1); 	
                  }
                  event->client->host_fd_poll = CLOSED; 
						close(event->client->host_sock); 
                  
                 return CLOSE; 
               }
 

					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 
				} 
			} 		
			else 
			{
				size_count +=size; 
			}
			if(size_count == event->client->buffered_packet[agent_id][packet_index].size - sizeof(int))
			{

				event->client->stats.total_sent_bytes += size_count; 	
				//packet__free_unpacked(event->client->buffered_packet[agent_id][packet_index].packet, NULL); 
#ifdef DEBUG
            printf("sent %d packet_index %d agent %d\n", event->client->recv_seq, packet_index, agent_id); 
#endif 
            event->client->agent_packet_queue_count[agent_id]--;   
            event->client->buffered_packet[agent_id][packet_index].size = 0; 
            event->client->buffered_packet[agent_id][packet_index].in_use = 0; 
				event->client->recv_seq++; 
         
            if(!(event->client->agent_fd_poll[agent_id]&IN))
            {
	         	event->client->event.data.ptr = &event->client->agent_side_event_info[agent_id]; 

               if(event->client->agent_fd_poll[agent_id] == OUT)
               {
                  event->client->event.events = EPOLLIN | EPOLLOUT; 
                  if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
                     event->client->agent_sock[agent_id], &event->client->event))
                  {
							perror(""); 
							printf("%s %d\n", __FILE__, __LINE__); 
							exit(1); 
                  }
                  event->client->agent_fd_poll[agent_id] = INAndOut; 
               }
               else if(event->client->agent_fd_poll[agent_id] == OFF)
               {
                  event->client->event.events = EPOLLIN; 
                  if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_ADD, 
                    event->client->agent_sock[agent_id], &event->client->event))
                  {

							perror(""); 
							printf("%s %d\n", __FILE__, __LINE__); 
							exit(1); 
                  } 
                  event->client->agent_fd_poll[agent_id] = IN; 
               }
               //else { printf("kjkjkj\n"); exit(1); } 


            }

				break; 
			}
		} 

		/* Check to see if we already have the next packet buffered */ 
		HASH_FIND_INT(event->client->buffered_packet_table, &event->client->recv_seq, send_packet);    
      if(send_packet != NULL)  
      {
        	agent_id = send_packet->agent_id;  
         packet_index =  send_packet->packet_index; 
#ifdef DEBUG
      	printf("Found %d agent=%d index=%d\n ", event->client->recv_seq,agent_id, packet_index ); 
#endif 
        	HASH_DEL(event->client->buffered_packet_table, send_packet); 
			size_count = 0; 
        
		}
		else 
		{ 
#ifdef DEBUG
         printf("did not find %d\n", event->client->recv_seq); 
#endif
			return EXIT_SUCCESS; 
		} 
	}
}


int clean_up_connections(client_t *client)
{
   int i; 
   close(client->host_sock); 
   for(i = 0; i < client->num_parallel_connections; i++)
   {
      close(client->agent_sock[i]); 
   }
   return EXIT_SUCCESS; 
}

int close_listener_sockets(agent_t *agent)
{
	int i; 
	close(agent->listen_fds.host_listen_sock); 
	for(i = 0; i < agent->options.num_parallel_connections; i++)
	{
		close(agent->listen_fds.agent_listen_sock[i]); 
	}
	free(agent->listen_fds.agent_listen_sock); 

	return EXIT_SUCCESS; 
}


int close_all_data_sockets(agent_t * agent, client_t * client)
{
	int i; 
   close(client->host_sock); 
	for(i = 0; i < agent->options.num_parallel_connections; i++)
	{
		close(client->agent_sock[i]); 
	}
	return EXIT_SUCCESS; 
}

int free_client(agent_t *agent, client_t * client) 
{
	free(client->agent_sock); 
	free(client->agent_side_event_info); 
	free(client->buffered_packet); 
	free(client->packet); 
	free(client->agent_fd_poll); 
	return EXIT_SUCCESS; 
}

void getinfo(client_t *client) { 
	int i ; 
	printf("num_connections: %d\n", client->transfer_request->allowed_connections); 
	printf("buffer_size: %d\n", client->transfer_request->buffer_size); 
	printf("queue_size: %d\n", client->transfer_request->queue_size); 
	printf("total_sent_bytes: %" PRIu64 "\n", client->stats.total_sent_bytes); 
	printf("total_recv_bytes: %" PRIu64 "\n", client->stats.total_recv_bytes); 


	printf(" i, sent_bytes, recv_bytes, sent_packets, recv_packets\n"); 
	for(i = 0; i < client->num_parallel_connections; i++) { 
		printf("%d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", i, client->stats.sent_bytes[i], client->stats.recv_bytes[i], 
				client->stats.sent_packets[i], client->stats.recv_packets[i]); 
	}


}
