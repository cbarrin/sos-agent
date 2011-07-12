#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <mysql.h> 

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "common.h"
#include "datatypes.h"
#include "tcp_thread.h"
#include "arguments.h"
#include "network.h"

#include "discovery.h"
#include "controller.h"



void parse_xml(request_t *request, char *xml_string) 
{

	xmlDocPtr doc; 
	xmlNode *root = NULL; 
	xmlNode *node = NULL; 
	xmlNode *child = NULL; 

	doc = xmlReadMemory(xml_string, strlen(xml_string), 
				"noname.xml", NULL, 0); 
	if(doc == NULL) { 
		printf("Failed to parse document\n"); 
		printf("[%s]\n", xml_string); 
		return; 
	} 

	root = xmlDocGetRootElement(doc); 

	for(node = root; node; node = node->next) 
	{
		if(node->type == XML_ELEMENT_NODE) 
		{
			if(!xmlStrcmp(node->name, (const xmlChar *)"request")) 
			{
				for(child = node->children; child; child = child->next) 
				{
					if(!xmlStrcmp(child->name, (const xmlChar *) "ServerIP")) 
						strcpy(request->ServerIP, (char *)xmlNodeGetContent(child)); 
					else if(!xmlStrcmp(child->name, (const xmlChar *) "ClientIP")) 
						strcpy(request->ClientIP, (char *) xmlNodeGetContent(child)); 
					else if(!xmlStrcmp(child->name, (const xmlChar *) "HomeAgentIP")) 
						strcpy(request->HomeAgentIP, (char *) xmlNodeGetContent(child)); 	
					else if(!xmlStrcmp(child->name, (const xmlChar *) "HomeAgentID")) 
						strcpy(request->HomeAgentID,  (char *)xmlNodeGetContent(child)); 
					else if (!xmlStrcmp(child->name, (const xmlChar *) "ForeignAgentIP"))
						strcpy(request->ForeignAgentIP,  (char *)xmlNodeGetContent(child)); 
					else if(!xmlStrcmp(child->name, (const xmlChar *) "ForeignAgentID"))
						strcpy(request->ForeignAgentID,  (char *)xmlNodeGetContent(child)); 
					else if(!xmlStrcmp(child->name, (const xmlChar *) "DateController"))
						strcpy(request->DateController,  (char *)xmlNodeGetContent(child)); 
					else 
						printf("Unknown tag: [%s]\n", child->name); 
				
				}				
			}
		}
	}

	xmlFreeDoc(doc); 
} 

int send_mysql_data(options_t *options) 
{ 
	char query[MAX_BUFFER*5]; 
	double elapsed_accept; 	
	double elapsed_data; 	
	MYSQL *conn; 
	request_t request; 
	conn = mysql_init(NULL); 

	if(!mysql_real_connect(conn, SQL_SERVER, USER, PASSWD, DB, 0, NULL, 0)) { 
		printf("%s\n", mysql_error(conn));  
		return EXIT_FAILURE; 
	} 


	elapsed_accept = ((double)options->accept_end.tv_usec - (double) options->accept_start.tv_usec)/(1000000); 
	elapsed_accept += options->accept_end.tv_sec -  options->accept_start.tv_sec; 

	elapsed_data = ((double)options->data_end.tv_usec - (double)options->data_start.tv_usec)/(1000000); 
	elapsed_data += options->data_end.tv_sec -  options->data_start.tv_sec; 

	parse_xml(&request, options->controller.controller_info) ; 
	
	if(options->side == SERVER) 
	{ 
		strcpy(request.EndPoint, "SERVER"); 
	}
	else 
	{
		strcpy(request.EndPoint, "CLIENT"); 
	} 

	sprintf(query, "INSERT INTO log (HomeAgentID, HomeAgentIP, "\
					  "ForeignAgentID, ForeignAgentIP, ClientIP,"\
					  "ServerIP, Port,  SentBytes,  ReceivedBytes, DataElapsed,"\
					  "AcceptElapsed, DateController, EndPoint) VALUES"\
						"('%s','%s', '%s', '%s','%s', '%s','%hi','%d', '%d',"\
						"'%lf','%lf','%s','%s')",  request.HomeAgentID,
						request.HomeAgentIP, request.ForeignAgentID, 
						request.ForeignAgentIP, request.ClientIP, 
						request.ServerIP, options->controller.port, 
						options->numbytes_sent, 
						options->numbytes_received, elapsed_data, 
						elapsed_accept, request.DateController, 
						request.EndPoint); 

	if(options->verbose) 
	{ 
		printf("[%s]\n", query); 
	} 
	if(mysql_query(conn, query)) 
	{
		printf("%s\n", mysql_error(conn));  
	} 

	return EXIT_SUCCESS; 
} 

