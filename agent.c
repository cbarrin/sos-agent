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


#include "common.h"
#include "uthash.h"
#include "datatypes.h"
#include "arguments.h"
#include "network.h"
#include "poll.h"




int main(int argc, char **argv) 
{
	agent_t agent; 
   memset(&agent, 0, sizeof(agent_t)); 

	get_arguments(&agent.options, argc, argv); 
	init_agent(&agent); 


	poll_loop(&agent); 



	return EXIT_SUCCESS; 
}
