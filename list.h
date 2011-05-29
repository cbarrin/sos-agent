stream_list_t * create_stream_list(int number_of_streams); 
stream_data_t * pop_head_stream_data( stream_info_t *stream_info); 
int add_data_to_list(stream_info_t *stream_info, void *data, int len); 
int stream_empty( stream_info_t * stream_info); 
