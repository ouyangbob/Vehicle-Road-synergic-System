/*-----------------------------------------------------------------------------*/
/*

	FileName	commRoadBlock

	EditHistory
	2019-06-11	creat file 

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

#define COMM_ROAD_BLOCK_C
#include "commRoadBlock.h" /* self head file */
/*-----------------------------------------------------------------------------*/
/* private macro define                                                        */
/*-----------------------------------------------------------------------------*/
#define TEST_COMM_RB_CLEAR

#define RB_SEND_HEADER 0x5555
#define RB_SEND_ADDR 0x01
#define RB_SEND_FUNC 0xD1

extern int speedlimit;
extern int color;

/*-----------------------------------------------------------------------------*/
/* private typedef define                                                      */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* private parameters                                                          */
/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/* private function                                                            */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	FuncName	connectServer
	function	和限速牌建立tcp网络连接
	return value	返回u_int16_t
	EditHistory	2019-04-10 creat function buy wpz
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
	server_addr.sin_port = htons(ROADBLOCK_SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(ROADBLOCK_SERVER_IP);
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
	FuncName	checkSum
	function	BCC异或校验
	return value	返回u_int8_t
	EditHistory	2019-06-11 creat function by ljl

-----------------------------------------------------------------------------*/
static uint8_t checkSum(uint8_t *data, uint8_t len)
{
	uint8_t chkSum = 0u;

	while (len > 0)
	{
		chkSum ^= data[len - 1];
		len--;
	}

	return chkSum;
}


/*-----------------------------------------------------------------------------
	FuncName	RoadBlockProcess
	function	将RoadBlock信息发送给限速牌
	return value	返回void
	EditHistory	2019-06-11 creat function by ljl
-----------------------------------------------------------------------------*/
void RoadBlockProcess(void)
{
	int client_fd = connectServer(); /* 链接符号 */
	uint32_t len;

	printf("create RoadBlockProcess thread succeed\n");

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
		uint8_t sendBuf[7] = {0};
		uint8_t buf[6];

		//定义指针指向buf
		uint16_t *u16StHeader = (uint16_t *)&buf[0];					/* 包头 固定:0x5555 RB_SEND_HEADER */
		uint8_t *u8Addr = (uint8_t *)((uint8_t *)u16StHeader + sizeof(uint16_t));	/* 地址 固定:0x01 RB_SEND_ADDR*/
		uint8_t *u8Func = (uint8_t *)((uint8_t *)u8Addr + sizeof(uint8_t));		/* 功能码 固定:0xD1 RB_SEND_FUNC*/
		uint8_t *u8Color = (uint8_t *)((uint8_t *)u8Func + sizeof(uint8_t));		/* 颜色 */
		uint8_t *u8Num = (uint8_t *)((uint8_t *)u8Color + sizeof(uint8_t)); 		/* 限速值 */

		memset(buf, 0, sizeof(buf));

		//设定固定的数据
		*u16StHeader = RB_SEND_HEADER;
		*u8Addr = RB_SEND_ADDR;
		*u8Func = RB_SEND_FUNC;

		//srand((unsigned)time(NULL));
		//*u8Color = rand() % 3;
		*u8Color = 0;
		//*u8Num = rand() % 91 + 30;
		//*u8Num = speedlimit;
		*u8Num = 15;

		printf("buf = ");
		for(int i = 0; i < sizeof(buf); i++)
		{
			printf("%02x ", buf[i]);
		}
		printf("\n");

		memcpy(sendBuf, buf, sizeof(buf));

		//做check sum校验和
		sendBuf[sizeof(buf)] = checkSum((uint8_t *)buf, sizeof(buf));

		if (send(client_fd, sendBuf, sizeof(sendBuf), 0) <= 0)
		{
			printf("send RoadBlock data failed!\n");
			continue;
		}
		printf("send SpeedLimit success! ");
		printf("sendBuf = ");
		for(int i = 0; i < sizeof(sendBuf); i++)
		{
			printf("%02x ", sendBuf[i]);
		}
		printf("\n");
		sleep(1);
	}
}


