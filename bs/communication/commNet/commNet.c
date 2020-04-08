/*-----------------------------------------------------------------------------*/
/*

	FileName	commNet

	EditHistory
	2019-04-09	creat file 
	2019-04-10	server test

*/
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* general include                                                             */
/*-----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/ip.h>
#include <netinet/tcp.h>

#include <sys/time.h>
#include <time.h>

/*-----------------------------------------------------------------------------*/
/* module include                                                             */
/*-----------------------------------------------------------------------------*/
#include "../proto/pb_common.h"
#include "../proto/pb_decode.h"
#include "../proto/pb_encode.h"
#include "../proto/pb.h"
#include "../proto/test.pb.h"

#include "../../TargetSettings.h"

#define COMM_NET_C
#include "commNet.h" /* self head file */
/*-----------------------------------------------------------------------------*/
/* private macro define                                                        */
/*-----------------------------------------------------------------------------*/
#define TEST_COMM_NET_CLEAR

#define PROTOBUF_MAIN_ID_OFFSET 0
#define PROTOBUF_SUB_ID_OFFSET 4
#define PROTOBUF_LEN_OFFSET 8
#define PROTOBUF_MESSAGE_OFFSET 12
#define TRAFFIC_FIFO_PATH "./traffic_fifo"

static proto_RoadBlockResponse stRoadBlockResponse = {0};
int speedlimit = 0, color = 0;

/*-----------------------------------------------------------------------------*/
/* private typedef define                                                      */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* private parameters                                                          */
/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/* private function                                                             */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/*
	FuncName	getCurrTime
    
    return current time stamp in milliseconds
    @return long long

	EditHistory
	2019-04-11	 from acme
*/
uint64_t getCurrTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/*-----------------------------------------------------------------------------
	FuncName	connectServer
	function	和云平台建立tcp网络连接
	return value	返回u_int16_t
	EditHistory		2019-04-10 creat function buy wpz
-----------------------------------------------------------------------------*/
static u_int16_t connectServer(void)
{
	struct sockaddr_in server_addr;
	int client_fd = -1;

	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0)
	{
		printf("create socket failed\n");
		return 0;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PROTOBUF_SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(PROTOBUF_SERVER_IP);
	if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		client_fd = -1;
		printf("connect failed\n");
		return 0;
	}
	printf("connect succeed\n");
	return client_fd;
}

/*-----------------------------------------------------------------------------
	FuncName	uploadRoadBlockProcess
	function	将RoadBlock信息发送给云平台
	return value	返回void
	EditHistory		2019-06-03 creat function by ljl
-----------------------------------------------------------------------------*/
void uploadRoadBlockProcess(proto_RoadBlockRequest *(RoadBlockInfoGet)(void))
{
	uint64_t time = 0;
	uint32_t n = 0;
	uint8_t recvBuf[100] = {0};
	uint32_t u32loopCount = 0;
	pb_ostream_t ostream;
	bool rec_bool;
	pb_istream_t istream;
	uint32_t len;
	uint32_t recvLen;
	uint8_t sendBuf[256];
	uint32_t *protoBufMainId = NULL;
	uint32_t *protoBufSubId = NULL;
	uint32_t *protoBufLen = NULL;
	uint8_t *protoBufMessage = NULL;
	int client_fd = connectServer();
	printf("create thread succeed\n");

	while (1)
	{
		struct tcp_info info;
		len = sizeof(info);
		getsockopt(client_fd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
		if ((info.tcpi_state != TCP_ESTABLISHED))
		{
			perror(strerror(errno));
			printf("server close!!!\n");
			close(client_fd);
			client_fd = connectServer();
			if (client_fd == 0)
			{
				printf("create socket failed or connect failed!\n");
			}
			sleep(1);
		}

		protoBufMainId = (uint32_t *)(sendBuf + PROTOBUF_MAIN_ID_OFFSET);
		protoBufSubId = (uint32_t *)(sendBuf + PROTOBUF_SUB_ID_OFFSET);
		protoBufLen = (uint32_t *)(sendBuf + PROTOBUF_LEN_OFFSET);
		protoBufMessage = (uint8_t *)(sendBuf + PROTOBUF_MESSAGE_OFFSET);
		proto_RoadBlockRequest *RoadBlockRequest = RoadBlockInfoGet();
		//根据获取的时间来判断，如果为0 的话没必要上传。
		if (RoadBlockRequest != NULL)
		{
#ifndef TEST_COMM_NET_CLEAR
			printf("\n=========UPLOAD_START==========\n");
			printf("Id = %d\n", RoadBlockRequest->RsuId);
			printf("Longitude = %f\n", RoadBlockRequest->longitude);
			printf("Latitude = %f\n", RoadBlockRequest->latitude);
#endif
			time = getCurrTime();
			RoadBlockRequest->Time = (uint64_t)(time/1000000);

			//
			//数据流配置
			//
			ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(sendBuf) - PROTOBUF_MESSAGE_OFFSET);
			//
			//将测试数据编码后写入流/protoBufMessage
			//
			rec_bool = pbEncode(&ostream, proto_RoadBlockRequest_fields, RoadBlockRequest);
			len = ostream.bytes_written;
			if (rec_bool == false)
			{
				printf("pbEncode failed!!!\n");
			}

#ifndef TEST_COMM_NET_CLEAR
			printf("Upload len = %d, encode is : ", len);
			for (uint32_t i = 0; i < len; i++)
			{
				printf("%02x ", protoBufMessage[i]);
			}
			printf("\n");
#endif
			//
			//设定MainID SubID
			//
			*protoBufMainId = htonl(3);
			*protoBufSubId = htonl(3003);
			*protoBufLen = htonl(ostream.bytes_written);

//#ifndef TEST_COMM_NET_CLEAR
			printf("speedlimit sendBuf:");
			for (int i = 0; i < PROTOBUF_MESSAGE_OFFSET + len; i++)
			{
				printf("%02x ", sendBuf[i]);
			}
			printf("\n");
//#endif
			n = send(client_fd, sendBuf, PROTOBUF_MESSAGE_OFFSET + len, 0);
			if (n < PROTOBUF_MESSAGE_OFFSET + len)
			{
				printf("send_failed\n");
				memset(sendBuf, 0, sizeof(sendBuf));
				continue;
			}
#ifndef TEST_COMM_NET_CLEAR
			printf("fd = %d\n", client_fd);

			printf("n = %d\n", n);
#endif
			memset(sendBuf, 0, sizeof(sendBuf));

			recvLen = recv(client_fd, recvBuf, 100, 0);
			if (recvLen <= 0)
			{
				printf("recv_failed\n");
				memset(recvBuf, 0, sizeof(recvBuf));
				continue;
			}

#ifndef TEST_COMM_NET_CLEAR
			printf("recv:\n");
			for (uint32_t i = 0; i < recvLen; i++)
			{
				printf("%x ,", recvBuf[i]);
			}
#endif

			protoBufMessage = (uint8_t *)(recvBuf + PROTOBUF_MESSAGE_OFFSET);
			//
			//数据流配置
			//
			istream = pb_istream_from_buffer(protoBufMessage, sizeof(recvBuf) - PROTOBUF_MESSAGE_OFFSET);

			//
			//将buf数据解码后写入流
			//
			rec_bool = pbDecode(&istream, proto_RoadBlockResponse_fields, &stRoadBlockResponse);
			if (rec_bool == false)
			{
				printf("pbDecode failed!!!\n");
			}

			printf("\n==========response========\n");
			printf("Carid = %d\n", stRoadBlockResponse.RsuId);
			printf("Color = %d\n", stRoadBlockResponse.Color);
			printf("SpeedLimit = %f\n", stRoadBlockResponse.SpeedLimit);
			printf("\n=========UPLOAD_END_%d==========\n", u32loopCount++);

			speedlimit = (int)stRoadBlockResponse.SpeedLimit;
			color = stRoadBlockResponse.Color;
			/*char buffer[32];
			sprintf(buffer, "%f", stRoadBlockResponse.SpeedLimit);
			printf("---- %s ----\n", buffer);
			int a, b = 0;
			sscanf(buffer, "%d.%d", &a, &b);
			printf("=== a = %d, b = %d ===\n", a, b);*/
			memset(recvBuf, 0, sizeof(recvBuf));
			sleep(5); //数据十秒发送一次给云平台
		}
	}
}
