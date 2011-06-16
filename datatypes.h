typedef struct hints_struct { 
	int type; 
}hints_t; 

typedef struct client_list_struct { 
	struct client_struct *head; 
	struct client_struct *tail; 
	int num_clients; 
} client_list_t; 

typedef struct client_struct { 
	struct epoll_event tcp_ev; 	
	struct epoll_event *parallel_ev; 
	int *parallel_sock; 
	int tcp_sock; 	
	int last_read_fd; 
	int last_write_fd; 
	int num_fds;
	struct client_struct *next; 				
	struct client_struct *prev; 				
}client_t; 



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
//	struct client_list_struct client_list; 
			


/*
	struct pollfd tcp_listen_sock; 
	struct pollfd *parallel_socks; 
*/ 
	struct epoll_event tcp_listen_ev; 
	struct epoll_event events_accept[2]; 
		
	struct epoll_event tcp_ev; 	
	struct epoll_event parallel_ev; 

	int *parallel_sock;  /* parallel_data */ 
	int tcp_sock; 	 /* tcp data */ 

	int last_read_fd; 
	int last_write_fd; 
	
	int tcp_listen_sock; 
	int *parallel_listen_socks; 
	int highsock; 
	int epfd_accept; 
	int epfd_data; 
	fd_set read_socks; 
//	int num_clients; 
	
	int data_verbose; 		/*If enabled this will print data */  
	int protocol;  					/* Parallel Socket protocol to use SCTP/TCP */ 
	int verbose;						/* Verbose Mode 	*/  
	int num_parallel_sock;			/* Number of parallel sockets to use */  
//	int *p_listen_sock;				/* FD for listening sockets */  
//	int *p_conn_sock_server;		/* FD for servers connected sockets  (BIND) */ 
//	int *p_conn_sock_client;		 /* FD for parallel connecting sockets (CONNECT) */  
//	struct pollfd *poll_p_conn_sock_client; 
////	int tcp_client_sock;				/* FD that connects to end's host single TCP */  
//	int tcp_server_sock;				/* FD that is binded to steal TCP connection */  
} options_t; 
