
typedef struct stream_list_struct { 
	int num_streams; 
	struct stream_info_struct *stream; 
} stream_list_t; 

typedef struct stream_info_struct  { 
	struct stream_data_struct *head; 
	struct stream_data_struct *tail; 
	int size; 
	int stream_number; 
} stream_info_t; 

typedef struct stream_data_struct { 
	struct stream_data_struct *next; 
	int len; 
	void *data; 
} stream_data_t; 


typedef struct options_struct { 
	int protocol;  					/* Parallel Socket protocol to use SCTP/TCP */ 
	int verbose;						/* Verbose Mode 	*/  
	int num_parallel_sock;			/* Number of parallel sockets to use */  
	int *p_listen_sock;				/* FD for listening sockets */  
	int *p_conn_sock_server;		/* FD for parallel connecting sockets (CONNECT) */  
	int *p_conn_sock_client;		/* FD for servers connected sockets  (BIND) */  
	int tcp_client_sock;				/* FD that connects to end's host single TCP */  
	int tcp_server_sock;				/* FD that is binded to steal TCP connection */  
} options_t; 
