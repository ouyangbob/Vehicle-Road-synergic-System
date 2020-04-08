/*-----------------------------------------------------------------------------*/
/*

	FileName	commNetUdp

	EditHistory
	2019-04-09	creat file 

*/
/*-----------------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../commUsart/commUsart.h"
#include "../commCan/commCan.h"
#include "../itsManager/itsManager.h"
#include "../logic/position.h"

#define COMM_NET_UDP_C
#include "commNetUdp.h" 
#define UDP_MAX_LEN 1024
#define UDP_SEND_HEADER 0x2019
#define UDP_SEND_TAIL 0x0425
#define UDP_SERVER_PORT 5555
#define UDP_CLIENT_PORT 6666
#define USART_INFO_LEN 76

pthread_mutex_t mutex1;
pthread_mutex_t mutex2;

extern CurrVehicleInfo stCurrVehicleInfo;

/*-----------------------------------------------------------------------------
	FuncName	timestamp_now
	function	获取实时时间函数
	return value	返回uint64_t时间信息
	EditHistory		2019-04-10 creat function buy wpz
-----------------------------------------------------------------------------*/
static __inline uint64_t timestamp_now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

static uint16_t checkSum(uint8_t *data, uint8_t len)
{
	uint16_t chkSum = 0u;

	while (len > 0)
	{
		chkSum += data[len - 1];
		len--;
	}

	return chkSum;
}

/*-----------------------------------------------------------------------------
	FuncName	UdpServerProcess
	function	自动驾驶与APP发送数据及接收数据（UDP）
	return value	返回 0
	EditHistory		2019-07-16 creat function by ljl
-----------------------------------------------------------------------------*/
int UdpServerProcess(void) 
{
	int sockfd;
	struct sockaddr_in server;
	socklen_t addrlen = sizeof(struct sockaddr);
	uint8_t sendBuf[UDP_MAX_LEN];

	struct GearSpeed *gearspeed = NULL;

	CurrInfoForUdp *UDPCurrInfo = (CurrInfoForUdp *)&sendBuf[0];

	proto_RsuInfoRequest *RsuInfo = NULL;
	proto_RoadBlockRequest *RoadBlockInfo = NULL;
	proto_RoadBlockRequest *SpeedLimitInfo = NULL;
	proto_position_data_t *ObuInfo = NULL;

	memset(sendBuf, 0, sizeof(sendBuf));

	if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("####L(%d) socket fail!\n", __LINE__);
		return -1;
	}
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	server.sin_port = htons(UDP_SERVER_PORT);//UDP 广播包 本地端口
	/*if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)))
	{
		printf("####L(%d) server bind port failed!\n", __LINE__);
		close(sockfd);
		return -1;
	}*/
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));

	printf("-------- %d ---------\n", sizeof(CurrInfoForUdp));
	uint32_t sendcount = 0;

	while (1)
	{
		RsuInfo = rsuInfoGet();
		UDPCurrInfo->u32RsuId = RsuInfo->RsuId;
		UDPCurrInfo->u8NorthSignalL = RsuInfo->NorthSignalL;
		UDPCurrInfo->u8NorthDelayL = RsuInfo->NorthDelayL;
		UDPCurrInfo->u8NorthSignalD = RsuInfo->NorthSignalD;
		UDPCurrInfo->u8NorthDelayD = RsuInfo->NorthDelayD;
		UDPCurrInfo->u8NorthSignalR = RsuInfo->NorthSignalR;
		UDPCurrInfo->u8NorthDelayR = RsuInfo->NorthDelayR;
		UDPCurrInfo->u8SouthSignalL = RsuInfo->SouthSignalL;
		UDPCurrInfo->u8SouthDelayL = RsuInfo->SouthDelayL;
		UDPCurrInfo->u8SouthSignalD = RsuInfo->SouthSignalD;
		UDPCurrInfo->u8SouthDelayD = RsuInfo->SouthDelayD;
		UDPCurrInfo->u8SouthSignalR = RsuInfo->SouthSignalR;
		UDPCurrInfo->u8SouthDelayR = RsuInfo->SouthDelayR;
		UDPCurrInfo->u8WestSignalL = RsuInfo->WestSignalL;
		UDPCurrInfo->u8WestDelayL = RsuInfo->WestDelayL;
		UDPCurrInfo->u8WestSignalD = RsuInfo->WestSignalD;
		UDPCurrInfo->u8WestDelayD = RsuInfo->WestDelayD;
		UDPCurrInfo->u8WestSignalR = RsuInfo->WestSignalR;
		UDPCurrInfo->u8WestDelayR = RsuInfo->WestDelayR;
		UDPCurrInfo->u8EastSignalL = RsuInfo->EastSignalL;
		UDPCurrInfo->u8EastDelayL = RsuInfo->EastDelayL;
		UDPCurrInfo->u8EastSignalD = RsuInfo->EastSignalD;
		UDPCurrInfo->u8EastDelayD = RsuInfo->EastDelayD;
		UDPCurrInfo->u8EastSignalR = RsuInfo->EastSignalR;
		UDPCurrInfo->u8EastDelayR = RsuInfo->EastDelayR;
		UDPCurrInfo->u32SensorRain = RsuInfo->SensorRain;
		UDPCurrInfo->u32SensorWaterLogged = RsuInfo->SensorWaterLogged;
		UDPCurrInfo->u32SensorVisibility = RsuInfo->SensorVisibility;

		//获取限速牌数据
		SpeedLimitInfo = SpeedLimitInfoGet();
		UDPCurrInfo->u32SpeedLimitId = SpeedLimitInfo->RsuId;
		UDPCurrInfo->u32SpeedLimit = SpeedLimitInfo->SpeedLimit;
		UDPCurrInfo->flSpeedLimitLatitude = SpeedLimitInfo->latitude;
		UDPCurrInfo->flSpeedLimitLongitude = SpeedLimitInfo->longitude;

		//获取路障数据
		RoadBlockInfo = RoadBlockInfoGet();
		UDPCurrInfo->u32RoadblockId = RoadBlockInfo->RsuId;
		UDPCurrInfo->flRoadblockLatitude = RoadBlockInfo->latitude;
		UDPCurrInfo->flRoadblockLongitude = RoadBlockInfo->longitude;

		gearspeed = getGearSpeed();
		if(gearspeed == NULL)
		{
			printf("getGearSpeed failed!\n");
			continue;
		}
		UDPCurrInfo->u32CarGear = gearspeed->Gear;
		UDPCurrInfo->flCarSpeed = gearspeed->VehicleSpeed;
		UDPCurrInfo->u32CarMotorSpeed = gearspeed->MotorSpeed;
		UDPCurrInfo->u32CarMeterTotalMileage = gearspeed->MeterTotalMileage;
		UDPCurrInfo->flCarSOCValue = gearspeed->SOCValue;
		UDPCurrInfo->u32CarBackDoorOpenSignal = gearspeed->BackDoorOpenSignal;
		UDPCurrInfo->u32CarFrontDoorOpenSignal = gearspeed->FrontDoorOpenSignal;
		UDPCurrInfo->u32CarTurnLeftInstruction = gearspeed->TurnLeftInstruction;
		UDPCurrInfo->u32CarFarLightInstruction = gearspeed->FarLightInstruction;
		UDPCurrInfo->u32CarTurnRightInstruction = gearspeed->TurnRightInstruction;

		UDPCurrInfo->flCarDirection = stCurrVehicleInfo.flCurrHeading;
		UDPCurrInfo->flCarLongitude = stCurrVehicleInfo.flCarLongitude;
		UDPCurrInfo->flCarLatitude = stCurrVehicleInfo.flCarLatitude;
		UDPCurrInfo->flAutoDriveLongitude = stCurrVehicleInfo.flAutoDriveLongitude;
		UDPCurrInfo->flAutoDriveLatitude = stCurrVehicleInfo.flAutoDriveLatitude;
		UDPCurrInfo->flMagneticLongitude = stCurrVehicleInfo.flMagneticLongitude;
		UDPCurrInfo->flMagneticLatitude = stCurrVehicleInfo.flMagneticLatitude;
		UDPCurrInfo->flCarTrace = stCurrVehicleInfo.flCurrTrace;

		stCurrVehicleInfo.flMagneticLongitude = 0.0;
		stCurrVehicleInfo.flMagneticLatitude = 0.0;

		ObuInfo = ObuToObuInfoGet();
		UDPCurrInfo->u32ObuCarId = ObuInfo->CarId;
		UDPCurrInfo->flObuLatitude = ObuInfo->latitude;
		UDPCurrInfo->flObuLongitude = ObuInfo->longitude;
		UDPCurrInfo->flObuHeading = ObuInfo->heading;
		UDPCurrInfo->flObuSpeed = ObuInfo->speed;
		UDPCurrInfo->flObuTrace = ObuInfo->trace;
		/*UDPCurrInfo->u32RsuId = 100;
		UDPCurrInfo->u8NorthSignalL = 0;
		UDPCurrInfo->u8NorthDelayL = 1;
		UDPCurrInfo->u8NorthSignalD = 2;
		UDPCurrInfo->u8NorthDelayD = 3;
		UDPCurrInfo->u8NorthSignalR = 4;
		UDPCurrInfo->u8NorthDelayR = 5;
		UDPCurrInfo->u8SouthSignalL = 6;
		UDPCurrInfo->u8SouthDelayL = 7;
		UDPCurrInfo->u8SouthSignalD = 8;
		UDPCurrInfo->u8SouthDelayD = 9;
		UDPCurrInfo->u8SouthSignalR = 10;
		UDPCurrInfo->u8SouthDelayR = 11;
		UDPCurrInfo->u8WestSignalL = 12;
		UDPCurrInfo->u8WestDelayL = 13;
		UDPCurrInfo->u8WestSignalD = 14;
		UDPCurrInfo->u8WestDelayD = 15;
		UDPCurrInfo->u8WestSignalR = 16;
		UDPCurrInfo->u8WestDelayR = 17;
		UDPCurrInfo->u8EastSignalL = 18;
		UDPCurrInfo->u8EastDelayL = 19;
		UDPCurrInfo->u8EastSignalD = 20;
		UDPCurrInfo->u8EastDelayD = 21;
		UDPCurrInfo->u8EastSignalR = 22;
		UDPCurrInfo->u8EastDelayR = 23;
		UDPCurrInfo->u32SensorRain = 24;
		UDPCurrInfo->u32SensorWaterLogged = 25;
		UDPCurrInfo->u32SensorVisibility = 26;

		UDPCurrInfo->u32SpeedLimitId = 200;
		UDPCurrInfo->u32SpeedLimit = 60;
		UDPCurrInfo->flSpeedLimitLatitude = 28.123456;
		UDPCurrInfo->flSpeedLimitLongitude = 113.123456;

		UDPCurrInfo->u32RoadblockId = 300;
		UDPCurrInfo->flRoadblockLatitude = 28.123456;
		UDPCurrInfo->flRoadblockLongitude = 113.123456;

		UDPCurrInfo->u32CarGear = 1;
		UDPCurrInfo->flCarSpeed = 15.5;
		UDPCurrInfo->u32CarMotorSpeed = 360;
		UDPCurrInfo->u32CarMeterTotalMileage = 720;
		UDPCurrInfo->flCarSOCValue = 78.123;
		UDPCurrInfo->u32CarBackDoorOpenSignal = 1;
		UDPCurrInfo->u32CarFrontDoorOpenSignal = 1;
		UDPCurrInfo->u32CarTurnLeftInstruction = 1;
		UDPCurrInfo->u32CarFarLightInstruction = 1;
		UDPCurrInfo->u32CarTurnRightInstruction = 1;

		UDPCurrInfo->flCarDirection = 58.123;
		UDPCurrInfo->flCarLongitude = 118.123;
		UDPCurrInfo->flCarLatitude = 24.123;
		UDPCurrInfo->flCarTrace = 23.231;*/

		printf("u32SpeedLimit = %d\n", UDPCurrInfo->u32SpeedLimit);
		printf("flSpeedLimitLongitude = %f\n", UDPCurrInfo->flSpeedLimitLongitude);
		printf("flSpeedLimitLatitude = %f\n", UDPCurrInfo->flSpeedLimitLatitude);
		printf("flRoadblockLongitude = %f\n", UDPCurrInfo->flRoadblockLongitude);
		printf("flRoadblockLatitude = %f\n", UDPCurrInfo->flRoadblockLatitude);
		printf("flCarDirection = %f\n", UDPCurrInfo->flCarDirection);
		printf("flCarLongitude = %f\n", UDPCurrInfo->flCarLongitude);
		printf("flCarLatitude = %f\n", UDPCurrInfo->flCarLatitude);
		printf("flCarTrace = %f\n", UDPCurrInfo->flCarTrace);
		printf("flCarSpeed = %f\n", UDPCurrInfo->flCarSpeed);
		printf("flCarSOCValue = %f\n", UDPCurrInfo->flCarSOCValue);
		printf("flMagneticLongitude = %f\n", UDPCurrInfo->flMagneticLongitude);
		printf("flMagneticLatitude = %f\n", UDPCurrInfo->flMagneticLatitude);
		printf("flAutoDriveLongitude = %f\n", UDPCurrInfo->flAutoDriveLongitude);
		printf("flAutoDriveLatitude = %f\n", UDPCurrInfo->flAutoDriveLatitude);

		//printf("u32CarGear = %d\n", UDPCurrInfo->u32CarGear);
		//printf("u32CarMotorSpeed = %d\n", UDPCurrInfo->u32CarMotorSpeed);
		//printf("u32CarMeterTotalMileage = %d\n", UDPCurrInfo->u32CarMeterTotalMileage);
		//printf("u32CarBackDoorOpenSignal = %d\n", UDPCurrInfo->u32CarBackDoorOpenSignal);
		//printf("u32CarFrontDoorOpenSignal = %d\n", UDPCurrInfo->u32CarFrontDoorOpenSignal);
		//printf("u32CarTurnLeftInstruction = %d\n", UDPCurrInfo->u32CarTurnLeftInstruction);
		//printf("u32CarFarLightInstruction = %d\n", UDPCurrInfo->u32CarFarLightInstruction);
		//printf("u32CarTurnRightInstruction = %d\n", UDPCurrInfo->u32CarTurnRightInstruction);
		//发送数据
		sendcount = sendto(sockfd, sendBuf, sizeof(CurrInfoForUdp), 0, (struct sockaddr *)&server, addrlen);
		if (sendcount > 0)
		{
			printf("==== send success, sendcount = %d\n", sendcount);
			for (int i = 0; i < sizeof(CurrInfoForUdp); i++)
			{
				printf("%02x ", sendBuf[i]);
			}
			printf("\n");
		}
		memset(sendBuf, 0, sizeof(sendBuf));
		usleep(500000);
	}
	close(sockfd);
	return 0;
}

int UdpServerProcess_back(void) 
{
	int sockfd;
	struct sockaddr_in server;
	socklen_t addrlen = sizeof(struct sockaddr);
	struct GearSpeed *gearspeed = NULL;
	uint8_t sendBuf[UDP_MAX_LEN];

	//定义指针指向sendBuf
	uint16_t *u16StHeader = (uint16_t *)&sendBuf[0];									/* 包头  固定:0x2019 UDP_SEND_HEADER */
	uint64_t *u64Time = (uint64_t *)((uint8_t *)u16StHeader + sizeof(uint16_t));		/* 时间戳 */
	uint8_t *u8StLength = (uint8_t *)((uint8_t *)u64Time + sizeof(uint64_t));			/* 长度 (u8NorthSignalL -> u8SensorVisibility 27字节) */
	CurrInfoForUdp *UDPCurrInfo = (CurrInfoForUdp *)((uint8_t *)u8StLength + sizeof(uint8_t));
	uint16_t *u16CrcCheck = (uint16_t *)((uint8_t *)UDPCurrInfo + sizeof(CurrInfoForUdp));		/* Crc 校验 */
	uint16_t *u16StTail = (uint16_t *)((uint8_t *)u16CrcCheck + sizeof(uint16_t));			/* 包尾 固定0x425 UDP_SEND_TAIL */

	proto_RsuInfoRequest *RsuInfo = NULL;
	proto_RoadBlockRequest *RoadBlockInfo = NULL;
	proto_RoadBlockRequest *SpeedLimitInfo = NULL;
	proto_position_data_t *ObuInfo = NULL;

	memset(sendBuf, 0, sizeof(sendBuf));

	//设定固定的数据
	*u16StHeader = UDP_SEND_HEADER;
	*u8StLength = sizeof(CurrInfoForUdp);
	*u16StTail = UDP_SEND_TAIL;

	//服务端绑定准备
	if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("####L(%d) socket fail!\n", __LINE__);
		return -1;
	}
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	server.sin_port = htons(UDP_SERVER_PORT);//UDP 广播包 本地端口
	if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)))
	{
		printf("####L(%d) server bind port failed!\n", __LINE__);
		close(sockfd);
		return -1;
	}
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));

	printf("-------- %d ---------\n", sizeof(CurrInfoForUdp));
	uint32_t sendlen = sizeof(CurrInfoForUdp) + sizeof(uint16_t) + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);
	printf("------------ UdpServerProcess sendlen = %d -----------\n", sendlen);
	uint32_t sendcount = 0;
	while (1)
	{
		RsuInfo = rsuInfoGet();
		UDPCurrInfo->u32RsuId = RsuInfo->RsuId;
		UDPCurrInfo->u8NorthSignalL = RsuInfo->NorthSignalL;
		UDPCurrInfo->u8NorthDelayL = RsuInfo->NorthDelayL;
		UDPCurrInfo->u8NorthSignalD = RsuInfo->NorthSignalD;
		UDPCurrInfo->u8NorthDelayD = RsuInfo->NorthDelayD;
		UDPCurrInfo->u8NorthSignalR = RsuInfo->NorthSignalR;
		UDPCurrInfo->u8NorthDelayR = RsuInfo->NorthDelayR;
		UDPCurrInfo->u8SouthSignalL = RsuInfo->SouthSignalL;
		UDPCurrInfo->u8SouthDelayL = RsuInfo->SouthDelayL;
		UDPCurrInfo->u8SouthSignalD = RsuInfo->SouthSignalD;
		UDPCurrInfo->u8SouthDelayD = RsuInfo->SouthDelayD;
		UDPCurrInfo->u8SouthSignalR = RsuInfo->SouthSignalR;
		UDPCurrInfo->u8SouthDelayR = RsuInfo->SouthDelayR;
		UDPCurrInfo->u8WestSignalL = RsuInfo->WestSignalL;
		UDPCurrInfo->u8WestDelayL = RsuInfo->WestDelayL;
		UDPCurrInfo->u8WestSignalD = RsuInfo->WestSignalD;
		UDPCurrInfo->u8WestDelayD = RsuInfo->WestDelayD;
		UDPCurrInfo->u8WestSignalR = RsuInfo->WestSignalR;
		UDPCurrInfo->u8WestDelayR = RsuInfo->WestDelayR;
		UDPCurrInfo->u8EastSignalL = RsuInfo->EastSignalL;
		UDPCurrInfo->u8EastDelayL = RsuInfo->EastDelayL;
		UDPCurrInfo->u8EastSignalD = RsuInfo->EastSignalD;
		UDPCurrInfo->u8EastDelayD = RsuInfo->EastDelayD;
		UDPCurrInfo->u8EastSignalR = RsuInfo->EastSignalR;
		UDPCurrInfo->u8EastDelayR = RsuInfo->EastDelayR;
		UDPCurrInfo->u32SensorRain = RsuInfo->SensorRain;
		UDPCurrInfo->u32SensorWaterLogged = RsuInfo->SensorWaterLogged;
		UDPCurrInfo->u32SensorVisibility = RsuInfo->SensorVisibility;

		//获取限速牌数据
		SpeedLimitInfo = SpeedLimitInfoGet();
		UDPCurrInfo->u32SpeedLimitId = SpeedLimitInfo->RsuId;
		UDPCurrInfo->u32SpeedLimit = SpeedLimitInfo->SpeedLimit;
		UDPCurrInfo->flSpeedLimitLatitude = SpeedLimitInfo->latitude;
		UDPCurrInfo->flSpeedLimitLongitude = SpeedLimitInfo->longitude;

		//获取路障数据
		RoadBlockInfo = RoadBlockInfoGet();
		UDPCurrInfo->u32RoadblockId = RoadBlockInfo->RsuId;
		UDPCurrInfo->flRoadblockLatitude = RoadBlockInfo->latitude;
		UDPCurrInfo->flRoadblockLongitude = RoadBlockInfo->longitude;

		gearspeed = getGearSpeed();
		if(gearspeed == NULL)
		{
			printf("getGearSpeed failed!\n");
			continue;
		}
		UDPCurrInfo->u32CarGear = gearspeed->Gear;
		UDPCurrInfo->flCarSpeed = gearspeed->VehicleSpeed;
		UDPCurrInfo->u32CarMotorSpeed = gearspeed->MotorSpeed;
		UDPCurrInfo->u32CarMeterTotalMileage = gearspeed->MeterTotalMileage;
		UDPCurrInfo->flCarSOCValue = gearspeed->SOCValue;
		UDPCurrInfo->u32CarBackDoorOpenSignal = gearspeed->BackDoorOpenSignal;
		UDPCurrInfo->u32CarFrontDoorOpenSignal = gearspeed->FrontDoorOpenSignal;
		UDPCurrInfo->u32CarTurnLeftInstruction = gearspeed->TurnLeftInstruction;
		UDPCurrInfo->u32CarFarLightInstruction = gearspeed->FarLightInstruction;
		UDPCurrInfo->u32CarTurnRightInstruction = gearspeed->TurnRightInstruction;

		UDPCurrInfo->flCarDirection = stCurrVehicleInfo.flCurrHeading;
		UDPCurrInfo->flCarLongitude = stCurrVehicleInfo.flCarLongitude;
		UDPCurrInfo->flCarLatitude = stCurrVehicleInfo.flCarLatitude;
		UDPCurrInfo->flAutoDriveLongitude = stCurrVehicleInfo.flAutoDriveLongitude;
		UDPCurrInfo->flAutoDriveLatitude = stCurrVehicleInfo.flAutoDriveLatitude;
		UDPCurrInfo->flMagneticLongitude = stCurrVehicleInfo.flMagneticLongitude;
		UDPCurrInfo->flMagneticLatitude = stCurrVehicleInfo.flMagneticLatitude;
		UDPCurrInfo->flCarTrace = stCurrVehicleInfo.flCurrTrace;

		ObuInfo = ObuToObuInfoGet();
		UDPCurrInfo->u32ObuCarId = ObuInfo->CarId;
		UDPCurrInfo->flObuLatitude = ObuInfo->latitude;
		UDPCurrInfo->flObuLongitude = ObuInfo->longitude;
		UDPCurrInfo->flObuHeading = ObuInfo->heading;
		UDPCurrInfo->flObuSpeed = ObuInfo->speed;
		UDPCurrInfo->flObuTrace = ObuInfo->trace;

		//做check sum校验和
		uint32_t sum = 0;
		sum = checkSum((uint8_t *)UDPCurrInfo, sizeof(CurrInfoForUdp));
		*u16CrcCheck = sum;

		//更新时间戳
		*u64Time = timestamp_now();

		//发送数据
		sendcount = sendto(sockfd, sendBuf, sendlen, 0, (struct sockaddr *)&server, addrlen);
		if (sendcount > 0)
		{
			printf("==== send success : ");
			for (int i = 0; i < sendlen; i++)
			{
				printf("%02x ", sendBuf[i]);
			}
			printf("\n");
		}
		usleep(500000);
	}
	//free(currInfo);
	close(sockfd);
	return 0;
}

int UdpRecvProcess(void)
{
	int sockfd;
	struct sockaddr_in client;
	socklen_t addrlen = sizeof(struct sockaddr);
	uint8_t recvBuf[UDP_MAX_LEN];

	//服务端绑定准备
	if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("####L(%d) socket fail!\n", __LINE__);
		return -1;
	}
	bzero(&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_addr.s_addr = htonl(INADDR_ANY);
	client.sin_port = htons(UDP_CLIENT_PORT);//UDP 广播包 远端端口
	if(bind(sockfd, (struct sockaddr *)&client, sizeof(client)))
	{
		close(sockfd);
		return -1;
	}
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));

	uint32_t recvcount = 0;
	float longitude = 0.0, latitude = 0.0, trace = 0.0;
	while (1)
	{
		memset(recvBuf, 0, sizeof(recvBuf));
		recvcount = recvfrom(sockfd, recvBuf, sizeof(recvBuf), 0, (struct sockaddr *)&client, &addrlen);
		int num;
		if(recvcount > 0)
        {
			//printf("============ recv_msg: %s\n", recvBuf);
			sscanf(recvBuf, "%f,%f,%f,%d", &longitude, &latitude, &trace, &num);
//#if CURR_GPS == AUTODRIVE_GPS
    		stCurrVehicleInfo.flAutoDriveLongitude = longitude;
    		stCurrVehicleInfo.flAutoDriveLatitude = latitude;
//#endif
			stCurrVehicleInfo.flCurrTrace = trace;
			//printf("%f, %f, %f, %d\n", longitude, latitude, trace, num);
        }
		//usleep(500000);
	}
	close(sockfd);
	return 0;
}
