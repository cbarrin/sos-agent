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
#include <netinet/sctp.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "datatypes.h"
#include "common.h"
#include "arguments.h"
#include "network.h"




int main(int argc, char **argv) 
{
	agent_t agent; 
   memset(&agent, 0, sizeof(agent_t)); 

	get_arguments(&agent.options, argc, argv); 
	init_agent(&agent); 


















	return EXIT_SUCCESS; 
}
