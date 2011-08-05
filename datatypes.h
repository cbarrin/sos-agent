#define PARENT 0 
#define CHILD 1 


enum defines 
{ 
   FALSE=0, 
   TRUE, 
   SCTP, 
   TCP,
   NONE, 
   DATA, 
   BLOCKING  
  
};

enum options
{  
   VERBOSE = 'a', 
   NUM_CONNECTIONS, 
   NONOF, 
   LOGGING,
   PROTOCOL, 
   BIND_IP
}; 

enum poll_event_types
{
   HOST_SIDE_CONNECT=1, 
   AGENT_SIDE_CONNECT, 
   HOST_SIDE_DATA_IN, 
   AGENT_SIDE_DATA_IN
}; 


typedef struct controller_struct
{
	int sock; 
	short int port;
	char send_ip[INET6_ADDRSTRLEN]; 
	struct addrinfo *dest; 
	char controller_info[MAX_BUFFER]; 
} controller_t; 


typedef  struct options_struct 
{ 
   char verbose_level;                    /* amoutn of verbose info 			 */ 
   int num_parallel_connections;          /* number of parallel connections */ 
   char nonOF;                            /* nonOF mode                     */ 
   char logging;                          /* enable logging to mysql        */ 
   char protocol;                         /* network protocol for agents    */ 
   char bind_ip[INET6_ADDRSTRLEN]; 
}options_t; 


typedef struct listen_fds_struct
{
   int host_listen_sock;                  /* client side connection to agent     */ 
   struct epoll_event event_host;         /* epoll event for client side         */  
   int *agent_listen_sock;             /* agent parallel connection to agent  */  
   struct epoll_event event_agent;        /* epoll event for agent side          */  
}listen_fds_t; 

typedef struct event_info_struct { 
   char type; 
   int fd; 
   struct client_struct *client; 
} event_info_t; 

typedef struct client_struct 
{
   int client_event_pool; 
   struct event_info_struct host_side_event_info;  
   struct event_info_struct *agent_side_event_info;  
   struct epoll_event event; 
   int host_sock; 
   int *agent_sock; 
} client_t; 


typedef struct discovery_struct 
{
   int sock ;
   struct addrinfo *dest;
}discovery_t ;

typedef struct agent_struct 
{ 
   int event_pool; 
   struct options_struct options; 
   struct listen_fds_struct  listen_fds; 
   struct controller_struct controller; 
   struct discovery_struct discovery; 
   int message_fd[2]; 

}agent_t; 
