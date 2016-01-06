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

#define TCP_PORT 9877
#define PARALLEL_PORT_START 9878
#define BACKLOG 400
#define MAX_BUFFER 1024
#define CONTROLLER_MSG_PORT "9998"
//#define DISCOVERY_DEST_ADDR "10.43.100.2"
#define DISCOVERY_DEST_ADDR "10.0.255.255"
//#define DISCOVERY_DEST_ADDR "130.127.39.2"
#define DISCOVERY_PORT "9997"
#define MAX_AGENT_CONNECTIONS 1000
#define MAX_QUEUE_SIZE 100
#define TIMEOUT 10

#define STATISTICS_DEST_ADDR "10.0.255.255"
#define STATISTICS_PORT "9999"
#define STATISTICS_INTERVAL 1