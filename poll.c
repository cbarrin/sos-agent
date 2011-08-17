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
	char data=0; 

	client_t *new_client; 

	struct epoll_event events; 

   if(pipe(agent->message_fd) != 0)
   {
      printf("Failed to create pipe\n"); 
      exit(1); 
   }


	while(1) 
	{
		n_events = epoll_wait(agent->event_pool, &events, 1, timeout); 
		if(n_events < 0)
		{
			perror("epoll_wait"); 
			exit(1); 
		}
      else if (n_events) 
      { 
         if(events.data.ptr == (void *) HOST_SIDE_CONNECT) 
         {
            i= fork(); 
            if(i < 0) 
            {
               printf("fork() failed\n"); 
               exit(1); 
            }
            if(!i) 
            { 
					printf("NEW client host side connect\n"); 
				   new_client = handle_host_side_connect(agent); 		
					poll_data_transfer(agent, new_client); 
            }
         }
			else if ( events.data.ptr == (void *)AGENT_SIDE_CONNECT)
         {
		      i= fork(); 
            if(i < 0) 
            {
               printf("fork() failed\n"); 
               exit(1); 
            }
            if(!i) 
            { 
					printf("NEW client agent side connect\n"); 
			      new_client = handle_agent_side_connect(agent); 
					poll_data_transfer(agent, new_client); 
            }
         }
         else
         {
			   printf("unknown event_into type!!\n"); 
			   exit(1); 
         }
			if(read(agent->message_fd[PARENT], &data, 1) < 1)
			{
				printf("read failed\n"); 
				exit(1); 
			} 
			printf("CONTINUE PARENT\n"); 
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



int poll_data_transfer(agent_t *agent, client_t * client) 
{

	int n_events; 
	int timeout = -1; 
	struct epoll_event event; 


	while(1) 
	{
//		printf("STUCK?\n"); 
		n_events = epoll_wait(client->client_event_pool, &event, 1, timeout); 
//		printf("not STUCK?\n"); 

		if(n_events < 0) 
		{
			perror("epoll_wait (client_event_poll)"); 
			exit(1); 
		}
     
		else if(n_events) { 
			event_info_t *event_info_host = (event_info_t *)event.data.ptr; 
			
			if(event_info_host->type == HOST_SIDE_DATA && event.events & EPOLLIN)
			{
				n_events = epoll_wait(client->event_poll_out_agent, &event, 1, 0); 
				if(n_events < 0) 
				{ 
					perror( "epoll_wait"); 
					exit(1); 
				} 
				if(n_events) 
				{
					event_info_t *event_info_agent = (event_info_t *)event.data.ptr; 
					printf("A\n"); 
					if(read_host_send_agent(agent, event_info_host, event_info_agent) == CLOSE) 
               {
                  timeout = 1000; 
               }  
				}
				else 
				{
					/* All parallel socks are full.  FIX ME*/
			
				}
			}
			else if(event_info_host->type == AGENT_SIDE_DATA && event.events & EPOLLOUT)
			{
				if(event_info_host->client->packet[event_info_host->agent_id].host_packet_size == 0) 
				{ printf("ASSERT!!! %d \n",event_info_host->agent_id); exit(1); } 
					printf("b\n"); 
				if(read_host_send_agent(agent, &event_info_host->client->host_side_event_info, event_info_host)== CLOSE)
            {
               timeout = 1000; 
            } 
			}


			else if (event_info_host->type == AGENT_SIDE_DATA && event.events & EPOLLIN)
			{
	//		printf("c %d\n",event_info_host->agent_id); 
				if(read_agent_send_host(agent, event_info_host) == CLOSE)
            {
               timeout = 1000; 
            } 

			}
			else if(event_info_host->type == HOST_SIDE_DATA && event.events & EPOLLOUT )
			{
				printf("d %d\n", event_info_host->agent_id); 
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
      else 
      {
         clean_up_connections(client, agent); 
      } 
	}
	return EXIT_SUCCESS; 
}

