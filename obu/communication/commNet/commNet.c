
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
#include <stdbool.h>

#include "../proto/pb_common.h"
#include "../proto/pb_decode.h"
#include "../proto/pb_encode.h"
#include "../proto/pb.h"
#include "../proto/test.pb.h"
#include "../commUsart/commUsart.h"
#include "../commCan/commCan.h"
#include "../itsManager/itsManager.h"
#include "../logic/position.h"
#include "../../TargetSettings.h"

#define COMM_NET_C
#include "commNet.h"
#define PROTOBUF_MAIN_ID_OFFSET 0
#define PROTOBUF_SUB_ID_OFFSET 4
#define PROTOBUF_LEN_OFFSET 8
#define PROTOBUF_MESSAGE_OFFSET 12

extern CurrVehicleInfo stCurrVehicleInfo;

static u_int16_t connectServer(void)
{
	printf("now connect yunpingtai!\n");
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
	printf("connect cloud platform succeed\n");
	return client_fd;
}

/*-----------------------------------------------------------------------------
	FuncName	uploadCanUwbProcess
	function	把can uwb数据发送给云平台
	return value	返回void
	EditHistory		2019-04-10 creat function buy linh
-----------------------------------------------------------------------------*/
void uploadCanUwbProcess()
{
	uint32_t n = 0;
	uint32_t t = 0;
	uint64_t time = 0;
	pb_ostream_t ostream;
	bool rec_bool;
	pb_istream_t istream;
	uint32_t len;
	uint32_t recvLen;
	uint8_t sendBuf[256] = {0};
	uint32_t *protoBufMainId = NULL;
	uint32_t *protoBufSubId = NULL;
	uint32_t *protoBufLen = NULL;
	uint8_t *protoBufMessage = NULL;
	proto_CANMessage *Vech = NULL;
	int client_fd = connectServer();
	if (client_fd < 0)
	{
		printf("socket err\n");
		return;
	}
	printf("========== %s, line: %d ==========\n", __FUNCTION__, __LINE__);
	uint64_t realtime = 0;
	int16_t ret_fifo = 0;
	proto_ObuTrafficLightsRequest *obuTrafficLightsRequest = NULL;
	protoBufMainId = (uint32_t *)(sendBuf + PROTOBUF_MAIN_ID_OFFSET);
	protoBufSubId = (uint32_t *)(sendBuf + PROTOBUF_SUB_ID_OFFSET);
	protoBufLen = (uint32_t *)(sendBuf + PROTOBUF_LEN_OFFSET);
	protoBufMessage = (uint8_t *)(sendBuf + PROTOBUF_MESSAGE_OFFSET);

	proto_uwb uwb_data = {0};
	int count = 0;
	int i = 0, j = 0;
	while (1)
	{
		sleep(1);
		struct tcp_info info;
		int len = sizeof(info);
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
				continue;
			}
			sleep(1);
		}
		realtime = (getCurrTime()) / 1000000;
		Vech = getCANMessage();
		if (Vech == NULL)
		{
			printf("getVechileInfo failed!\n");
			continue;
		}
		Vech->Bms1Msg.Timestamp = realtime;
		Vech->Meter1Msg.Timestamp = realtime;
		Vech->Meter2Msg.Timestamp = realtime;

#if CURR_GPS == UWB_GPS
		//UWB数据上云
		uwb_data.Timestamp = realtime;
		uwb_data.CarId = CAR_ID;
		uwb_data.Latitude = stCurrVehicleInfo.flCarLatitude;
		uwb_data.Longitude = stCurrVehicleInfo.flCarLongitude;
		uwb_data.NorthHemisphere = stCurrVehicleInfo.NorthHemisphere;
		uwb_data.EastHemisphere = stCurrVehicleInfo.EastHemisphere;

		printf("uwb_date.Latitude = %0.4f\n", uwb_data.Latitude);
		printf("uwb_date.Longitude = %0.4f\n", uwb_data.Longitude);
		printf("uwb_date.NorthHemisphere = %d\n", uwb_data.NorthHemisphere);
		printf("uwb_date.EastHemisphere = %d\n", uwb_data.EastHemisphere);
		ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(sendBuf) - PROTOBUF_MESSAGE_OFFSET);
		//将测试数据编码后写入流/sendBuf
		rec_bool = pbEncode(&ostream, proto_uwb_fields, &uwb_data);
		len = ostream.bytes_written;
		if (rec_bool == false)
		{
			printf("pbEncode failed!!!\n");
		}
		printf("UWB data encode is : ");
		for (uint32_t i = 0; i < len; i++)
		{
			printf("%x ", protoBufMessage[i]);
		}
		printf("\n");
		*protoBufMainId = htonl(10); //设定MainID SubID
		*protoBufSubId = htonl(10001);
		*protoBufLen = htonl(ostream.bytes_written);
		n = send(client_fd, sendBuf, PROTOBUF_MESSAGE_OFFSET + len, 0);
		// for (uint32_t i = 0; i < len + PROTOBUF_MESSAGE_OFFSET; i++)
		// {
		// 	printf("%x ", sendBuf[i]);
		// }
		// printf("\n");
		memset(sendBuf, 0, sizeof(sendBuf));
#endif

		obuTrafficLightsRequest = ObuTrafficInfoGet();
		obuTrafficLightsRequest->Timestamp = realtime;
		obuTrafficLightsRequest->VechicleId = CAR_ID;
		printf("obuTrafficLightsRequest->TrafficId = %d\n", obuTrafficLightsRequest->TrafficId);
		printf("obuTrafficLightsRequest->VechicleLatitude = %0.2lf\n", obuTrafficLightsRequest->VechicleLatitude);
		printf("obuTrafficLightsRequest->VechicleLongitude = %0.2lf\n", obuTrafficLightsRequest->VechicleLongitude);
		//obuTrafficLightsRequest->VechicleForwardTurning = 1;
		//obuTrafficLightsRequest->VechicleLaneChangingIntention = 1;
		//obuTrafficLightsRequest->VechicleSpeedChangeIntention = 1;
		//obuTrafficLightsRequest->TrafficId = 1;
		//obuTrafficLightsRequest->TrafficDirection = 1;
		//obuTrafficLightsRequest->TrafficTrace = 1;
		//obuTrafficLightsRequest->TrafficSignal = 1;
		//obuTrafficLightsRequest->TrafficDelay = 1;
		ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(sendBuf) - PROTOBUF_MESSAGE_OFFSET);
		//将测试数据编码后写入流/sendBuf
		rec_bool = pbEncode(&ostream, proto_ObuTrafficLightsRequest_fields, obuTrafficLightsRequest);
		len = ostream.bytes_written;
		if (rec_bool == false)
		{
			printf("pbEncode failed!!!\n");
		}
		printf("车身GPS数据及车辆转向数据 encode is : ");
		for (uint32_t i = 0; i < len; i++)
		{
			printf("%x ", protoBufMessage[i]);
		}
		printf("\n");
		*protoBufMainId = htonl(4); //设定MainID SubID
		*protoBufSubId = htonl(4001);
		*protoBufLen = htonl(ostream.bytes_written);
		n = send(client_fd, sendBuf, PROTOBUF_MESSAGE_OFFSET + len, 0);
		memset(sendBuf, 0, sizeof(sendBuf));

		// memset(&Vech->Bms1Msg, 0, sizeof(Vech->Bms1Msg));
		// Vech->Bms1Msg.CarId = 1;
		// time = getCurrTime();
		// Vech->Bms1Msg.Timestamp = (uint64_t)(time/1000000);
		// Vech->Bms1Msg.TotalBatteryVoltage = 3;
		// Vech->Bms1Msg.RechargeDischargeElectricity = 4;
		// Vech->Bms1Msg.SOCValue = 5;
		// Vech->Bms1Msg.PressureBig = 6;
		// Vech->Bms1Msg.TotalOvervoltage = 7;
		// Vech->Bms1Msg.TotalUndervoltage = 8;
		// Vech->Bms1Msg.InsideCommunicationFail = 9;
		// Vech->Bms1Msg.SOCLow = 10;
		// Vech->Bms1Msg.SOCHigt = 11;
		// Vech->Bms1Msg.MonocaseUndervoltage = 12;
		// Vech->Bms1Msg.MonocaseOvervoltage = 1;
		// Vech->Bms1Msg.TemperatureOverSize = 1;
		// Vech->Bms1Msg.TemperatureRiseOversize = 1;
		// Vech->Bms1Msg.TemperatureOverTop = 1;
		// Vech->Bms1Msg.RechargeElectricityOverSize = 1;
		// Vech->Bms1Msg.DischargeElectricityOverSize = 1;
		// Vech->Bms1Msg.InsulationAlarm = 1;
		// Vech->Bms1Msg.SmogAlarm = 1;
		// Vech->Bms1Msg.CellTemperatureOverSize = 1;
		// Vech->Bms1Msg.FuseStatus = 1;
		// Vech->Bms1Msg.CurrentRechargeStatus = 1;
		// Vech->Bms1Msg.RechargeConnectSureSignal = 1;
		// Vech->Bms1Msg.CommunicationWithBatteryCharger = 1;
		// Vech->Bms1Msg.HaveCapacitySmallCell = 1;
		// Vech->Bms1Msg.HaveResistanceBigCell = 1;
		// Vech->Bms1Msg.InsideContactorSignal = 1;
		// Vech->Bms1Msg.RadiatSystemRunning = 1;

		ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(sendBuf) - PROTOBUF_MESSAGE_OFFSET);
		//将测试数据编码后写入流/sendBuf
		rec_bool = pbEncode(&ostream, proto_Bms1_fields, &(Vech->Bms1Msg));
		len = ostream.bytes_written;
		if (rec_bool == false)
		{
			printf("pbEncode failed!!!\n");
		}
		printf("电池1 encode is : ");
		for (uint32_t i = 0; i < len; i++)
		{
			printf("%x ", protoBufMessage[i]);
		}
		printf("\n");
		*protoBufMainId = htonl(8); //设定MainID SubID
		*protoBufSubId = htonl(8001);
		*protoBufLen = htonl(ostream.bytes_written);
		// printf("电池1 sendbuf = ");
		// for(int i = 0; i < 10; i++)
		// {
		// 	printf("%x ",sendBuf[i]);
		// }
		// printf("\n");
		n = send(client_fd, sendBuf, PROTOBUF_MESSAGE_OFFSET + len, 0);
		printf("send number = %d\n", n);
		memset(sendBuf, 0, sizeof(sendBuf));

		ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(sendBuf) - PROTOBUF_MESSAGE_OFFSET);
		//将测试数据编码后写入流/sendBuf
		rec_bool = pbEncode(&ostream, proto_Meter1_fields, &(Vech->Meter1Msg));
		len = ostream.bytes_written;
		if (rec_bool == false)
		{
			printf("pbEncode failed!!!\n");
		}
		printf("仪表盘1 encode is : ");
		for (uint32_t i = 0; i < len; i++)
		{
			printf("%x ", protoBufMessage[i]);
		}
		printf("\n");
		*protoBufMainId = htonl(6); //设定MainID SubID
		*protoBufSubId = htonl(6001);
		*protoBufLen = htonl(ostream.bytes_written);
		n = send(client_fd, sendBuf, PROTOBUF_MESSAGE_OFFSET + len, 0);
		memset(sendBuf, 0, sizeof(sendBuf));

		ostream = pb_ostream_from_buffer(protoBufMessage, sizeof(sendBuf) - PROTOBUF_MESSAGE_OFFSET);
		//将测试数据编码后写入流/sendBuf
		rec_bool = pbEncode(&ostream, proto_Meter2_fields, &(Vech->Meter2Msg));
		len = ostream.bytes_written;
		if (rec_bool == false)
		{
			printf("pbEncode failed!!!\n");
		}
		printf("仪表盘2 encode is : ");
		for (uint32_t i = 0; i < len; i++)
		{
			printf("%x ", protoBufMessage[i]);
		}
		printf("\n");
		*protoBufMainId = htonl(6); //设定MainID SubID
		*protoBufSubId = htonl(6003);
		*protoBufLen = htonl(ostream.bytes_written);
		n = send(client_fd, sendBuf, PROTOBUF_MESSAGE_OFFSET + len, 0);
		memset(sendBuf, 0, sizeof(sendBuf));
	}
}
