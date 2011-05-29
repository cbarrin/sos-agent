
typedef struct stream_list_struct { 
	int num_streams; 
	struct stream_info_struct *stream; 
} stream_list_t; 

typedef struct stream_info_struct  { 
	struct stream_data_struct *head; 
	struct stream_data_struct *tail; 
	int size; 
	int stream_number; 
} stream_info_t; 

typedef struct stream_data_struct { 
	struct stream_data_struct *next; 
	int len; 
	void *data; 
} stream_data_t; 


