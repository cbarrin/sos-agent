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
#include <netinet/in.h>
#include <uuid/uuid.h>




#include "common.h"
#include "packet.pb-c.h"
#include "uthash.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "poll.h"
#include "controller.h"


#include "protobuf-rpc.pb-c.h" 


int init_controller_listener(controller_t * controller) 
{
	struct addrinfo hints, *servinfo; 
	int rv; 
	
	memset(&hints, 0, sizeof(hints)); 
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_DGRAM; 
	hints.ai_flags = AI_PASSIVE; 

	if(( rv = getaddrinfo(NULL, CONTROLLER_MSG_PORT, &hints, &servinfo)) != 0) 	
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
		exit(1); 
	}  
	for( controller->dest = servinfo; controller->dest != NULL; controller->dest = controller->dest->ai_next) 
	{
		if((controller->sock = socket(controller->dest->ai_family, controller->dest->ai_socktype, 
			controller->dest->ai_protocol)) == -1) 
		{
			perror("Discovery: socket"); 
			continue;
		}
		if( bind(controller->sock, controller->dest->ai_addr, controller->dest->ai_addrlen) == -1)
		{
			close(controller->sock); 
			perror("controller bind"); 
			continue; 
		}
		break; 	
	}
	if(controller->dest == NULL) 
	{
		fprintf(stderr, "controller failed to bind"); 
		exit(1); 
	} 
	return EXIT_SUCCESS; 
}



/* 
*   FIXME we should check to make sure allowed_connections is less
*   than the number of sockets we are binding on... 
*/ 

int get_controller_message(controller_t *controller) 
{ 
	socklen_t addr_len; 	
	struct sockaddr_in their_addr; 
	int size; 
   char buf[MAX_BUFFER];  
   char uuid_temp[36]; 

   transfer_request_t *transfer = malloc(sizeof(transfer_request_t)); 
   if(transfer == NULL) { 
      printf("Malloc failed...\n"); 
      return EXIT_FAILURE; 
   } 
   
   
	memset(controller->controller_info, 0, sizeof(controller->controller_info)); 
   memset(buf, 0, sizeof(buf)); 
		
	addr_len = sizeof(their_addr); 
	if( (size = recvfrom(controller->sock, buf, 
			MAX_BUFFER, 0, 
			(struct sockaddr *) &their_addr, &addr_len)) == -1) 
	{
		perror("recvfrom get_controller_message"); 
		exit(1); 
	}
   


/*
   payload = connect_info_t__unpack(NULL, size, buf); 
   strcpy(controller->send_ip, payload->connectip); 
   controller->port = payload->port; 
*/ 
/*	
	inet_ntop(their_addr.sin_family, 
			get_in_addr((struct sockaddr *) &their_addr), 
			controller->send_ip, sizeof(controller->send_ip)); 

	controller->port = ntohs(their_addr.sin_port); 
   */ 

   printf("%s\n", buf); 
     sscanf(buf, "%s %s", transfer->type, uuid_temp); 
   if(uuid_parse(uuid_temp, transfer->id)) { 
      printf("UUID FAILED TO CONVERT!!\n"); 
   } 
   if(!strcmp(transfer->type, "CLIENT")) { 

      sscanf(&buf[strlen("CLIENT ") + 36], "%s %hu %s %hu",
         transfer->source_ip, &transfer->source_port, transfer->agent_ip, 
         &transfer->allowed_connections); 

   }else if(!strcmp(transfer->type, "AGENT")) { 
      sscanf(&buf[strlen("AGENT ") + 36], "%s %hu %hu",
            transfer->agent_ip, &transfer->agent_port, &transfer->allowed_connections); 

   }
   

	printf("[%d]\n", transfer->allowed_connections); 

   HASH_ADD_INT(controller->requests, id, transfer); 
   
	return EXIT_SUCCESS; 
} 



int  check_for_transfer_request(agent_t *agent, client_t * client, char * type) { 
   transfer_request_t * iter_hash; 

   for(iter_hash = agent->controller.requests; iter_hash != NULL; iter_hash = iter_hash->hh.next) { 
      if(!strcmp(type, iter_hash->type)) {
         if(!strcmp(type, "CLIENT")) { 

            if(!strcmp(iter_hash->source_ip, client->source_ip) && 
                  iter_hash->source_port == client->source_port)
            {

               printf("%s %s \n", iter_hash->source_ip, client->source_ip); 
                 strcpy(client->agent_ip, iter_hash->agent_ip); 
                 client->allowed_connections = iter_hash->allowed_connections;  
                 uuid_copy(client->uuid, iter_hash->id); 
                 free(iter_hash); 
                 //HASH_DEL(agent->controller.requests, iter_hash); 
                 return TRUE; 
            }
         }
         else if (!strcmp(type, "AGENT")) { 
			   if(!uuid_compare(client->client_hash.id, iter_hash->id))
			   {
		         printf("FOUND!\n"); 	
					// really client ip... 
					strcpy(client->agent_ip, iter_hash->agent_ip); 
					// really client port.. 
					client->agent_port = iter_hash->agent_port; 	
               client->allowed_connections = iter_hash->allowed_connections;  
					uuid_copy(client->uuid, iter_hash->id); 
						
//               HASH_DEL(agent->controller.requests, iter_hash); 
					free(iter_hash); 
					return TRUE; 
            }
         }
         else 
         { 
            printf("unknown type: %s\n", type); 
           return FALSE; 
         }
      }
   }
   return FALSE; 
}

