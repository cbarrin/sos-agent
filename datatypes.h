enum defines { 
   FALSE=0, 
   TRUE, 
   SCTP, 
   TCP,
   NONE, 
   DATA, 
   BLOCKING  
 };

enum options{  
   VERBOSE = 'a', 
   NUM_CONNECTIONS, 
   NONOF, 
   LOGGING,
   PROTOCOL, 
   BIND_IP
}; 


#ifndef INET6_ADDRSTRLEN
   #define INET6_ADDRSTRLEN 16
#endif


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
   int host_listen_sock;                /* client side_connection to agent     */ 
   int *parallel_listen_sock;             /* agent parallel connection to agent  */  

}listen_fds_t; 





typedef struct agent_struct 
{ 
   struct options_struct options; 
   struct listen_fds_struct  listen_fds; 

}agent_t; 
