int init_agent(agent_t *agent) ; 
int create_listen_sockets(agent_t *agent)  ; 
int init_poll(agent_t *agent); 
void setnonblocking(int sock); 
client_t * handle_agent_side_connect(agent_t *agent);
client_t *  handle_host_side_connect(agent_t *agent) ; 
void *get_in_addr(struct sockaddr *sa);
client_t * init_new_client(agent_t *agent ) ; 
int accept_host_side(agent_t *agent, client_t *new_client) ; 
int connect_agent_side(agent_t *agent, client_t *new_client) ; 
int read_host_send_agent(agent_t * agent, event_info_t *event_host, event_info_t *event_agent); 
int read_agent_send_host(agent_t * agent, event_info_t *event); 
int clean_up_connections(client_t *client, agent_t *agent);      
int send_data_host(agent_t *agent,  event_info_t *event, int remove_fd) ; 
int close_listener_sockets(agent_t *agent); 

