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
   int *parallel_listen_sock;             /* agent parallel connection to agent  */  
   struct epoll_event event_agent;        /* epoll event for agent side          */  
}listen_fds_t; 


typedef struct client_struct 
{
   int  host_sock; 
   int  *parallel_sock; 
} client_t; 

typedef struct event_info_struct { 
   char type; 

} event_info_t; 



typedef struct agent_struct 
{ 
   int event_pool; 
   struct options_struct options; 
   struct listen_fds_struct  listen_fds; 

}agent_t; 
