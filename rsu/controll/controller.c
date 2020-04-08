#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/in6.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <v2x_radio_api.h>
#include <v2x_log.h>
#include <v2x_kinematics_api.h>
#include <math.h>
#include <glib.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include "pb_common.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "pb.h"
#include "test.pb.h"
#include <string.h>
#include "controller.h"
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../proto/pb_common.h"
#include "../proto/pb_decode.h"
#include "../proto/pb_encode.h"
#include "../proto/pb.h"
#include "../proto/test.pb.h"



//proto_v2x_location_fix_t v2x_gps;
//proto_RsuInfoRequest RegisterRequest;
extern int recv_flag ;
pb_ostream_t ostream;
bool rec_bool;
sem_t gsem;
char pbuf[PBUF_LEN]={0};
int pbuf_len = 0;
const int MAXLINE = 1024;  
extern char recv_buf[MAX_DUMMY_PACKET_LEN] ;//recvive obu sendback data
extern int user_data_len;//user's data
extern int total_recv_len;


int sockfd, clientfd;  
socklen_t cliaddr_len;	
struct sockaddr_in server_addr, client_addr;	

void *p = NULL;
struct sockaddr_in remote_addr;
char *server_ip = "47.111.188.230";
//char *server_ip = "192.168.3.120";
int server_port = 8989;
int client_sockfd;
int fifo_fd;

int imx_port = 9000;

sem_t buf_number;
int send_to_stm32_flag = 0;

#define PROTOBUF_MAIN_ID_OFFSET 0
#define PROTOBUF_SUB_ID_OFFSET 4
#define PROTOBUF_LEN_OFFSET 8
#define PROTOBUF_MESSAGE_OFFSET 12
#define HIK_TRAFFIC_FIFO_PATH "./hik_traffic_fifo"

typedef struct {
    uint8_t u8NorthSignalL;        /* 北方向左转红绿灯状态 */
    uint8_t u8NorthDelayL;         /* 北方向左转红绿灯倒计时 */
    uint8_t u8NorthSignalD;        /* 北方向直行红绿灯状态 */
    uint8_t u8NorthDelayD;         /* 北方向直行红绿灯倒计时 */
    uint8_t u8NorthSignalR;        /* 北方向右转红绿灯状态 */
    uint8_t u8NorthDelayR;         /* 北方向右转红绿灯倒计时 */
    uint8_t u8SouthSignalL;        /* 南方向左转红绿灯状态 */
    uint8_t u8SouthDelayL;         /* 南方向左转红绿灯倒计时 */
    uint8_t u8SouthSignalD;        /* 南方向直行红绿灯状态 */
    uint8_t u8SouthDelayD;         /* 南方向直行红绿灯倒计时 */
    uint8_t u8SouthSignalR;        /* 南方向右转红绿灯状态 */
    uint8_t u8SouthDelayR;         /* 南方向右转红绿灯倒计时 */
    uint8_t u8WestSignalL;         /* 西方向左转红绿灯状态 */
    uint8_t u8WestDelayL;          /* 西方向左转红绿灯倒计时 */
    uint8_t u8WestSignalD;         /* 西方向直行红绿灯状态 */
    uint8_t u8WestDelayD;          /* 西方向直行红绿灯倒计时 */
    uint8_t u8WestSignalR;         /* 西方向右转红绿灯状态 */
    uint8_t u8WestDelayR;          /* 西方向右转红绿灯倒计时 */
    uint8_t u8EastSignalL;         /* 东方向左转红绿灯状态 */
    uint8_t u8EastDelayL;          /* 东方向左转红绿灯倒计时 */
    uint8_t u8EastSignalD;         /* 东方向直行红绿灯状态 */
    uint8_t u8EastDelayD;          /* 东方向直行红绿灯倒计时 */
    uint8_t u8EastSignalR;         /* 东方向右转红绿灯状态 */
    uint8_t u8EastDelayR;          /* 东方向右转红绿灯倒计时 */
} HikTrafficLights;

static proto_RsuInfoRequest stRsuInfoRequest = {0};


#if 0
void init_gps(void)
{
v2x_gps.utc_fix_time = 1555511449.5320001;
v2x_gps.fix_mode = 3;
v2x_gps.latitude = 24.48634921;
v2x_gps.longitude = 118.19132145;
v2x_gps.altitude = -0.467498779296875;
v2x_gps.qty_SV_in_view = 44;
v2x_gps.qty_SV_used = 44;

v2x_gps.gnss_status.unavailable = 0;
v2x_gps.gnss_status.aPDOPofUnder5 = 0;
v2x_gps.gnss_status.inViewOfUnder5 = 0;
v2x_gps.gnss_status.localCorrectionsPresent = 0;
v2x_gps.gnss_status.networkCorrectionsPresent = 0;

v2x_gps.SemiMajorAxisAccuracy = 9.6381301879882812;
v2x_gps.SemiMajorAxisOrientation = 61.874992370605469;
v2x_gps.velocity = 0 ;
v2x_gps.climb = 0;
v2x_gps.time_confidence = 0.10334371030330658;
v2x_gps.velocity_confidence = 0.26266935467720032;
v2x_gps.elevation_confidence = 32;
v2x_gps.leap_seconds = 18;


}
#endif
void server_init(void)
{
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);  
	
	  if (sockfd == -1) {  
		  perror("server sockfd error");	
		  exit(1);	
	  }  
	 int optval = 1;
	  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
      perror("setsockopt1 error");
      exit(-1);
  }	
  
	
	  bzero(&server_addr, sizeof(server_addr));  
	
	  server_addr.sin_family = AF_INET;  
	  server_addr.sin_port = htons(imx_port);  
	  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	
	
	  int br =bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));	
	  if (br == -1) {  
		  perror("bind err");	
		  exit(1);	
	  }  
	
	  if ((listen(sockfd, 20)) == -1) {  
		  perror("listen err");	
		  exit(1);	
	  }  
	 #if 0
	  char buf[MAXLINE];  
	   for (;;) {  
        clientfd = accept(sockfd, (struct sockaddr *) &client_addr,&cliaddr_len);  
        printf("server get connection from %s.\n", inet_ntoa(client_addr.sin_addr));  
        int readize = 0;  
        while ((readize = read(clientfd, buf, MAXLINE)) > 0) {  
            printf("  buf =%s ", readize,buf);  
            printf("readize %d \n", readize);  
        }  
		
       }  
	#endif

}

#if 1
void init_client(void)
{
	

  if ((client_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	perror("socket1 error");
	return (-1);
  }
 
  //printf("socket create successfully.\n"); 
  int optval = 1;
  if (setsockopt(client_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
	  perror("setsockopt1 error");
	  return(-2);
  } 
  
  bzero(&remote_addr, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(server_port);
  remote_addr.sin_addr.s_addr = inet_addr(server_ip);
  

  if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)
	{
		perror("=========connect==========");
		return (-3);
	}
	printf("connected to cloud success,client is ====%d ==== \n",client_sockfd);
	
  

}
#endif

void fifo_init(void)
{
	uint16_t ret;
	if (access(HIK_TRAFFIC_FIFO_PATH, F_OK) != 0)
	{
		ret = mkfifo(HIK_TRAFFIC_FIFO_PATH, 777);
		if (ret == -1)
		{
			printf("create file failed!!!\n");
			return;
		}
	}
	printf("create file success !!!\n");
	fifo_fd = open(HIK_TRAFFIC_FIFO_PATH, O_RDWR);
	if (fifo_fd == -1)
	{
		printf("open failed!\n");
		return;
	}
}

uint64_t getCurrTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

void init_shm(void)
{
	key_t key = ftok("./", 66);
	int shmid = shmget(key, 0, 0);
	if(-1 == shmid) 
	{
		perror("shmget failed");
		return;
		// exit(1); 
	}
	p = shmat(shmid, 0, 0); 
	if((void*)-1 == p)
	{
		perror("shmat failed");
		return;
		//exit(2);

	}
	


}

#ifdef HIK_TRAFFIC

void *pthread_stm32_date_func(void* ptr)//accept stm32 data
{
	server_init();
	uint8_t recvBuf[256] = {0};
	while(1){
		clientfd = accept(sockfd, (struct sockaddr *) &client_addr,&cliaddr_len);  
		printf("server get connection from %s.\n", inet_ntoa(client_addr.sin_addr));  
		int readize = 0;
		while ((readize = read(clientfd, recvBuf, MAXLINE)) > 0) {
		
			printf("stm32 date is: %d\n", readize);
			for (int i=0; i<readize; i++)
				printf("%02x ", recvBuf[i]);
			printf("\n");

			stRsuInfoRequest.SensorRain = (uint32_t)(recvBuf[1]<<24 | recvBuf[2]<<16 | recvBuf[3]<<8 | recvBuf[4]);
			stRsuInfoRequest.SensorVisibility = (uint32_t)(recvBuf[5]<<24 | recvBuf[6]<<16 | recvBuf[7]<<8 | recvBuf[8]);
			stRsuInfoRequest.SensorWaterLogged = (uint32_t)(recvBuf[9]<<24 | recvBuf[10]<<16 | recvBuf[11]<<8 | recvBuf[12]);

			printf("==== SensorRain = %d\n", stRsuInfoRequest.SensorRain);
			printf("==== SensorVisibility = %d\n", stRsuInfoRequest.SensorVisibility);
			printf("==== SensorWaterLogged = %d\n", stRsuInfoRequest.SensorWaterLogged);
			usleep(10000);
		}
		close(clientfd);
	}
}

void *pthread_protobuf_func(void* ptr)
{
	fifo_init();
	HikTrafficLights TrafficLights = {0};
	unsigned long cnt = 33017;
	uint32_t len;
	uint64_t time = 0;
	uint32_t *protoBufMainId = NULL;
	uint32_t *protoBufSubId = NULL;
	uint32_t *protoBufLen = NULL;
	uint8_t *protoBufMessage = NULL;
	while(1){
		cnt = 0;
		protoBufMainId = (uint32_t *)(pbuf + PROTOBUF_MAIN_ID_OFFSET);
		protoBufSubId = (uint32_t *)(pbuf + PROTOBUF_SUB_ID_OFFSET);
		protoBufLen = (uint32_t *)(pbuf + PROTOBUF_LEN_OFFSET);
		protoBufMessage = (uint8_t *)(pbuf + PROTOBUF_MESSAGE_OFFSET);
		while (read(fifo_fd, &TrafficLights, sizeof(HikTrafficLights)) > 0) {	
			printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", 
					TrafficLights.u8NorthSignalL, TrafficLights.u8NorthDelayL,
					TrafficLights.u8NorthSignalD, TrafficLights.u8NorthDelayD,
					TrafficLights.u8NorthSignalR, TrafficLights.u8NorthDelayR,
					TrafficLights.u8SouthSignalL, TrafficLights.u8SouthDelayL,
					TrafficLights.u8SouthSignalD, TrafficLights.u8SouthDelayD,
					TrafficLights.u8SouthSignalR, TrafficLights.u8SouthDelayR,
					TrafficLights.u8WestSignalL, TrafficLights.u8WestDelayL,
					TrafficLights.u8WestSignalD, TrafficLights.u8WestDelayD,
					TrafficLights.u8WestSignalR, TrafficLights.u8WestDelayR,
					TrafficLights.u8EastSignalL, TrafficLights.u8EastDelayL,
					TrafficLights.u8EastSignalD, TrafficLights.u8EastDelayD,
					TrafficLights.u8EastSignalR, TrafficLights.u8EastDelayR);
			stRsuInfoRequest.NorthSignalL = (uint32_t)TrafficLights.u8NorthSignalL;
			stRsuInfoRequest.NorthDelayL = (uint32_t)TrafficLights.u8NorthDelayL;
			stRsuInfoRequest.NorthSignalD = (uint32_t)TrafficLights.u8NorthSignalD;
			stRsuInfoRequest.NorthDelayD = (uint32_t)TrafficLights.u8NorthDelayD;
			stRsuInfoRequest.NorthSignalR = (uint32_t)TrafficLights.u8NorthSignalR;
			stRsuInfoRequest.NorthDelayR = (uint32_t)TrafficLights.u8NorthDelayR;
			stRsuInfoRequest.SouthSignalL = (uint32_t)TrafficLights.u8SouthSignalL;
			stRsuInfoRequest.SouthDelayL = (uint32_t)TrafficLights.u8SouthDelayL;
			stRsuInfoRequest.SouthSignalD = (uint32_t)TrafficLights.u8SouthSignalD;
			stRsuInfoRequest.SouthDelayD = (uint32_t)TrafficLights.u8SouthDelayD;
			stRsuInfoRequest.SouthSignalR = (uint32_t)TrafficLights.u8SouthSignalR;
			stRsuInfoRequest.SouthDelayR = (uint32_t)TrafficLights.u8SouthDelayR;
			stRsuInfoRequest.WestSignalL = (uint32_t)TrafficLights.u8WestSignalL;
			stRsuInfoRequest.WestDelayL = (uint32_t)TrafficLights.u8WestDelayL;
			stRsuInfoRequest.WestSignalD = (uint32_t)TrafficLights.u8WestSignalD;
			stRsuInfoRequest.WestDelayD = (uint32_t)TrafficLights.u8WestDelayD;
			stRsuInfoRequest.WestSignalR = (uint32_t)TrafficLights.u8WestSignalR;
			stRsuInfoRequest.WestDelayR = (uint32_t)TrafficLights.u8WestDelayR;
			stRsuInfoRequest.EastSignalL = (uint32_t)TrafficLights.u8EastSignalL;
			stRsuInfoRequest.EastDelayL = (uint32_t)TrafficLights.u8EastDelayL;
			stRsuInfoRequest.EastSignalD = (uint32_t)TrafficLights.u8EastSignalD;
			stRsuInfoRequest.EastDelayD = (uint32_t)TrafficLights.u8EastDelayD;
			stRsuInfoRequest.EastSignalR = (uint32_t)TrafficLights.u8EastSignalR;
			stRsuInfoRequest.EastDelayR = (uint32_t)TrafficLights.u8EastDelayR;

			time = getCurrTime();
			stRsuInfoRequest.Timestamp = (uint64_t)(time/1000000);
			stRsuInfoRequest.RsuId = 101;

			//
			//数据流配置
			//
			ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(pbuf) - PROTOBUF_MESSAGE_OFFSET);
			//
			//将测试数据编码后写入流/protoBufMessage
			//
			rec_bool = pbEncode(&ostream, proto_RsuInfoRequest_fields, &stRsuInfoRequest);
			len = ostream.bytes_written;
			if (rec_bool == false)
			{
				printf("pbEncode failed!!!\n");
			}

			printf("Upload len = %d, encode is : ", len);
			for (uint32_t i = 0; i < len; i++)
			{
				printf("%02x ", protoBufMessage[i]);
			}
			printf("\n");

			/* test begin
			proto_RsuInfoRequest stRsuInfoRequest2 = {0};
			pb_istream_t istream;
			//
			//数据流配置
			//
			istream = pb_istream_from_buffer(protoBufMessage, len);

			//
			//将buf数据解码后写入流
			//
			rec_bool = pbDecode(&istream, proto_RsuInfoRequest_fields, &stRsuInfoRequest2);
			if (rec_bool == false)
			{
				printf("pbDecode failed!!!\n");
			}

			printf("SensorRain = %d\n", stRsuInfoRequest2.SensorRain);
			printf("SensorVisibility = %d\n", stRsuInfoRequest2.SensorVisibility);
			printf("SensorWaterLogged = %d\n", stRsuInfoRequest2.SensorWaterLogged);
			test end */

			//
			//设定MainID SubID
			//
			*protoBufMainId = htonl(3);
			*protoBufSubId = htonl(3001);
			*protoBufLen = htonl(ostream.bytes_written);


			for (int i = 0; i < PROTOBUF_MESSAGE_OFFSET + len; i++)
			{
				printf("%02x ", pbuf[i]);
			}
			printf("\n");

			pbuf_len = PROTOBUF_MESSAGE_OFFSET + len;
			user_data_to_server_len = PROTOBUF_MESSAGE_OFFSET + len;
			sem_post(&buf_number);

			printf("cnt :%d...\n", cnt++);  

			#if 0
			RegisterRequest.RsuId = 1;
			RegisterRequest.Timestamp = 2;
			ostream =  pb_ostream_from_buffer(pbuf, sizeof(pbuf));
			rec_bool = pb_encode(&ostream, proto_RegisterRequest_fields, &RegisterRequest);
			if(rec_bool == false)
			{
				printf("pb_encode failed!!!\n");
				return ;
			}

			pbuf_len = ostream.bytes_written;
			

			printf("encode is : "); 
			for(int i = 0; i < pbuf_len; i++)
			{
				printf("%c ",pbuf[i]);
			}
			#endif

			recv_flag = 1;
		}
	}
}

#else
void *pthread_protobuf_func(void* ptr)//accept stm32 data, then return to stm32
{
//char client_buf[1024]={0};//save client's sensor data ,light data
//init_gps();
//init_shm();

server_init();
unsigned long cnt = 33017;
while(1){	
	
	cnt = 0;
	clientfd = accept(sockfd, (struct sockaddr *) &client_addr,&cliaddr_len);  
	printf("server get connection from %s.\n", inet_ntoa(client_addr.sin_addr));  
	int readize = 0;  
	while ((readize = read(clientfd, pbuf, MAXLINE)) > 0) {	
		
		printf("stm32 date is: %d\n", readize);

		pbuf_len = readize;
		user_data_to_server_len = readize;
		for (int i = 0;i<pbuf_len; i++)
			printf("%02x ",pbuf[i]);
		printf("\n");
		sem_post(&buf_number);

		printf("cnt :%d...\n", cnt++);  

		#if 0
		RegisterRequest.RsuId = 1;
		RegisterRequest.Time = 2;
		ostream =  pb_ostream_from_buffer(pbuf, sizeof(pbuf));
		rec_bool = pb_encode(&ostream, proto_RegisterRequest_fields, &RegisterRequest);
		if(rec_bool == false)
		{
			printf("pb_encode failed!!!\n");
			return ;
		}
		
		pbuf_len = ostream.bytes_written;
		#endif
		/*
		printf("encode is : "); 
		for(int i = 0; i < pbuf_len; i++)
		{
			printf("%c ",pbuf[i]);
		}
		*/

		//finaly send back to check ,use heartbeat
		//read shm send to stm
		
		/*if ((0 != user_data_len) && (1== send_to_stm32_flag)){
			write(clientfd, &recv_buf[total_recv_len - user_data_len], user_data_len);
			//then  clear user_data_len
			user_data_len = 0;
			send_to_stm32_flag = 0;
			//sem_post(&buf_number);
			
			printf("====line: %d=====",__LINE__);
		}*/
		
		//write(clientfd, "ok", 2);
		recv_flag = 1;
		
		printf("===line :%d...\n", __LINE__);  
		usleep(10000);
	} 
	
	close(clientfd);
	printf("===line :%d...\n", __LINE__);  
	//usleep(1000);
}

}
#endif

#if 1
void *pthread_sendto_cloud(void* ptr)
{
	init_client();
	signal(SIGPIPE,SIG_IGN);
    sem_init(&buf_number,0 ,0);
	while(1){	
		sem_wait(&buf_number);
				
		 //then send to server cloud
	   if ( 0 != user_data_to_server_len){
		   	int ret = 0;
			int i = 0;
			
			for (i = 0;i<user_data_to_server_len;i++)
				printf("%02x ",pbuf[i]);
			printf("\n");
			
			
   			ret = send(client_sockfd,pbuf, user_data_to_server_len,0);
			printf("====send to cloud data len is %d ===line: %d==========\n",ret,__LINE__);
			
			if ( ret <= 0){
				printf("====reconnect,  close client_sockfd is %d ======\n",client_sockfd);
				close(client_sockfd);//if not use this ,will extend the fd ,and will core dump
				init_client();
				sleep(1);
			}

		   user_data_to_server_len = 0;
	   }
		//usleep(1000);
		
	}

}
#else
void *pthread_sendto_cloud(void* ptr)
{
	init_client();
	signal(SIGPIPE,SIG_IGN);
    sem_init(&buf_number,0 ,0);
	while(1){	
		sem_wait(&buf_number);
				
		 //then send to server cloud
	   if ( 0 != user_data_to_server_len){
		   	int ret = 0;
			int i = 0;
			/*
			for (i = 0;i<user_data_to_server_len;i++)
				printf("%d ",recv_buf[total_recv_len - user_data_len +i]);
			printf("\n");
			*/
			
   			ret = send(client_sockfd,&recv_buf[total_recv_len - user_data_len], user_data_to_server_len,0);
			printf("====send to cloud data len is %d ===line: %d==========\n",ret,__LINE__);
			
			if ( ret <= 0){
				printf("====reconnect,  close client_sockfd is %d ======\n",client_sockfd);
				close(client_sockfd);//if not use this ,will extend the fd ,and will core dump
				init_client();
				sleep(1);
			}

		   user_data_to_server_len = 0;
	   }
		//usleep(1000);
		
	}

}
#endif









