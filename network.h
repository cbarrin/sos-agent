/*
* Copyright (c) 2012 aaronorosen@gmail.com
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and/or hardware specification
* (the "Work") to deal in the Work without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Work, and to 
* permit persons to whom the Work is furnished to do so, subject to
* the following conditions:  The above copyright notice and this
* permission notice shall be included in all copies or substantial
* portions of the Work.
*
* THE WORK IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
* OUT OF OR IN CONNECTION WITH THE WORK OR THE USE OR OTHER DEALINGS
* IN THE WORK.
*/ 


int init_agent(agent_t *agent) ; 
int create_listen_sockets(agent_t *agent)  ; 
int init_poll(agent_t *agent); 
void setnonblocking(int sock); 
client_t *  handle_host_side_connect(agent_t *agent) ; 
void *get_in_addr(struct sockaddr *sa);
client_t * init_new_client(agent_t *agent, uuid_t *uuid ) ; 
int accept_host_side(agent_t *agent, client_t *new_client) ; 
int connect_agent_side(agent_t *agent, client_t *new_client) ; 
int read_host_send_agent(agent_t * agent, event_info_t *event_host, event_info_t *event_agent); 
int read_agent_send_host(agent_t * agent, event_info_t *event); 
int clean_up_connections(client_t *client);      
int send_data_host(agent_t *agent,  event_info_t *event, int remove_fd) ; 
int close_listener_sockets(agent_t *agent); 
int configure_poll(client_t * client); 
int accept_agent_side( agent_t *agent, event_info_t *event_info); 
int connect_host_side(agent_t *agent, client_t *new_client); 
int agent_connected_event(agent_t *agent, event_info_t *event); 
int get_uuid_and_confirm_client(agent_t *agent, int fd) ; 
int clean_up_unconnected_parallel_sockets(agent_t *agent, client_t *client) ; 
int handle_host_connected(agent_t *agent, client_t * client) ;        
int free_client(agent_t *agent, client_t * client) ; 
int close_all_data_sockets(agent_t * agent, client_t * client); 
void getinfo(client_t *client) ;
int configure_stats(client_t *client);
