#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include "datatypes.h"
#include "list.h"


int main() { 
	stream_list_t *streams_list = create_stream_list(10); 
	stream_data_t *data; 
	add_data_to_list(&streams_list->stream[2], "Hello World!", strlen("Hello World!")); 
	add_data_to_list(&streams_list->stream[2], "Hello World1", strlen("Hello World!")); 
	add_data_to_list(&streams_list->stream[2], "Hello World2", strlen("Hello World!")); 

	data = pop_head_stream_data(&streams_list->stream[2]); 
	printf("%s %d\n", (char *)data->data, data->len); 
	data = pop_head_stream_data(&streams_list->stream[2]); 
	printf("%s %d\n", (char *)data->data, data->len); 
	data = pop_head_stream_data(&streams_list->stream[2]); 
	printf("%s %d\n", (char *)data->data, data->len); 
	data = pop_head_stream_data(&streams_list->stream[2]); 

	printf("-----------\n"); 

	add_data_to_list(&streams_list->stream[1], "Hello World!", strlen("Hello World!")); 
	add_data_to_list(&streams_list->stream[1], "Hello World1", strlen("Hello World!")); 
	add_data_to_list(&streams_list->stream[1], "Hello World2", strlen("Hello World!")); 
	data = pop_head_stream_data(&streams_list->stream[1]); 
	printf("%s %d\n", (char *)data->data, data->len); 
	data = pop_head_stream_data(&streams_list->stream[1]); 
	printf("%s %d\n", (char *)data->data, data->len); 
	data = pop_head_stream_data(&streams_list->stream[1]); 
	printf("%s %d\n", (char *)data->data, data->len); 


	return EXIT_SUCCESS; 


} 
