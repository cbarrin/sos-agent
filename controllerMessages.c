#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <uuid/uuid.h>

#include "uthash.h"
#include "common.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "controllerMessages.h"

int init_statistics(statistics_t *statistics) {
    struct addrinfo hints, *servinfo;
    int rv;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    if ((rv = getaddrinfo(STATISTICS_DEST_ADDR, STATISTICS_PORT, &hints,
                          &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    for (statistics->dest = servinfo; statistics->dest != NULL;
         statistics->dest = statistics->dest->ai_next) {
        if ((statistics->sock = socket(statistics->dest->ai_family,
                                      statistics->dest->ai_socktype,
                                      statistics->dest->ai_protocol)) == -1) {
            perror("statistics: socket");
            continue;
        }
        break;
    }
    
    int broadcastEnable = 1;
    int ret = setsockopt(statistics->sock, SOL_SOCKET, SO_BROADCAST,
                         &broadcastEnable, sizeof(broadcastEnable));
    
    if (statistics->dest == NULL) {
        fprintf(stderr, "statistics failed to bind\n");
        exit(1);
    }
    return EXIT_SUCCESS;
}

int send_statistics_message(client_t *client,
                                        statistics_t *statistics, time_t elapsedtime) {
    char uuid_msg[200];
    char buffer[500 + 40 * client->num_parallel_connections * 2];
    
    uint64_t throughput = client->stats.total_recv_bytes * 8 / (uint64_t)elapsedtime;
    uint64_t windowed_throughput = client->stats.windowed_total_recv_bytes * 8 / (uint64_t)STATISTICS_INTERVAL;
    uuid_unparse(client->uuid, uuid_msg);
    
    sprintf(buffer, "{ ");
    sprintf(buffer, "\"transfer_id\" : \"%s\", ", uuid_msg);
    sprintf(buffer, "\"type\" : \"%s\", ", client->transfer_request->type);
    sprintf(buffer, "\"cumulative_throughput\" : \"%lu\", ", throughput);
    sprintf(buffer, "\"rolling_throughput\" : \"%lu\", ", windowed_throughput);
    
    put_recv_bytes_in_buffer(client, buffer);
    
    sprintf(buffer, " }");
    
    if ((sendto(statistics->sock, buffer, strlen(buffer), 0,
                statistics->dest->ai_addr, statistics->dest->ai_addrlen)) < 0) {
        perror("Send statistics message\n");
    }
    
    printf("\n%s\n", buffer);
    return EXIT_SUCCESS;
}

void put_recv_bytes_in_buffer(client_t *client, char *buffer) {
    int i;
    
    sprintf(buffer, " \"per_socket_throughput\" : [");
    
    for (i = 0; i < client->num_parallel_connections - 1; i++) {
        sprintf(buffer, " { \"socket_id\" : \"%d\",", i);
        sprintf(buffer, " \"cumulative_throughput\" : \"%lu\",", i, client->stats.recv_bytes[i]);
        sprintf(buffer, " \"rolling_throughput\" : \"%lu\" },", i, client->stats.windowed_recv_bytes[i]);
    }
    sprintf(buffer, " { \"socket_id\" : \"%d\",", i);
    sprintf(buffer, " \"cumulative_throughput\" : \"%lu\",", i, client->stats.recv_bytes[i]);
    sprintf(buffer, " \"rolling_throughput\" : \"%lu\" }", i, client->stats.windowed_recv_bytes[i]);
    sprintf(buffer, " ] ");
}

