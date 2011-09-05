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
#include "common.h"
#include "packet.pb-c.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "poll.h"
#include "discovery.h"



int poll_loop(agent_t *agent) 
{
	int n_events; 
   int i;
   int timeout = 1000; 
   struct timeval accept_start, accept_end; 
   accept_start.tv_sec = -1; 

	client_t *new_client = init_new_client(agent); 

	struct epoll_event events; 

   if(pipe(agent->message_fd) != 0)
   {
      printf("Failed to create pipe\n"); 
      exit(1); 
   }


	while(1) 
	{

       
      if(accept_start.tv_sec != -1)
      { 
         if((accept_end.tv_sec - accept_start.tv_sec) > 5)
         {
            i=fork(); 

            if(i < 0) 
            {
               printf("fork() failed\n"); 
               exit(1); 
            }
            if(!i) 
            { 
					printf("NEW client agent side connect\n"); 
               printf("%d parallel sockets accepted\n", new_client->num_parallel_connections); 
               close_listener_sockets(agent); 
               configure_poll(new_client); 
					poll_data_transfer(agent, new_client); 
            }
            else 
            {
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
               accept_start.tv_sec = -1; 
               new_client->num_parallel_connections = 0; 
               clean_up_connections(new_client); 
			      printf("CONTINUE PARENT\n"); 
            }
         } 
         gettimeofday(&accept_end, NULL); 
      }

		n_events = epoll_wait(agent->event_pool, &events, 1, timeout); 

		if(n_events < 0)
		{
			perror("epoll_wait"); 
			exit(1); 
		}
      else if (n_events) 
      { 


			event_info_t *event_info = (event_info_t *)events.data.ptr; 

         if(event_info->type ==  HOST_SIDE_CONNECT) 
         {
				handle_host_side_connect(agent, new_client); 		
            i= fork(); 
            if(i < 0) 
            {
               printf("fork() failed\n"); 
               exit(1); 
            }
            if(!i) 
            { 
					printf("NEW client host side connect\n"); 
               close_listener_sockets(agent); 
               configure_poll(new_client); 
					poll_data_transfer(agent, new_client); 
            }
            else 
            {
              clean_up_connections(new_client); 
              new_client->num_parallel_connections = 0; 
            }
         }
			else if ( event_info->type == AGENT_SIDE_CONNECT)
         {
            if(!new_client->num_parallel_connections)
            {
               if(epoll_ctl(agent->event_pool, EPOLL_CTL_DEL, 
                  agent->listen_fds.host_listen_sock, NULL))
               {
			         perror(""); 
         			printf("%s %d\n", __FILE__, __LINE__); 
		            exit(1); 	
               }
               gettimeofday(&accept_start, NULL); 
               gettimeofday(&accept_end, NULL); 
               connect_host_side(agent, new_client); 
            }

            accept_agent_side(agent, new_client, event_info->fd); 
         
            if(new_client->num_parallel_connections == agent->options.num_parallel_connections) 
            {
               accept_end.tv_sec +=6; 
            }
         }
         else
         {
			   printf("unknown event_into type!!\n"); 
			   exit(1); 
         }
      }
      else 
      {
         if(!agent->options.nonOF) 
         {
            send_discovery_message(&agent->discovery); 
         }
      }

	}
   return EXIT_SUCCESS; 
}

int all_agents_socks_full(agent_t *agent, client_t *client)
{
	int i; 
	/* we want to remove poll in on host_sock */ 
	if(client->host_fd_poll == INAndOut)
	{
		client->event.events = EPOLLOUT; 	
		client->event.data.ptr = &client->host_side_event_info; 
		if(epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD, 	
			client->host_sock, &client->event))
		{
			perror(""); 
		   printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 
		}
		client->host_fd_poll = OUT; 
	} 
	else if(client->host_fd_poll == IN)
	{ 
		if(epoll_ctl(client->client_event_pool, EPOLL_CTL_DEL, 
			client->host_sock, NULL))
		{
			perror(""); 
		   printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 
		} 	
		client->host_fd_poll = OFF; 
	} 
	else 
	{ 
		printf("unknown bad %s %d\n", __FILE__, __LINE__); 	
		//exit(1); 
	} 

	/* now we need to POLLOUT on all agent socks */ 
	for(i = 0; i < client->num_parallel_connections; i++)
	{
		/* if we are not already polling out */ 
		if(!(client->agent_fd_poll[i]&OUT))
		{ 
			if(client->agent_fd_poll[i] == IN) 
			{ 
				client->event.events = EPOLLIN | EPOLLOUT; 
				client->event.data.ptr = &client->agent_side_event_info[i]; 

				if(epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD,
					client->agent_sock[i], &client->event))
				{
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 
				} 
				client->agent_fd_poll[i] = INAndOut; 
			} 	
			else if(client->agent_fd_poll[i] == OFF)
			{
				client->event.events = EPOLLOUT; 
				client->event.data.ptr = &client->agent_side_event_info[i]; 
				if(epoll_ctl(client->client_event_pool, EPOLL_CTL_ADD, 
					client->agent_sock[i], &client->event))
				{

					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 

				}
				client->agent_fd_poll[i] = OUT; 
			} 
		}  
	}
	return EXIT_SUCCESS; 
}
int not_all_agent_socks_full(agent_t *agent, client_t * client) 
{

	int i; 
	/* we want to add poll in on host_sock */ 
	if(client->host_fd_poll == OUT)
	{
		client->event.events = EPOLLOUT | EPOLLIN; 	
		client->event.data.ptr = &client->host_side_event_info; 
		if(epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD, 	
			client->host_sock, &client->event))
		{
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 
		}
		client->host_fd_poll = INAndOut; 
	} 
	else if(client->host_fd_poll == OFF)
	{ 
		client->event.events = EPOLLIN; 	
		client->event.data.ptr = &client->host_side_event_info; 
		if(epoll_ctl(client->client_event_pool, EPOLL_CTL_ADD, 
			client->host_sock, &client->event))
		{
			perror(""); 
			printf("%s %d\n", __FILE__, __LINE__); 
			exit(1); 
		} 	
		client->host_fd_poll = IN; 
	} 
	else 
	{ 
		printf("unknown bad %d\n", client->host_fd_poll); 	
		exit(1); 
	} 

	/* now we need to remove POLLOUT on all agent socks */ 
	for(i = 0; i < client->num_parallel_connections; i++)
	{
		/* if we are not already polling out */ 

		if(client->packet[i].host_packet_size == 0) 
		{
			if(client->agent_fd_poll[i] == INAndOut)
			{ 
				client->event.events = EPOLLIN; 
				client->event.data.ptr = &client->agent_side_event_info[i]; 

				if(epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD,
					client->agent_sock[i], &client->event))
				{
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 
				} 
				client->agent_fd_poll[i] = IN; 
			} 	
			else if(client->agent_fd_poll[i] == OUT)
			{
				if(epoll_ctl(client->client_event_pool, EPOLL_CTL_DEL, 
					client->agent_sock[i], NULL))
				{

					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 

				}
				client->agent_fd_poll[i] = OFF; 
			} 
		}  
	} 
	return EXIT_SUCCESS; 
}

int poll_data_transfer(agent_t *agent, client_t * client) 
{

	int n_events; 
	int timeout = -1; 
	struct epoll_event event;


	while(1) 
	{
		n_events = epoll_wait(client->client_event_pool, &event, 1, timeout); 
		if(n_events < 0) 
		{
			perror(""); 
		   printf("%s %d\n", __FILE__, __LINE__); 
		   exit(1); 
		}
     
      if(n_events) 
      { 
			event_info_t *event_info_host = (event_info_t *)event.data.ptr; 
			
			if(event_info_host->type == HOST_SIDE_DATA && event.events & EPOLLIN)
			{
				n_events= epoll_wait(client->event_poll_out_agent, &event, 1, 0); 
				if(n_events < 0) 
				{ 
					perror(""); 
					printf("%s %d\n", __FILE__, __LINE__); 
					exit(1); 
				} 
				if(n_events) 
				{
					event_info_t *event_info_agent = (event_info_t *)event.data.ptr; 
#ifdef DEBUG
               printf("A %d\n",  event_info_agent->agent_id); 
#endif 
					if(read_host_send_agent(agent, event_info_host, event_info_agent) == CLOSE) 
               {
                  timeout = 1000; 
               }  
				}
				else 
				{
					/* All parallel socks are full.  FIX ME*/
#ifdef DEBUG
					printf("BAD!!! %d\n", event_info_host->client->host_fd_poll); 
#endif
					all_agents_socks_full(agent, event_info_host->client); 
#ifdef DEBUG
					printf("AFTER BAD!!! %d\n", event_info_host->client->host_fd_poll); 
#endif
			
				}
			}
			else if(event_info_host->type == AGENT_SIDE_DATA && event.events & EPOLLOUT)
			{
				if(event_info_host->client->packet[event_info_host->agent_id].host_packet_size == 0)
				{
					/* parallel socket unblocked */ 	
#ifdef DEBUG
               printf("Something freed %d perm=%d\n", event_info_host->agent_id, event_info_host->client->host_fd_poll); 
#endif
					not_all_agent_socks_full(agent, event_info_host->client); 
				} 
 
				if(read_host_send_agent(agent, &event_info_host->client->host_side_event_info, event_info_host)== CLOSE)
            {
               timeout = 1000; 
            } 
			}
			else if (event_info_host->type == AGENT_SIDE_DATA && event.events & EPOLLIN)
			{
#ifdef DEBUG
			   printf("c %d\n",event_info_host->agent_id); 
#endif
				if(read_agent_send_host(agent, event_info_host) == CLOSE)
            {
               timeout = 1000; 
            } 

			}
			else if(event_info_host->type == HOST_SIDE_DATA && event.events & EPOLLOUT )
			{
#ifdef DEBUG
				printf("d %d\n", event_info_host->agent_id); 
#endif
				if(send_data_host(agent,event_info_host, 1) == CLOSE) 
            {
               timeout = 1000; 
            } 
			}
			else 
			{
				printf("Uknown event_info->type! [%d]\n", event_info_host->type); 
				exit(1); 
			}

		} 
      else if(!n_events) 
      {
         
         clean_up_connections(client); 
         printf("All sockets closed!\n"); 
         exit(1); 
      } 
	}
	return EXIT_SUCCESS; 
}

