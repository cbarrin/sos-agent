int allocate_network_server(options_t *options) ; 
int create_sctp_sockets_server(options_t *options); 
int create_sctp_sockets_client(options_t *options); 
int connect_tcp_socket_client(options_t *options); 
int create_tcp_server(options_t *options); 
int parallel_recv_to_tcp_send(options_t *options); 
int recv_to_parallel_send(options_t *options); 
void close_sockets_sctp_server(options_t *options); 
void close_sockets_sctp_client(options_t *options); 

