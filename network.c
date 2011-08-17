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

#include "uthash.h"
#include "packet.pb-c.h"
#include "common.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "controller.h"
#include "discovery.h"



int init_agent(agent_t *agent) 
{
	init_poll(agent); 
	create_listen_sockets(agent); 
   init_discovery(&agent->discovery); 
	init_controller_listener(&agent->controller); 

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
   int yes = 1; 


	agent->listen_fds.agent_listen_sock = 
		calloc(sizeof(int) , agent->options.num_parallel_connections); 

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
   
      if(setsockopt(agent->listen_fds.host_listen_sock, 
         SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
      { 
         perror("setsockopt"); 
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

         if(setsockopt(agent->listen_fds.agent_listen_sock[i], 
            SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
         { 
            perror("setsockopt"); 
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
	new_client->host_side_event_info.type = HOST_SIDE_DATA;  
	new_client->host_side_event_info.fd = new_client->host_sock; 
	new_client->host_side_event_info.client = new_client; 
	new_client->event.events = EPOLLIN; 
   new_client->host_fd_poll = IN; 

	if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
		new_client->host_sock, &new_client->event))
	{
		perror("epoll_ctl new_client->event"); 
		exit(1); 
	}

	new_client->event.events = EPOLLOUT; 

	if( epoll_ctl(new_client->event_poll_out_host, EPOLL_CTL_ADD, 
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
      new_client->agent_side_event_info[count].agent_id = count; 
	   new_client->agent_side_event_info[count].fd = new_client->agent_sock[count]; 
	   new_client->agent_side_event_info[count].type = AGENT_SIDE_DATA;  
	   new_client->agent_side_event_info[count].client = new_client; 
	   new_client->event.events = EPOLLIN; 
      new_client->agent_fd_poll[count] =  IN; 

   	if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
   		new_client->agent_sock[count], &new_client->event))
   	{
   		perror("epoll_ctl new_client->event"); 
   		exit(1); 
   	}

	   new_client->event.events = EPOLLOUT; 

   	if( epoll_ctl(new_client->event_poll_out_agent, EPOLL_CTL_ADD, 
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
   servaddr.sin_port = htons(agent->controller.port); 
   if(agent->options.verbose_level)
   {
      printf("connecting to server [%s:%d]\n", agent->controller.send_ip, agent->controller.port); 
   }

   printf("Connect...\n"); 


   if(connect(new_client->host_sock, 
      (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
   {
      perror("connect: host_sock"); 
      exit(1); 
   }   
   printf("Connected\n"); 

   setnonblocking(new_client->host_sock); 
	new_client->event.events = EPOLLIN; 
   new_client->event.data.ptr = &new_client->host_side_event_info; 
   new_client->host_side_event_info.fd = new_client->host_sock; 
   new_client->host_side_event_info.type = HOST_SIDE_DATA; 
   new_client->host_side_event_info.client = new_client; 
   new_client->host_fd_poll = IN; 
   
   if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
      new_client->host_sock, &new_client->event))
   {
   		perror("epoll_ctl new_client->event (host)"); 
   		exit(1); 
   }

	new_client->event.events = EPOLLOUT; 
   
   if( epoll_ctl(new_client->event_poll_out_host, EPOLL_CTL_ADD, 
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


	for (count = 0; count < agent->options.num_parallel_connections; count++)
	{
		servaddr.sin_port = htons(PARALLEL_PORT_START + count); 
      printf("Connecteding parallel %d\n", PARALLEL_PORT_START + count); 
		if( connect(new_client->agent_sock[count], 
			(struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
		{
			perror("client to agent connect\n"); 
			exit(1); 
		}

      setnonblocking(new_client->agent_sock[count]); 
	   new_client->event.events = EPOLLIN; 
	   new_client->event.data.ptr = &new_client->agent_side_event_info[count]; 
	   new_client->agent_side_event_info[count].fd = new_client->agent_sock[count]; 
	   new_client->agent_side_event_info[count].agent_id = count; 
	   new_client->agent_side_event_info[count].type = AGENT_SIDE_DATA;  
	   new_client->agent_side_event_info[count].client = new_client; 
      
      new_client->agent_fd_poll[count] =  IN; 


   	if( epoll_ctl(new_client->client_event_pool, EPOLL_CTL_ADD, 
   		new_client->agent_sock[count], &new_client->event))
   	{
   		perror("epoll_ctl new_client->event (agent)"); 
   		exit(1); 
   	}

	   new_client->event.events = EPOLLOUT; 


   	if( epoll_ctl(new_client->event_poll_out_agent, EPOLL_CTL_ADD, 
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
   int i; 
	client_t *new_client; 	
	new_client = calloc(sizeof(client_t),1); 
   new_client->buffered_packet_table = NULL; 

	new_client->agent_sock = calloc(sizeof(int) , agent->options.num_parallel_connections); 
	new_client->agent_side_event_info = calloc(sizeof(struct event_info_struct) ,agent->options.num_parallel_connections); 
   new_client->last_fd_sent = 0; 
   new_client->send_seq = 0; 
   new_client->recv_seq = 0; 
   new_client->buffered_packet = calloc(sizeof(packet_hash_t) , agent->options.num_parallel_connections); 
   new_client->packet =  calloc(sizeof(serialized_data_t) ,agent->options.num_parallel_connections);  
   new_client->agent_fd_poll = calloc(sizeof(char) , agent->options.num_parallel_connections); 

	if(new_client == NULL || 
      new_client->agent_sock == NULL ||  
      new_client->buffered_packet == NULL || 
      new_client->agent_fd_poll == NULL || 
      new_client->packet == NULL || 
      new_client->agent_side_event_info == NULL )
	{
		printf("Failed to malloc new client!\n"); 
		return NULL; 
	}


	for(i = 0; i < agent->options.num_parallel_connections; i++)
	{
		new_client->packet[i].host_packet_size = 0; 	
      new_client->buffered_packet[i].size = 0 ; 
	} 

	new_client->event_poll_out_host = epoll_create(1); 
	new_client->event_poll_out_agent = epoll_create(1); 

	new_client->client_event_pool = epoll_create(1); 
	if(new_client->client_event_pool < 0 || new_client->event_poll_out_agent < 0)
	{
		perror("client_event_pool"); 
		return NULL; 
	}

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
	uint8_t buf[MAX_BUFFER]; 

	Packet packet = PACKET__INIT; 

   if(!event_host->client->packet[event_agent->agent_id].host_packet_size)
   {
	   if(( size = recv(event_host->fd, buf, sizeof(buf), 0)) == -1) 
	   {
		   if(errno == EAGAIN) { 
				printf("Eagain??\n"); 
				return EXIT_SUCCESS; 
			} 
		   perror("recv: read_host_send_agent"); 
		   exit(1); 
	   }
	   if(!size) {
         if(event_host->client->host_fd_poll != OFF)
         {
            printf("CLOSEING HOST SOCKET!\n"); 
            if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_DEL, 
               event_host->fd, NULL))  
            {
               perror("DELL\n"); 
               exit(1); 
            }
            event_host->client->host_fd_poll = OFF; 
         } 
         else { printf("LKJLKJL!!!!!!!!!!!!\n");  } 

         return CLOSE; 
	   }
	   serialize_packet(&packet, event_host, buf, (size_t)size, 
         (uint8_t *)&event_host->client->packet[event_agent->agent_id].serialized_data[sizeof(size)]); 

	   size = packet__get_packed_size(&packet); 

	   /* send size of data and then serialized data */
      n_size = htonl(size);  
      memcpy(&event_host->client->packet[event_agent->agent_id].serialized_data, &n_size, sizeof(size)); 
       
      size +=sizeof(size);  
      event_host->client->packet[event_agent->agent_id].host_packet_size =  size; 
      size_count = 0; 
 //     printf("%d\n", size); 
   } 
   else 
   {
      size_count = event_host->client->packet[event_agent->agent_id].host_sent_size; 
      size = event_host->client->packet[event_agent->agent_id].host_packet_size; 
//		printf("ADDED %d\n", event_agent->agent_id); 

      /* we want to remove POLLOUT */ 
      if(event_host->client->agent_fd_poll[event_agent->agent_id] == INAndOut) 
      {
	      event_host->client->event.events = EPOLLIN; 
	      event_host->client->event.data.ptr = &event_host->client->agent_side_event_info[event_agent->agent_id]; 

         if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_MOD, 
            event_host->client->agent_sock[event_agent->agent_id], 
            &event_host->client->event))
         {
            perror("epoll_ctl: epoll_ctl_mod, epollin");  
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
            perror("epoll_ctl: epoll_ctl_mod, epollin");  
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
            

				//printf("Aremoved %d\n", event_agent->agent_id); 

	         event_host->client->event.data.ptr = &event_host->client->agent_side_event_info[event_agent->agent_id]; 

            /* we need to pollout on this FD  */ 
            if(event_host->client->agent_fd_poll[event_agent->agent_id] == IN )
            {
	            event_host->client->event.events = EPOLLOUT | EPOLLIN;

               if(epoll_ctl(event_host->client->client_event_pool, EPOLL_CTL_MOD, 
                  event_host->client->agent_sock[event_agent->agent_id], 
                  &event_host->client->event))
               {
                     perror("epoll_ctl: epoll_ctl_mod, epollout");  
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
                  perror("epoll_ctl, epoll_ctl_add"); 
                  exit(1); 
               }
               event_host->client->agent_fd_poll[event_agent->agent_id] = OUT; 
            }
           
				return EXIT_SUCCESS; 
         }
         else 
         { 
			   perror("send: serialzed_data"); 
				exit(1); 
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

	event_host->client->last_fd_sent++; 
	event_host->client->last_fd_sent%=10;  
			

   return EXIT_SUCCESS; 
}



int read_agent_send_host(agent_t * agent, event_info_t *event)
{
   #define  PACKET  event->client->buffered_packet
	int  size; 
	uint32_t  n_size=0; 
	int size_count = 0; 
	uint32_t packet_size; 
   int agent_id;  
   if(!PACKET[event->agent_id].size)
   {
	   while(1) { 
		   if(( size = recv(event->fd, (uint8_t *)&n_size +size_count, sizeof(n_size) - size_count, 0)) == -1)  
		   { 
            if (errno == ESHUTDOWN) 
            {
               printf("KLJLK COKC!!!\n"); 
               return CLOSE; 
            }
			   else if(errno == EAGAIN)  { 
				   if(!size_count) 
				   {
					   printf("Weird false fire?\n"); 	
					   return EXIT_SUCCESS; 
				   }
			   } 
			   else 
			   { 
				   perror("recv: n_size"); 	
				   exit(1); 
			   }
		   }
         else 
         {
		      size_count +=size; 
         }
         if(size == 0) { 
            //printf("CLOSIING@!!!!!!!!!!!!!!!!!! %d %d\n", event->agent_id, event->fd); 
            // remove epoll event 
           // if(event->client->agent_fd_poll[event->agent_id] != OFF) 
            //{
               if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
                  event->fd, NULL))
               {
                  perror("shutting socket down"); 
                  exit(1); 
               }
//               close(event->fd); 
             //  event->client->agent_fd_poll[event->agent_id] = OFF; 
            //}
            //else {printf("LJKLKJKLJ HELP!@!!!! %d\n", event->client->agent_fd_poll[event->agent_id]);  /*exit(1);*/  } 
            return CLOSE; 
         }
		   if(size_count == sizeof(n_size))
		   {
            //printf("break\n"); 
			   break; 
		   }
	   }
	   packet_size = ntohl(n_size); 
      PACKET[event->agent_id].size = packet_size; 

	   if(agent->options.verbose_level)
	   {
   		//printf("size: %d\n", packet_size ); 
	   }

	   if(packet_size > sizeof(PACKET[event->agent_id].serialized_data))  /* FIX ME */
  	   {
   	   printf("BUFFER TO SMALL AH!\n");     
         exit(1); 
      }
	   size_count = 0; 
   }
   else 
   {
      //printf("HERE\n"); 
      packet_size = PACKET[event->agent_id].size; 
      size_count = PACKET[event->agent_id].host_sent_size;  
   }
	while(1) 
	{
		if(( size = recv(event->fd, PACKET[event->agent_id].serialized_data + size_count, 
           packet_size - size_count, 0)) == -1) 		
		{
         if(errno == EAGAIN)
         {
//            printf("waiting on data!!! %d size=%d agent %d\n", size_count, packet_size, event->agent_id); 
            PACKET[event->agent_id].host_sent_size = size_count; 
            return EXIT_SUCCESS; 
         }
         else 
         {
			   perror("recv serialized_data"); 
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
               perror("shutting socket down"); 
               exit(1); 
            }
            event->client->agent_fd_poll[event->agent_id] = OFF; 
         }
         else {printf("LJKLKJKLJ HELP!@!!!! %d\n", event->client->agent_fd_poll[event->agent_id]);  /*exit(1);*/  } 
         return CLOSE; 

      }
	 	else if(size_count == packet_size)
		{
         PACKET[event->agent_id].size = 0; 
			break;
		}
	}

  PACKET[event->agent_id].packet = packet__unpack(NULL, 
      packet_size, PACKET[event->agent_id].serialized_data);   


	if(PACKET[event->agent_id].packet == NULL)  
   {
		printf("protobuf error\n"); 
      exit(1); 
	} 


   agent_id = event->agent_id; 
   //printf("got %d--- %d\n", PACKET[agent_id].packet->seq_num, event->client->recv_seq); 
   if(event->client->recv_seq == PACKET[agent_id].packet->seq_num) 
   {
      //printf("send_data_host\n"); 
//      exit(1); 
		send_data_host(agent, event, 0); 
   }
   else 
   { 
   
   //printf("totototo\n"); 
//		printf("DEL! %d\n", event->fd); 
//      printf("%d =!= %d \n",  event->client->recv_seq, PACKET[agent_id].packet->seq_num) ; 

      /* we need to remove this FD from from client_event_pool
      * since this FD is a head of the needed sequence number
      */ 
      if(event->client->agent_fd_poll[agent_id] == INAndOut)
      {
         event->client->event.events =  EPOLLOUT; 
		   event->client->event.data.ptr = &event->client->agent_side_event_info[event->agent_id]; 
         if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
            event->fd, &event->client->event))
         {
            perror(" epoll_ctl 1"); 
            exit(1); 
         }
         event->client->agent_fd_poll[agent_id] = OUT; 
      }
      else if(event->client->agent_fd_poll[agent_id] == IN)
      {
         //printf("HERER %d\n", agent_id); 
         if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL,  
            event->fd, NULL))
         {
            perror("epoll_ctl: EPOLL_CTL_DEL"); 
            exit(1); 
         }          
         event->client->agent_fd_poll[agent_id] = OFF; 
      }
      else { 
         printf("EREROIEJROEIRJ\n"); 
      } 

      /* Now we store this packet in a hash table for lookup when id is needed */       
		PACKET[event->agent_id].agent_id =  event->agent_id; 
		PACKET[event->agent_id].id = PACKET[event->agent_id].packet->seq_num; 
   	HASH_ADD_INT(event->client->buffered_packet_table, id, (&event->client->buffered_packet[event->agent_id])); 
   } 
	return EXIT_SUCCESS; 
}


int send_data_host(agent_t *agent,  event_info_t *event, int remove_fd) 
{
	int size, size_count; 
   packet_hash_t *send_packet; 
	int agent_id = event->agent_id;
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
  			   perror("epoll_ctl: epoll_ctl_mod, epollin");  
			   exit(1); 
		   }
         event->client->host_fd_poll = IN; 
      }
      else if(event->client->host_fd_poll == OUT)
      {
         printf("DEAD\n"); 
         if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL, 
  			   event->client->host_sock, 
  			   NULL))
  		   {
  			   perror("epoll_ctl: epoll_ctl_mod, epollin");  
			   exit(1); 
		   }
         event->client->host_fd_poll = OFF; 
      }
		size_count = PACKET[event->agent_id].host_sent_size; 
	}
	else 
	{
		size_count = 0; 
	} 
	while(1)
	{
		while(1) 
		{
//         size = send(event->client->host_sock, temp, strlen(temp), 0); 
			size = send(event->client->host_sock,(uint8_t *) PACKET[agent_id].packet->payload.data + size_count,  
     			PACKET[agent_id].packet->payload.len - size_count, 0);  
			if(size == -1) 
			{ 
				if(errno == EAGAIN) 
         	{	
					PACKET[agent_id].host_sent_size = size_count; 	
               if(size_count == 0) printf("WTF!\n"); 
//					printf("removed %d [%d]\n", event->agent_id, event->client->host_fd_poll); 
               /* Got blocked writing on host_sock need to now poll out! */ 

               event->client->host_side_event_info.agent_id = agent_id; 
               if(event->client->host_fd_poll == IN) 
               {
                  printf("HERE222\n"); 
	         	   event->client->event.events = EPOLLOUT | EPOLLIN;
	         	   event->client->event.data.ptr = &event->client->host_side_event_info; 

           		   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
            		   event->client->host_sock, 
            		   &event->client->event))
            	   {
            		   perror("epoll_ctl: epoll_ctl_mod, epollout");  
            		   exit(1); 
            	   }
                  event->client->host_fd_poll = INAndOut; 
               }
               else if(event->client->host_fd_poll == OFF) 
               {
                  printf("HERE333\n"); 
                  event->client->event.events = EPOLLOUT;
	         	   event->client->event.data.ptr = &event->client->host_side_event_info; 
           		   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_ADD, 
            		   event->client->host_sock, 
            		   &event->client->event))
            	   {
            		   perror("epoll_ctl: epoll_ctl_mod, epollout");  
            		   exit(1); 
            	   }
                  event->client->host_fd_poll = OUT; 
               }
               else {   
                  printf(" AIHLKJKj \n") ; exit(1); 
               } 
               /* we also need to remove poll in on the agent FD since the packet hasn't been sent! */ 
               if( event->client->agent_fd_poll[agent_id] == INAndOut)
               {
                  event->client->event.events = EPOLLOUT;
	         	   event->client->event.data.ptr = &event->client->agent_side_event_info[agent_id]; 
           		   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
            		   event->client->agent_sock[agent_id], 
            		   &event->client->event))
            	   {
            		   perror("epoll_ctl: epoll_ctl_mod, epollout");  
            		   exit(1); 
            	   }
                  event->client->agent_fd_poll[agent_id] = OUT; 
               }   
               else if(event->client->agent_fd_poll[agent_id] == IN)
               {
                  if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_DEL,  
                  event->client->agent_sock[agent_id], NULL))
                  {
                     perror("epoll_ctl: EPOLL_CTL_DEL"); 
                     exit(1); 
                  }          
                  event->client->agent_fd_poll[agent_id] = OFF;
               }
               else { printf("kljdf\n"); exit(1); }
					return EXIT_FAILURE; 
            }
				else 
				{ 
					perror("send: send_data_host"); 
					exit(1); 
				} 
			} 		
			else 
			{
				size_count +=size; 
			}
			if(size_count == PACKET[agent_id].packet->payload.len)
			{
            //printf("%s %d\n", PACKET[agent_id].packet->payload.data, (int)PACKET[agent_id].packet->payload.len); 
            PACKET[agent_id].size = 0; 
				event->client->recv_seq++; 
         
            //printf("sent: %d looking for %d\n",PACKET[agent_id].packet->seq_num, event->client->recv_seq); 
            if(!event->client->agent_fd_poll[agent_id]&IN)
            {
	         	event->client->event.data.ptr = &event->client->agent_side_event_info[agent_id]; 

               if(event->client->agent_fd_poll[agent_id] == OUT)
               {
                  event->client->event.events = EPOLLIN | EPOLLOUT; 
                  if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD, 
                     event->client->agent_sock[agent_id], &event->client->event))
                  {
                     perror("mod"); 
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

                     perror("add");
                     exit(1); 
                  } 
                  event->client->agent_fd_poll[agent_id] = IN; 
               }
               else { printf("kjkjkj\n"); exit(1); } 

            }

				break; 
			}
		} 

		/* Check to see if we already have the next packet buffered */ 
		send_packet = NULL; 
		HASH_FIND_INT(event->client->buffered_packet_table, &event->client->recv_seq, send_packet);    
      if(send_packet != NULL)  
      {
        	agent_id = send_packet->agent_id;  
//      	printf("Found %d fd=%d id=%d\n ", event->client->recv_seq, event->client->agent_sock[send_packet->agent_id], agent_id); 
        	HASH_DEL(event->client->buffered_packet_table, send_packet); 
			size_count = 0; 
         
         if(!event->client->agent_fd_poll[agent_id]&IN) 
         {
            if(event->client->agent_fd_poll[agent_id] == OUT) 
            {
               event->client->event.events = EPOLLIN | EPOLLOUT; 
         	   if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_MOD,  
       		      event->client->agent_sock[send_packet->agent_id], 
       		      &event->client->event))
      	      {
				      perror("epoll_ctl, mod fd back"); 
                  printf("agent: %d\n", send_packet->agent_id); 
       		      exit(1); 
      	      }
               event->client->agent_fd_poll[agent_id] = INAndOut;  
            }
            else if(event->client->agent_fd_poll[agent_id] == OFF) 
            {
       	      event->client->event.data.ptr = &event->client->agent_side_event_info[agent_id]; 
	            event->client->event.events = EPOLLIN; 
      	      if(epoll_ctl(event->client->client_event_pool, EPOLL_CTL_ADD,  
       	         event->client->agent_sock[send_packet->agent_id], 
       		      &event->client->event))
      	      {
		            perror("epoll_ctl, adding fd back"); 
                  printf("agent: %d\n", send_packet->agent_id); 
       		      exit(1); 
               }
               event->client->agent_fd_poll[agent_id] = IN; 
            }
         }
		}
		else 
		{ 
			return EXIT_SUCCESS; 
		} 
	}
}
#undef PACKET 
int clean_up_connections(client_t *client, agent_t *agent)
{
   printf("connection closed!\n"); 
   int i; 
   close(client->host_sock); 
   for(i = 0; i < agent->options.num_parallel_connections; i++)
   {
      close(client->agent_sock[i]); 
   }
   free(client->buffered_packet_table);  
   free(client->buffered_packet);  
   free(client->agent_side_event_info); 
   free(client->agent_sock); 
   free(client->agent_fd_poll); 
   exit(1); 
   return EXIT_SUCCESS; 
}

