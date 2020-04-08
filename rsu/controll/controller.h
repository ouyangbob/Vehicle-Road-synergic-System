#ifndef __CONTROLLER_H

#define HIK_TRAFFIC

#define PBUF_LEN 10240
#define MAX_DUMMY_PACKET_LEN (10000)

extern void *pthread_protobuf_func(void* ptr);
extern void *pthread_sendto_cloud(void* ptr);

#ifdef HIK_TRAFFIC
extern void *pthread_stm32_date_func(void* ptr);
#endif

extern unsigned int user_data_to_server_len;
extern int send_to_stm32_flag ;
#endif
