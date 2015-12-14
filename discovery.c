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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int init_discovery(discovery_t *discovery) {
    struct addrinfo hints, *servinfo;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(DISCOVERY_DEST_ADDR, DISCOVERY_PORT, &hints,
                          &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    for (discovery->dest = servinfo; discovery->dest != NULL;
         discovery->dest = discovery->dest->ai_next) {
        if ((discovery->sock = socket(discovery->dest->ai_family,
                                      discovery->dest->ai_socktype,
                                      discovery->dest->ai_protocol)) == -1) {
            perror("Discovery: socket");
            continue;
        }
        break;
    }

    int broadcastEnable = 1;
    int ret = setsockopt(discovery->sock, SOL_SOCKET, SO_BROADCAST,
                         &broadcastEnable, sizeof(broadcastEnable));

    if (discovery->dest == NULL) {
        fprintf(stderr, "discovery failed to bind\n");
        exit(1);
    }
    return EXIT_SUCCESS;
}

int send_discovery_message(discovery_t *discovery) {
    char *msg = "<sos><agent>arosen</agent><status>active</status></sos>";

    if ((sendto(discovery->sock, msg, strlen(msg), 0, discovery->dest->ai_addr,
                discovery->dest->ai_addrlen)) == -1) {
        perror("Discovery: sendto\n");
    }
    return EXIT_SUCCESS;
}

int send_controller_termination_message(client_t *client,
                                        discovery_t *discovery) {
    char uuid_msg[200];
    char buffer[500];
    connection_info_t info = getinfo(client);
    uuid_unparse(client->uuid, uuid_msg);
    snprintf(buffer, 500,
             "{ \"type\" : \"%s\", \"transfer_id\" : \"%s\", \"overhead\" : "
             "%lu, \"avg_sent_bytes\" : %lu, \"std_sent_bytes\" : %lu, "
             "\"avg_chunks\" : %lu, \"std_chunks\" : %lu }",
             client->transfer_request->type, uuid_msg, info.overhead,
             info.avg_sent_bytes, info.std_sent_bytes, info.avg_chunks,
             info.std_chunks);
    printf(
        "'{ \"type\" : \"%s\", \"transfer_id\" : \"%s\", \"overhead\" : %lu, "
        "\"avg_sent_bytes\" : "
        "%lu, \"std_sent_bytes\" : %lu, \"avg_chunks\" : %lu, \"std_chunks\" : "
        "%lu }'",
        client->transfer_request->type, uuid_msg, info.overhead,
        info.avg_sent_bytes, info.std_sent_bytes, info.avg_chunks,
        info.std_chunks);
    if ((sendto(discovery->sock, buffer, strlen(buffer), 0,
                discovery->dest->ai_addr, discovery->dest->ai_addrlen)) < 0) {
        perror("Send termination message\n");
    }
    return EXIT_SUCCESS;
}