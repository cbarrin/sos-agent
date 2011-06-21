int create_parallel_server_listen(options_t *options) ; 
int create_sctp_sockets_server(options_t *options) ; 
int parallel_server_accept(options_t *options)  ;  
int parallel_server_accept(options_t *options);   
int create_sctp_sockets_client(options_t *options) ; 
int create_tcp_socket_client(options_t *  options) ; 
int create_tcp_server_listen(options_t * options) ; 
int tcp_socket_server_accept(options_t * options);  
int do_polling_for_new_connections(options_t *options) ;  
int do_polling_for_send_recv_data(options_t *options) ;  
void remove_client(options_t *options);  
int read_parallel_send_tcp( options_t *options) ;  
int read_tcp_send_parallel( options_t *options) ;  
void increment_index(options_t *options, int type) ;  
int init_sockets(options_t *options); 
void setnonblocking(int sock); 
int epoll_data_transfer(options_t *options) ; 
int epoll_connections(options_t *options) ; 
int configure_epoll(options_t *options) ; 
int close_data(options_t *options); 
int handle_parallel_accept(options_t *options) ; 
int handle_tcp_accept(options_t *options) ; 
void *get_in_addr(struct sockaddr *sa); 

