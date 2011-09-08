#define PARENT 0 
#define CHILD 1 
#define OFF 0
#define IN 1 
#define OUT 2
#define INAndOut 3
#define EMPTY -1



enum defines 
{ 
   FALSE=0, 
   TRUE, 
   SCTP, 
   TCP,
   NONE, 
   DATA, 
   BLOCKING,
	CLOSE  
  
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
   HOST_CONNECTED, 
   AGENT_CONNECTED, 
   AGENT_CONNECTED_UUID, 
   HOST_SIDE_DATA, 
   AGENT_SIDE_DATA 
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

typedef struct event_info_struct { 
   char type; 
   int fd; 
	int agent_id; 
   struct client_struct *client; 
} event_info_t; 



typedef struct listen_fds_struct
{
   int host_listen_sock;                  /* client side connection to agent     */ 
   struct epoll_event event_host;         /* epoll event for client side         */  
   int *agent_listen_sock;                /* agent parallel connection to agent  */  
   struct epoll_event event_agent;        /* epoll event for agent side          */  
   struct event_info_struct *agent_side_listen_event;  
   struct event_info_struct host_side_listen_event;  
}listen_fds_t; 

typedef struct packet_hash_struct 
{
	int id; /* this is the key and also the sequence number */ 
	int agent_id;  /* agent_sock_buffer[agent_id] == payload data */   
   int host_sent_size; 
	int size; 
	uint8_t serialized_data[MAX_BUFFER *2]; 
	Packet *packet;
	UT_hash_handle hh; 
}packet_hash_t; 

typedef struct client_hash_struct
{
   uuid_t id;  
   struct timeval accept_start, accept_end; 
   struct client_struct *client; 
   UT_hash_handle hh; /* makes this structure hashable */
}  client_hash_t; 

typedef struct serialized_data_struct 
{
   uint8_t serialized_data[MAX_BUFFER *2]; 
   int host_packet_size; 
   int host_sent_size; 
} serialized_data_t; 


typedef struct client_struct 
{
   struct client_hash_struct client_hash; 
	struct packet_hash_struct *buffered_packet_table; 
	struct packet_hash_struct *buffered_packet; 
   int send_seq; 
   int recv_seq; 

   int last_fd_sent; 
   int client_event_pool; 
	int event_poll_out_agent; 
	int event_poll_out_host; 
   struct event_info_struct host_side_event_info;  
   struct event_info_struct *agent_side_event_info;  
   struct epoll_event event; 
   int host_sock; 
   int *agent_sock; 
   struct serialized_data_struct * packet; 
	char *agent_fd_poll; 
	char host_fd_poll; 
   int num_parallel_connections; 
} client_t; 


typedef struct discovery_struct 
{
   int sock ;
   struct addrinfo *dest;
}discovery_t ;

typedef struct agent_struct 
{ 
   int event_pool; 
   struct client_hash_struct *clients_hashes; 
   struct options_struct options; 
   struct listen_fds_struct  listen_fds; 
   struct controller_struct controller; 
   struct discovery_struct discovery; 
   int message_fd[2]; 
   int agent_fd_pool[MAX_AGENT_CONNECTIONS]; 
   struct event_info_struct agent_fd_pool_event[MAX_AGENT_CONNECTIONS];  

}agent_t; 
