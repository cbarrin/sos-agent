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

#define PARENT 0
#define CHILD 1
#define OFF 0
#define IN 1
#define OUT 2
#define INAndOut 3
#define EMPTY -1
#define FULL -2
#define CLOSED 4

enum defines {
    FALSE = 0,
    TRUE,
    SCTP,
    TCP,
    NONE,
    DATA,
    BLOCKING,
    CLOSE

};

enum options {
    VERBOSE = 'a',
    NUM_CONNECTIONS,
    NONOF,
    LOGGING,
    PROTOCOL,
    BIND_IP
};

enum poll_event_types {
    HOST_SIDE_CONNECT = 1,
    AGENT_SIDE_CONNECT,
    HOST_CONNECTED,
    AGENT_CONNECTED,
    AGENT_CONNECTED_UUID,
    HOST_SIDE_DATA,
    AGENT_SIDE_DATA,
    CONTROLLER_MESSAGE
};

typedef struct transfer_request_struct {
    char type[80];
    uuid_t id;
    char source_ip[80];
    unsigned short int source_port;
    char agent_ip[80];
    unsigned short int agent_port;
    unsigned short int allowed_connections;
    int buffer_size;
    int queue_size;
    UT_hash_handle hh;
} transfer_request_t;

typedef struct controller_struct {
    int sock;
    struct transfer_request_struct *requests;
    short int port;
    char send_ip[INET6_ADDRSTRLEN];
    struct addrinfo *dest;
    char controller_info[MAX_BUFFER];
} controller_t;

typedef struct options_struct {
    char verbose_level;           /* amoutn of verbose info 			 */
    int num_parallel_connections; /* number of parallel connections */
    char nonOF;                   /* nonOF mode                     */
    char logging;                 /* enable logging to mysql        */
    char protocol;                /* network protocol for agents    */
    char bind_ip[INET6_ADDRSTRLEN];
} options_t;

typedef struct event_info_struct {
    char type;
    int fd;
    int agent_id;
    struct client_struct *client;
} event_info_t;

typedef struct listen_fds_struct {
    int host_listen_sock;           /* client side connection to agent     */
    struct epoll_event event_host;  /* epoll event for client side         */
    int *agent_listen_sock;         /* agent parallel connection to agent  */
    struct epoll_event event_agent; /* epoll event for agent side          */
    struct event_info_struct *agent_side_listen_event;
    struct event_info_struct host_side_listen_event;
    struct epoll_event event_controller;
    struct event_info_struct controller_message_event;
} listen_fds_t;

typedef struct packet_hash_struct {
    int id;       /* this is the key and also the sequence number */
    int agent_id; /* agent_sock_buffer[agent_id] == payload data */
    int host_sent_size;
    int size;
    uint8_t need_header_size;
    int packet_index;
    uint8_t *serialized_data;
    int seq_num;
    char in_use;
    UT_hash_handle hh;
} packet_hash_t;

typedef struct client_hash_struct {
    uuid_t id;
    struct timeval accept_start, accept_end;
    struct client_struct *client;
    UT_hash_handle hh; /* makes this structure hashable */
} client_hash_t;

typedef struct serialized_data_struct {
    // uint8_t serialized_data[MAX_BUFFER *2];
    uint8_t *serialized_data;
    int host_packet_size;
    int host_sent_size;
} serialized_data_t;

typedef struct stats_struct {
    struct timeval start, end;
    uint64_t total_sent_bytes;
    uint64_t total_recv_bytes;
    uint64_t windowed_total_recv_bytes; //Updated within poll loop every alarm
    uint64_t *sent_bytes;
    uint64_t *recv_bytes;
    uint64_t *windowed_recv_bytes;
    uint64_t *sent_packets;
    uint64_t *recv_packets;
    uint32_t *blocked;
    uint64_t *blocked_recv;
    uint32_t *not_full_send;
    unsigned int *average_queue_length;
} stats_t;

typedef struct client_struct {
    struct client_hash_struct client_hash;
    struct packet_hash_struct *buffered_packet_table;
    struct packet_hash_struct **buffered_packet;
    int send_seq;
    int recv_seq;

    uint32_t *header_size;
    int client_event_pool;
    int event_poll_out_agent;
    int event_poll_out_host;
    struct event_info_struct host_side_event_info;
    struct event_info_struct *agent_side_event_info;
    struct epoll_event event;
    int host_sock;
    int *agent_sock;
    struct serialized_data_struct *packet;

    char *agent_fd_poll;
    char host_fd_poll;
    int num_parallel_connections;
    int *agent_packet_queue_count;
    int *agent_packet_index_in;
    int agent_packet_index_out;
    int *agent_needed_header_size;
    char source_ip[INET6_ADDRSTRLEN];
    unsigned short int source_port;

    char agent_ip[INET6_ADDRSTRLEN];
    unsigned short int agent_port;

    unsigned short int allowed_connections;
    uuid_t uuid;
    struct transfer_request_struct *transfer_request;
    struct stats_struct stats;

} client_t;

typedef struct discovery_struct {
    int sock;
    struct addrinfo *dest;
} discovery_t;

typedef struct statistics_struct {
    int sock;
    struct addrinfo *dest;
} statistics_t;

typedef struct agent_struct {
    int event_pool;
    struct client_hash_struct *clients_hashes;
    struct options_struct options;
    struct listen_fds_struct listen_fds;
    struct controller_struct controller;
    struct statistics_struct statistics;
    struct discovery_struct discovery;
    int message_fd[2];
    int agent_fd_pool[MAX_AGENT_CONNECTIONS];
    struct event_info_struct agent_fd_pool_event[MAX_AGENT_CONNECTIONS];

} agent_t;

typedef struct connection_info_struct {
    uint64_t overhead;
    uint64_t total_sent_bytes;
    uint64_t avg_sent_bytes;
    uint64_t std_sent_bytes;
    uint64_t total_sent_chunks;
    uint64_t avg_chunks;
    uint64_t std_chunks;
} connection_info_t;