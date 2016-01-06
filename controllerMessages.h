int init_statistics(statistics_t *statistics);
int send_statistics_message(client_t *client, statistics_t *statistics, time_t elapsedtime);
void put_windowed_recv_bytes_in_buffer(client_t *client, char *buffer);