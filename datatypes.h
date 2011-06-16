typedef struct discovery_struct { 
	int sock ; 
	struct addrinfo *dest; 
}discovery_t ; 




typedef struct options_struct { 
	struct discovery_struct discovery; 
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
	int num_clients; 
	
	int data_verbose; 		/*If enabled this will print data */  
	int protocol;  					/* Parallel Socket protocol to use SCTP/TCP */ 
	int verbose;						/* Verbose Mode 	*/  
	int num_parallel_sock;			/* Number of parallel sockets to use */  
}  options_t; 
