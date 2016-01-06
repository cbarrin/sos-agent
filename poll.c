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
#include <sys/time.h>
#include <uuid/uuid.h>

#include "uthash.h"
#include "common.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "poll.h"
#include "discovery.h"
#include "controller.h"
#include "controllerMessages.h"

volatile int ALARM_FLAG = 0;

//#define DEBUG
int poll_loop(agent_t *agent) {
    int n_events;
    int i;
    int timeout = 1000;
    struct timeval accept_start;
    struct timeval current_time;
    accept_start.tv_sec = -1;
    client_hash_t *iter_hash;
    transfer_request_t *transfer_request_hash = NULL;

    struct epoll_event events;

    if (pipe(agent->message_fd) != 0) {
        printf("Failed to create pipe\n");
        exit(1);
    }

    while (1) {
        gettimeofday(&current_time, NULL);
        for (iter_hash = agent->clients_hashes; iter_hash != NULL;
             iter_hash = iter_hash->hh.next) {
            if ((current_time.tv_sec - iter_hash->accept_start.tv_sec) >
                (TIMEOUT)) {
                /*
                char buf[100];
                uuid_unparse(iter_hash->client->uuid, buf);
                printf("%s\n", buf);
                */

                HASH_FIND_INT(agent->controller.requests,
                              iter_hash->client->uuid, transfer_request_hash);
                if (transfer_request_hash == NULL) {
                    printf("ERROR did not find transfer requst id\n");
                } else if (iter_hash->client->num_parallel_connections !=
                               transfer_request_hash->allowed_connections
                           /*agent->options.num_parallel_connections*/ ||
                           iter_hash->client->host_fd_poll == 0) {
                    HASH_DEL(agent->controller.requests, transfer_request_hash);
                    printf(
                        "All sockets failed to connected or host socket "
                        "couldn't connect\n");
                } else {
#ifdef DEBUG
                    printf("FORKING %d\n", iter_hash->client->host_fd_poll);
#endif

                    HASH_DEL(agent->controller.requests, transfer_request_hash);
                    i = fork();

                    if (i < 0) {
                        printf("fork() failed\n");
                        exit(1);
                    }
                    if (!i) {
                        printf("NEW client agent side connect\n");
                        printf("%d parallel sockets accepted\n",
                               iter_hash->client->num_parallel_connections);
                        close_listener_sockets(agent);
                        //					clean_up_unconnected_parallel_sockets(agent,
                        // iter_hash->client);
                        //					HASH_DEL(agent->clients_hashes,
                        // iter_hash);
                        configure_stats(iter_hash->client);
                        configure_poll(iter_hash->client);
                        poll_data_transfer(agent, iter_hash->client);
                    }
                }
                // parent
                close_all_data_sockets(agent, iter_hash->client);
                free_client(agent, iter_hash->client);
                HASH_DEL(agent->clients_hashes, iter_hash);
            }
        }

        n_events = epoll_wait(agent->event_pool, &events, 1, timeout);

        if (n_events < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            exit(1);
        } else if (n_events) {
            event_info_t *event_info = (event_info_t *)events.data.ptr;

            if (event_info->type == CONTROLLER_MESSAGE) {
#ifdef DEBUG
                printf("CONTROLLER_MESSAGE\n");
#endif
                get_controller_message(&agent->controller);

                /*
                        Now we should check if there are sockets
                        that where waiting on this message.

                */

                for (iter_hash = agent->clients_hashes; iter_hash != NULL;
                     iter_hash = iter_hash->hh.next) {
                    if (check_for_transfer_request(agent, iter_hash->client,
                                                   "CLIENT")) {
#ifdef DEBUG
                        printf("FOUND client\n");
#endif
                        connect_agent_side(agent, iter_hash->client);
                    } else if (check_for_transfer_request(
                                   agent, iter_hash->client, "AGENT")) {
#ifdef DEBUG
                        printf("FOUND agent\n");
#endif
                        connect_host_side(agent, iter_hash->client);

                    } else {
#ifdef DEBUG
                        printf("Nothing in queue\n");
#endif
                    }
                }
            } else if (event_info->type == HOST_SIDE_CONNECT) {
#ifdef DEBUG
                printf("HOST_SIDE_CONNECT\n");
#endif
                handle_host_side_connect(agent);
            } else if (event_info->type == AGENT_CONNECTED) {
#ifdef DEBUG
                printf("AGENT_CONNECTED\n");
#endif
                agent_connected_event(agent, event_info);
            } else if (event_info->type == AGENT_SIDE_CONNECT) {
#ifdef DEBUG
                printf("ACCEPT_AGENT_SIDE\n");
#endif
                accept_agent_side(agent, event_info);
            } else if (event_info->type == AGENT_CONNECTED_UUID) {
#ifdef DEBUG
                printf("AGENT_CONNECTED_UUID\n");
#endif
                get_uuid_and_confirm_client(agent, event_info->fd);
            } else if (event_info->type == HOST_CONNECTED) {
#ifdef DEBUG
                printf("HOST_CONNECTED\n");
#endif
                handle_host_connected(agent, event_info->client);
            } else {
                printf("unknown event_into type!! [%d]\n", event_info->type);
                exit(1);
            }
        } else {
            if (!agent->options.nonOF) {
#ifdef DEBUG
                printf("ping \n");
#endif
                // send_discovery_message(&agent->discovery);
            }
            waitpid((pid_t)-1, NULL, WNOHANG);
        }
    }
    return EXIT_SUCCESS;
}

/*
   If all of the agent socks are full we
   want to remove pollin on the host_fd.

   Then we need to poll out on all the agent socks
   to let us know when it unblocks

*/

int all_agents_socks_full(agent_t *agent, client_t *client) {
    int i;
    /* we want to remove poll in on host_sock */
    if (client->host_fd_poll == INAndOut) {
        client->event.events = EPOLLOUT;
        client->event.data.ptr = &client->host_side_event_info;
        if (epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD,
                      client->host_sock, &client->event)) {
            perror("");
            printf("%s %d\n", __FILE__, __LINE__);
            exit(1);
        }
        client->host_fd_poll = OUT;
    } else if (client->host_fd_poll == IN) {
        if (epoll_ctl(client->client_event_pool, EPOLL_CTL_DEL,
                      client->host_sock, NULL)) {
            perror("");
            printf("%s %d\n", __FILE__, __LINE__);
            exit(1);
        }
        client->host_fd_poll = OFF;
    } else {
        printf("unknown bad %s %d\n", __FILE__, __LINE__);
        // exit(1);
    }

    /* now we need to POLLOUT on all agent socks */
    for (i = 0; i < client->num_parallel_connections; i++) {
        /* if we are not already polling out */
        if (!(client->agent_fd_poll[i] & OUT)) {
            if (client->agent_fd_poll[i] == IN) {
                client->event.events = EPOLLIN | EPOLLOUT;
                client->event.data.ptr = &client->agent_side_event_info[i];

                if (epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD,
                              client->agent_sock[i], &client->event)) {
                    perror("");

                    printf("%s %d %d\n", __FILE__, __LINE__, i);
                    exit(1);
                }
                client->agent_fd_poll[i] = INAndOut;
            } else if (client->agent_fd_poll[i] == OFF) {
                client->event.events = EPOLLOUT;
                client->event.data.ptr = &client->agent_side_event_info[i];
                if (epoll_ctl(client->client_event_pool, EPOLL_CTL_ADD,
                              client->agent_sock[i], &client->event)) {
                    perror("");
                    printf("%s %d\n", __FILE__, __LINE__);
                    exit(1);
                }
                client->agent_fd_poll[i] = OUT;
            }
        }
    }
    return EXIT_SUCCESS;
}

/*
we add pollin back to host_fd because we can send again

then we remove all the POLLOUTs  on the agent FDs we added before

*/

int not_all_agent_socks_full(agent_t *agent, client_t *client) {
    int i;
    /* we want to add poll in on host_sock */
    if (client->host_fd_poll == OUT) {
        client->event.events = EPOLLOUT | EPOLLIN;
        client->event.data.ptr = &client->host_side_event_info;
        if (epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD,
                      client->host_sock, &client->event)) {
            perror("");
            printf("%s %d\n", __FILE__, __LINE__);
            exit(1);
        }
        client->host_fd_poll = INAndOut;
    } else if (client->host_fd_poll == OFF) {
        client->event.events = EPOLLIN;
        client->event.data.ptr = &client->host_side_event_info;
        if (epoll_ctl(client->client_event_pool, EPOLL_CTL_ADD,
                      client->host_sock, &client->event)) {
            perror("");
            printf("%s %d\n", __FILE__, __LINE__);
            exit(1);
        }
        client->host_fd_poll = IN;
    } else {
        printf("unknown bad %d\n", client->host_fd_poll);
        // exit(1);
    }

    /* now we need to remove POLLOUT on all agent socks */
    for (i = 0; i < client->num_parallel_connections; i++) {
        /* if we are not already polling out */

        if (client->packet[i].host_packet_size == 0) {
            if (client->agent_fd_poll[i] == INAndOut) {
                client->event.events = EPOLLIN;
                client->event.data.ptr = &client->agent_side_event_info[i];

                if (epoll_ctl(client->client_event_pool, EPOLL_CTL_MOD,
                              client->agent_sock[i], &client->event)) {
                    perror("");
                    printf("%s %d\n", __FILE__, __LINE__);
                    exit(1);
                }
                client->agent_fd_poll[i] = IN;
            } else if (client->agent_fd_poll[i] == OUT) {
                if (epoll_ctl(client->client_event_pool, EPOLL_CTL_DEL,
                              client->agent_sock[i], NULL)) {
                    perror("");
                    printf("%s %d\n", __FILE__, __LINE__);
                    exit(1);
                }
                client->agent_fd_poll[i] = OFF;
            }
        }
    }
    return EXIT_SUCCESS;
}

int poll_data_transfer(agent_t *agent, client_t *client) {
    int n_events;
    int timeout = -1;
    struct epoll_event event;
    int host_socket_closed = 0;
    int i, j;
    int all_closed = 0;
    
    time_t transfer_start_time;
    time_t transfer_current_time;

    time(&transfer_start_time);
    
    /*--Send Statistics Alarm Initialization--*/
    signal(SIGALRM, send_statistics_alarm_handler);
    alarm(STATISTICS_INTERVAL);
    /*----------------------------------*/
    
    while (1) {
        /* we want to go through parallel sockets
         * and check if they have data. If not close them
         */

        if (client->host_fd_poll == CLOSED) {
            all_closed = 1;
            for (i = 0; i < client->num_parallel_connections; i++) {
                if (!client->packet[i].host_packet_size) {
                    close(client->agent_sock[i]);
                    client->agent_fd_poll[i] = CLOSED;
                } else if (client->agent_fd_poll[i] != CLOSED) {
                    all_closed = 0;
                }
            }
        } else {
            all_closed = 1;
            for (i = 0; i < client->num_parallel_connections; i++) {
                if (client->agent_fd_poll[i] != CLOSED) {
                    all_closed = 0;
                    break;
                }
            }
            if (all_closed) {
                //				printf("looks good so
                // far...\n");

                for (i = 0; i < client->num_parallel_connections; i++) {
                    for (j = 0; j < client->transfer_request->queue_size; j++) {
                        // for(j = 0; j < MAX_QUEUE_SIZE; j++) {
                        if (client->buffered_packet[i][j].in_use) {
                            all_closed = 0;
                            //							printf("FAILED
                            //%d
                            //%d\n",
                            // i,
                            // j);
                        }
                    }
                }
            }
            if (all_closed) {
                close(client->host_sock);
            }
        }

        /* Find the transfer request hash to determine agent type */
        //        client_hash_t *iter_hash;
        //        transfer_request_t *transfer_request_hash = NULL;
        //        for (iter_hash = agent->clients_hashes; iter_hash != NULL;
        //             iter_hash = iter_hash->hh.next) {
        //            HASH_FIND_INT(agent->controller.requests,
        //                          iter_hash->client->uuid,
        //                          transfer_request_hash);
        //            if (transfer_request_hash == NULL) {
        //                printf("ERROR did not find transfer requst id\n");
        //            }
        //        }

        if (all_closed) {
            send_controller_termination_message(client, &agent->discovery);
            printf("Everything has been closed!\n");
            // getinfo(client);
            exit(1);
        }

        n_events = epoll_wait(client->client_event_pool, &event, 1, timeout);

        if (n_events < 0) {
            if (errno == EINTR) continue;
            printf("%d %d\n", timeout,
                   ((event_info_t *)event.data.ptr)->agent_id);
            perror("");
            printf("%s %d\n", __FILE__, __LINE__);
            exit(1);
        }

        if (n_events) {
            event_info_t *event_info_host = (event_info_t *)event.data.ptr;

            if (event_info_host->type == HOST_SIDE_DATA &&
                event.events & EPOLLIN) {
                n_events =
                    epoll_wait(client->event_poll_out_agent, &event, 1, 0);
                if (n_events < 0) {
                    if (errno == EINTR) continue;
                    perror("");
                    printf("%s %d\n", __FILE__, __LINE__);
                    exit(1);
                }
                if (event.events & EPOLLERR) {
                    perror("");
                    exit(1);
                }
                if (n_events) {
                    event_info_t *event_info_agent =
                        (event_info_t *)event.data.ptr;
#ifdef DEBUG
//              printf("A %d\n",  event_info_agent->agent_id);
#endif
                    if (read_host_send_agent(agent, event_info_host,
                                             event_info_agent) == CLOSE) {
                        printf("returned a closed\n");
                        host_socket_closed = 1;
                        // timeout = 1000;
                    }
                } else {
/* All parallel socks are full.  FIX ME*/
#ifdef DEBUG
//					printf("BAD!!! %d\n",
// event_info_host->client->host_fd_poll);
#endif
                    all_agents_socks_full(agent, event_info_host->client);
#ifdef DEBUG
//					printf("AFTER BAD!!! %d\n",
// event_info_host->client->host_fd_poll);
#endif
                }
            } else if (event_info_host->type == AGENT_SIDE_DATA &&
                       event.events & EPOLLOUT) {
                if (event_info_host->client->packet[event_info_host->agent_id]
                        .host_packet_size == 0) {
/* parallel socket unblocked */
#ifdef DEBUG
//              printf("Something freed %d perm=%d\n",
//              event_info_host->agent_id,
//              event_info_host->client->host_fd_poll);
#endif
                    not_all_agent_socks_full(agent, event_info_host->client);
                }

                if (read_host_send_agent(
                        agent, &event_info_host->client->host_side_event_info,
                        event_info_host) == CLOSE) {
                    printf("returned a closed\n");
                    host_socket_closed = 1;
                    // timeout = 1000;
                }
            } else if (event_info_host->type == AGENT_SIDE_DATA &&
                       event.events & EPOLLIN) {
#ifdef DEBUG
//			   printf("c %d\n",event_info_host->agent_id);
#endif
                if (read_agent_send_host(agent, event_info_host) == CLOSE) {
                    // timeout = 1000;
                }

            } else if (event_info_host->type == HOST_SIDE_DATA &&
                       event.events & EPOLLOUT) {
#ifdef DEBUG
//				printf("d %d\n", event_info_host->agent_id);
#endif
                if (send_data_host(agent, event_info_host, 1) == CLOSE) {
                    // timeout = 1000;
                }
            } else {
                if (event.events & EPOLLERR) {
                    perror("");
                    free_client(agent, client);
                    clean_up_connections(client);
                    printf("All sockets closed!\n");
                    exit(1);
                }
                printf("Uknown event_info->type! [%d] pollinfo[%X]\n",
                       event_info_host->type, event.events);
                perror("");
                exit(1);
            }
        }
        /*
else if(!n_events)
{
                printf("Everything timed out...\n");
                        for(i = 0; i < client->num_parallel_connections; i++) {
                                for(j = 0; j < MAX_QUEUE_SIZE; j++) {
                                        if(client->buffered_packet[i][j].in_use)
{
                                                all_closed = 0 ;
                                                printf("FAILED %d %d\n", i, j);
                                        }
                                }
                        }

 free_client(agent, client);
 clean_up_connections(client);
 printf("All sockets closed!\n");
 exit(1);
}
*/
        
        if (ALARM_FLAG) {
            ALARM_FLAG = 0;
            time(&transfer_current_time);
            client->stats.windowed_sent_bytes = client->stats.total_sent_bytes - client->stats.windowed_sent_bytes;
            send_statistics_message(client, &agent->statistics, (transfer_current_time - transfer_start_time));
        }
    }
    return EXIT_SUCCESS;
}

void send_statistics_alarm_handler(int signum) {
    //printf("\nSend data alarm!");
    ALARM_FLAG = 1;
    signal(SIGALRM, send_statistics_alarm_handler);
    alarm(STATISTICS_INTERVAL);
}