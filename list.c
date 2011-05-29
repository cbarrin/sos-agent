#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "datatypes.h"
#include "list.h"


stream_list_t * create_stream_list(int number_of_streams) { 

	int count; 
	stream_list_t *list = (stream_list_t *) malloc(sizeof(stream_list_t)); 

	if(list == NULL) { 
		fprintf(stderr, "Failed to allocate stream_list_t\n"); 
		return NULL; 
	}

	list->num_streams = number_of_streams; 
	list->stream = (stream_info_t *) malloc(sizeof(stream_info_t) * number_of_streams); 
	if(list->stream == NULL) { 
		fprintf(stderr, "Failed to allocate stream_info_t\n"); 
		return NULL; 
	} 
	else { 
		for(count = 0; count < number_of_streams; count++) { 
			list->stream[count].size = 0; 
			list->stream[count].stream_number = count; 
			list->stream[count].head = NULL; 
			list->stream[count].tail = NULL; 
		} 
		return list; 
	} 
} 

int add_data_to_list(stream_info_t *stream_info, void *data, int len) { 

	stream_data_t * new_data_elem;  	
	void *payload; 
	
	new_data_elem = (stream_data_t * ) malloc(sizeof(stream_data_t)); 
	if(new_data_elem == NULL) { 
		fprintf(stderr, "Failed to allocate stream_data_t\n"); 
		return EXIT_FAILURE; 
	} 

	payload = (void *)malloc(len); 
	if(payload == NULL) { 
		fprintf(stderr, "Failed to malloc payload"); 
		return EXIT_FAILURE; 
	} 


	memcpy(payload, data, len); 

	new_data_elem->len = len; 
	new_data_elem->data = payload; 	
	new_data_elem->next = NULL; 


	if(!stream_info->size) { 
		stream_info->head = new_data_elem; 
		stream_info->tail = new_data_elem; 
	} 
	else { 
		stream_info->tail->next = new_data_elem; 
	}

	stream_info->tail = new_data_elem; 
	stream_info->size++; 
	
	return EXIT_SUCCESS; 	

} 

stream_data_t * pop_head_stream_data( stream_info_t *stream_info) { 
	stream_data_t *ret = NULL; 	

	if(!stream_info->size)  { 
#ifdef DEBUG
		fprintf(stderr, "Stream %d is empty\n", stream_info->stream_number); 
#endif
		return ret; 
	} 

	ret = stream_info->head; 
	if(stream_info->head->next != NULL) { 
		stream_info->head = stream_info->head->next; 	
	} 
	stream_info->size--; 

	return ret; 

}

int stream_empty( stream_info_t * stream_info) { 
	return stream_info->size; 
}
 
