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
#include "poll.h"
#include "discovery.h"





int poll_loop(agent_t *agent) 
{
	int n_events; 
   int i;
   int timeout = 1000; 

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
			      new_client = handle_agent_side_connect(agent); 
					poll_data_transfer(agent, new_client); 
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
			perror("epoll_wait (client_event_poll)"); 
			exit(1); 
		}
		else { 
			event_info_t *event_info = (event_info_t *)event.data.ptr; 
			
			if(event_info->type == HOST_SIDE_DATA_IN)
			{
//				read_host_send_agent(event_info); 

			}
			else if (event_info->type == AGENT_SIDE_DATA_IN)
			{
//				read_agent_send_host(event_info); 

			}

		} 



	}
	return EXIT_SUCCESS; 
}

