
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdlib.h>
#include "../proto/pb_common.h"
#include "../proto/pb_decode.h"
#include "../proto/pb_encode.h"
#include "../proto/pb.h"
#include "../proto/test.pb.h"
#include "../TargetSettings.h"
#include "../commNet/commNet.h"
#include "../commNet/commNetUdp.h"
#include "../commCan/commCan.h"
#include "../commUsart/commUsart.h"
#include "../commV2X/commV2X.h"
#include "../logic/position.h"

#define ITS_MANAGER_C
#include "itsManager.h"

#define V2X_SEND_BUF_SIZE 1024

static proto_RsuInfoRequest stRsuInfoRequest = {0};
static proto_position_data_t strCarLocationinfo = {0};
static proto_RoadBlockRequest stRoadBlockRequest = {0};
static proto_RoadBlockRequest stSpeedLimitRequest = {0};
static proto_ObuTrafficLightsRequest stObuTrafficLightsRequest = {0};
CurrVehicleInfo stCurrVehicleInfo = {0};
static uint8_t sendBuf[V2X_SEND_BUF_SIZE];
uint32_t stRsuID = 0;
SignalAndSensorInfo stRsuInfo = {0};
RoadBlockInfoMsg stRoadBlockInfo = {0};
SpeedLimitInfoMsg stSpeedLimitInfo = {0};
ObuToObuInfoMsg stObuToObuInfo = {0};

void GetGPSInfo()
{
	int i = 0, j = 0, count = 0;
	proto_position_data_t *ptrPositionData = NULL;
	ptrPositionData = carLocationGet();
	if (ptrPositionData == NULL)
	{
		printf("GPS idle\n");
		return;
	}
	stCurrVehicleInfo.flCurrVelocity = ptrPositionData->velocity;
	stCurrVehicleInfo.flCurrHeading = ptrPositionData->heading;

//#if CURR_GPS == CAR_GPS
    stCurrVehicleInfo.flCarLongitude = ptrPositionData->longitude;
    stCurrVehicleInfo.flCarLatitude = ptrPositionData->latitude;
//#endif

	char *uwbdata = get_usart_info();
	char NorthHemisphere = ' ';
	char EastHemisphere = ' ';
	char Latitude[10] = {0};
	char Longitude[10] = {0};
	if (uwbdata) {
		printf("----- uwb -----\n");
		for (i = 0; i < strlen(uwbdata) - 1; i++)
		{
			switch (uwbdata[i])
			{
			case ',':
				count++;
				j = 0;
				break;
			default:
				switch (count)
				{
				case 2:
					Latitude[j] = uwbdata[i];
					break;
				case 3:
					NorthHemisphere = uwbdata[i];
					break;
				case 4:
					Longitude[j] = uwbdata[i];
					break;
				case 5:
					EastHemisphere = uwbdata[i];
					break;
				}
				j++;
				break;
			}
		}
#if CURR_GPS == UWB_GPS
    	stCurrVehicleInfo.flCarLongitude = atof(Longitude);
    	stCurrVehicleInfo.flCarLatitude = atof(Latitude);
#endif
		if (NorthHemisphere == 'N')
		{
			stCurrVehicleInfo.NorthHemisphere = true;
		}
		else
		{
			stCurrVehicleInfo.NorthHemisphere = false;
		}
		if (EastHemisphere == 'E')
		{
			stCurrVehicleInfo.EastHemisphere = true;
		}
		else
		{
			stCurrVehicleInfo.EastHemisphere = false;
		}
	}
}

static uint8_t *v2xObuTxBufGet(uint16_t *bufLen, uint64_t time)
{
    bool rec_bool;
    pb_istream_t istream;
    pb_ostream_t ostream;
    proto_position_data_t position_data = {0};
    memset(sendBuf, 0, sizeof(sendBuf));
    *bufLen = 0;

	GetGPSInfo();

	position_data.latitude = stCurrVehicleInfo.flAutoDriveLatitude;
    position_data.longitude = stCurrVehicleInfo.flAutoDriveLongitude;
    position_data.heading = stCurrVehicleInfo.flCurrHeading;
    position_data.velocity = stCurrVehicleInfo.flCurrVelocity;
	position_data.CarId = CAR_ID;
	position_data.speed = stCurrVehicleInfo.flCurrSpeed;
	position_data.trace = stCurrVehicleInfo.flCurrTrace;
    printf("======================GPS INFO START==========================\n");
    printf("VechileLongitude = %0.2lf\n", stCurrVehicleInfo.flAutoDriveLongitude);
    printf("VechileLatitude = %0.2lf\n", stCurrVehicleInfo.flAutoDriveLatitude);
    printf("VechileVelocity = %0.2lf\n", stCurrVehicleInfo.flCurrVelocity);
    printf("VechileHeading = %0.2lf\n", stCurrVehicleInfo.flCurrHeading);
    printf("Timestamp = %" PRIu64 "\n", time);
    printf("======================GPS INFO END==========================\n");
    //数据流配置
    ostream = pb_ostream_from_buffer(sendBuf, V2X_SEND_BUF_SIZE);
    //将buf数据编码后写入流
    rec_bool = pbEncode(&ostream, proto_position_data_t_fields, &position_data);
    if (rec_bool == false)
    {
        printf("pbEncode failed!!!\n");
        return NULL;
    }
    //写入数据长度
    *bufLen = ostream.bytes_written;
    return sendBuf;
}

static void RsuInfoRecved(acme_message_r msg)
{
    bool rec_bool;
    pb_istream_t istream;
	proto_RsuInfoRequest RsuInfoRequest = {0};
    //数据流配置
    istream = pb_istream_from_buffer(msg.buf_ptr+12, msg.buf_len);
    //将buf数据解码后写入流
    rec_bool = pbDecode(&istream, proto_RsuInfoRequest_fields, &RsuInfoRequest);
    if (rec_bool == false)
    {
        printf("RSU to OBU pbDecode failed!!!\n");
        return;
    }
	stRsuID = RsuInfoRequest.RsuId;
	stRsuInfo.u32RsuId = RsuInfoRequest.RsuId;
	stRsuInfo.u8NorthSignalL = RsuInfoRequest.NorthSignalL;
	stRsuInfo.u8NorthDelayL = RsuInfoRequest.NorthDelayL;
	stRsuInfo.u8NorthSignalD = RsuInfoRequest.NorthSignalD;
	stRsuInfo.u8NorthDelayD = RsuInfoRequest.NorthDelayD;
	stRsuInfo.u8NorthSignalR = RsuInfoRequest.NorthSignalR;
	stRsuInfo.u8NorthDelayR = RsuInfoRequest.NorthDelayR;
	stRsuInfo.u8SouthSignalL = RsuInfoRequest.SouthSignalL;
	stRsuInfo.u8SouthDelayL = RsuInfoRequest.SouthDelayL;
	stRsuInfo.u8SouthSignalD = RsuInfoRequest.SouthSignalD;
	stRsuInfo.u8SouthDelayD = RsuInfoRequest.SouthDelayD;
	stRsuInfo.u8SouthSignalR = RsuInfoRequest.SouthSignalR;
	stRsuInfo.u8SouthDelayR = RsuInfoRequest.SouthDelayR;
	stRsuInfo.u8WestSignalL = RsuInfoRequest.WestSignalL;
	stRsuInfo.u8WestDelayL = RsuInfoRequest.WestDelayL;
	stRsuInfo.u8WestSignalD = RsuInfoRequest.WestSignalD;
	stRsuInfo.u8WestDelayD = RsuInfoRequest.WestDelayD;
	stRsuInfo.u8WestSignalR = RsuInfoRequest.WestSignalR;
	stRsuInfo.u8WestDelayR = RsuInfoRequest.WestDelayR;
	stRsuInfo.u8EastSignalL = RsuInfoRequest.EastSignalL;
	stRsuInfo.u8EastDelayL = RsuInfoRequest.EastDelayL;
	stRsuInfo.u8EastSignalD = RsuInfoRequest.EastSignalD;
	stRsuInfo.u8EastDelayD = RsuInfoRequest.EastDelayD;
	stRsuInfo.u8EastSignalR = RsuInfoRequest.EastSignalR;
	stRsuInfo.u8EastDelayR = RsuInfoRequest.EastDelayR;
	stRsuInfo.u32SensorRain = RsuInfoRequest.SensorRain;
	stRsuInfo.u32SensorWaterLogged = RsuInfoRequest.SensorWaterLogged;
	stRsuInfo.u32SensorVisibility = RsuInfoRequest.SensorVisibility;
    printf("==============Recv from RsuInfo! =================\n");
    printf("RsuId = %d\n", RsuInfoRequest.RsuId);
    printf("Timestamp = %" PRIu64 "\n", RsuInfoRequest.Timestamp);
    printf("NorthSignalL = %u\n", RsuInfoRequest.NorthSignalL);
    printf("NorthDelayL = %u\n", RsuInfoRequest.NorthDelayL);
    printf("NorthSignalD = %u\n", RsuInfoRequest.NorthSignalD);
    printf("NorthDelayD = %u\n", RsuInfoRequest.NorthDelayD);
    printf("NorthSignalR = %u\n", RsuInfoRequest.NorthSignalR);
    printf("NorthDelayR = %u\n", RsuInfoRequest.NorthDelayR);
    printf("SouthSignalL = %u\n", RsuInfoRequest.SouthSignalL);
    printf("SouthDelayL = %u\n", RsuInfoRequest.SouthDelayL);
    printf("SouthSignalD = %u\n", RsuInfoRequest.SouthSignalD);
    printf("SouthDelayD = %u\n", RsuInfoRequest.SouthDelayD);
    printf("SouthSignalR = %u\n", RsuInfoRequest.SouthSignalR);
    printf("SouthDelayR = %u\n", RsuInfoRequest.SouthDelayR);
    printf("WestSignalL = %u\n", RsuInfoRequest.WestSignalL);
    printf("WestDelayL = %u\n", RsuInfoRequest.WestDelayL);
    printf("WestSignalD = %u\n", RsuInfoRequest.WestSignalD);
    printf("WestDelayD = %u\n", RsuInfoRequest.WestDelayD);
    printf("WestSignalR = %u\n", RsuInfoRequest.WestSignalR);
    printf("WestDelayR = %u\n", RsuInfoRequest.WestDelayR);
    printf("EastSignalL = %u\n", RsuInfoRequest.EastSignalL);
    printf("EastDelayL = %u\n", RsuInfoRequest.EastDelayL);
    printf("EastSignalD = %u\n", RsuInfoRequest.EastSignalD);
    printf("EastDelayD = %u\n", RsuInfoRequest.EastDelayD);
    printf("EastSignalR = %u\n", RsuInfoRequest.EastSignalR);
    printf("EastSignalR = %u\n", RsuInfoRequest.EastSignalR);
    printf("EastDelayR = %u\n", RsuInfoRequest.EastDelayR);
    printf("SensorWaterLogged = %d\n", RsuInfoRequest.SensorWaterLogged);
    printf("SensorVisibility = %d\n", RsuInfoRequest.SensorVisibility);
    printf("SensorSensorRain = %d\n", RsuInfoRequest.SensorRain);
    printf("=================================================\n");
}

proto_RsuInfoRequest *rsuInfoGet(void)
{
	//proto_RsuInfoRequest *RsuInfo = NULL;
	//RsuInfo = &stRsuInfoRequest;
	memset(&stRsuInfoRequest, 0, sizeof(proto_RsuInfoRequest));

	stRsuInfoRequest.RsuId = stRsuInfo.u32RsuId;
	stRsuInfoRequest.NorthSignalL = stRsuInfo.u8NorthSignalL;
	stRsuInfoRequest.NorthDelayL = stRsuInfo.u8NorthDelayL;
	stRsuInfoRequest.NorthSignalD = stRsuInfo.u8NorthSignalD;
	stRsuInfoRequest.NorthDelayD = stRsuInfo.u8NorthDelayD;
	stRsuInfoRequest.NorthSignalR = stRsuInfo.u8NorthSignalR;
	stRsuInfoRequest.NorthDelayR = stRsuInfo.u8NorthDelayR;
	stRsuInfoRequest.SouthSignalL = stRsuInfo.u8SouthSignalL;
	stRsuInfoRequest.SouthDelayL = stRsuInfo.u8SouthDelayL;
	stRsuInfoRequest.SouthSignalD = stRsuInfo.u8SouthSignalD;
	stRsuInfoRequest.SouthDelayD = stRsuInfo.u8SouthDelayD;
	stRsuInfoRequest.SouthSignalR = stRsuInfo.u8SouthSignalR;
	stRsuInfoRequest.SouthDelayR = stRsuInfo.u8SouthDelayR;
	stRsuInfoRequest.WestSignalL = stRsuInfo.u8WestSignalL;
	stRsuInfoRequest.WestDelayL = stRsuInfo.u8WestDelayL;
	stRsuInfoRequest.WestSignalD = stRsuInfo.u8WestSignalD;
	stRsuInfoRequest.WestDelayD = stRsuInfo.u8WestDelayD;
	stRsuInfoRequest.WestSignalR = stRsuInfo.u8WestSignalR;
	stRsuInfoRequest.WestDelayR = stRsuInfo.u8WestDelayR;
	stRsuInfoRequest.EastSignalL = stRsuInfo.u8EastSignalL;
	stRsuInfoRequest.EastDelayL = stRsuInfo.u8EastDelayL;
	stRsuInfoRequest.EastSignalD = stRsuInfo.u8EastSignalD;
	stRsuInfoRequest.EastDelayD = stRsuInfo.u8EastDelayD;
	stRsuInfoRequest.EastSignalR = stRsuInfo.u8EastSignalR;
	stRsuInfoRequest.EastDelayR = stRsuInfo.u8EastDelayR;
	stRsuInfoRequest.SensorRain = stRsuInfo.u32SensorRain;
	stRsuInfoRequest.SensorWaterLogged = stRsuInfo.u32SensorWaterLogged;
	stRsuInfoRequest.SensorVisibility = stRsuInfo.u32SensorVisibility;

	memset(&stRsuInfo, 0, sizeof(SignalAndSensorInfo));
    return &stRsuInfoRequest;
}

static void ObuInfoRecved(acme_message_r msg)
{
    bool rec_bool;
    pb_istream_t istream;
	proto_position_data_t CarLocationinfo = {0};
    //数据流配置
    istream = pb_istream_from_buffer(msg.buf_ptr, msg.buf_len);
    //将buf数据解码后写入流
    rec_bool = pbDecode(&istream, proto_position_data_t_fields, &CarLocationinfo);
    if (rec_bool == false)
    {
        printf("OBU to OBU pbDecode failed!!!\n");
        return;
    }
	stObuToObuInfo.CarId = CarLocationinfo.CarId;
	stObuToObuInfo.latitude = CarLocationinfo.latitude;
	stObuToObuInfo.longitude = CarLocationinfo.longitude;
	stObuToObuInfo.heading = CarLocationinfo.heading;
	stObuToObuInfo.velocity = CarLocationinfo.velocity;
	stObuToObuInfo.speed = CarLocationinfo.speed;
	stObuToObuInfo.trace = CarLocationinfo.trace;
    printf("============Recv from OBU! ==============\n");
    printf("latitude = %f\n", CarLocationinfo.latitude);
    printf("longitude = %f\n", CarLocationinfo.longitude);
    printf("heading = %f\n", CarLocationinfo.heading);
    printf("velocity = %f\n", CarLocationinfo.velocity);
	printf("CarId = %d\n", CarLocationinfo.CarId);
	printf("speed = %f\n", CarLocationinfo.speed);
	printf("trace = %f\n", CarLocationinfo.trace);
    printf("=========================================\n");
    return;
}

proto_position_data_t *ObuToObuInfoGet()
{
	memset(&strCarLocationinfo, 0, sizeof(proto_position_data_t));
	strCarLocationinfo.CarId = stObuToObuInfo.CarId;
	strCarLocationinfo.latitude = stObuToObuInfo.latitude;
	strCarLocationinfo.longitude = stObuToObuInfo.longitude;
	strCarLocationinfo.heading = stObuToObuInfo.heading;
	strCarLocationinfo.velocity = stObuToObuInfo.velocity;
	strCarLocationinfo.speed = stObuToObuInfo.speed;
	strCarLocationinfo.trace = stObuToObuInfo.trace;
	memset(&stObuToObuInfo, 0, sizeof(ObuToObuInfoMsg));
    return &strCarLocationinfo;
}

static void RoadBlockInfoRecved(acme_message_r msg)
{
    bool rec_bool;
    pb_istream_t istream;
	proto_RoadBlockRequest RoadBlockRequest = {0};
    //数据流配置
    istream = pb_istream_from_buffer(msg.buf_ptr, msg.buf_len);
    //将buf数据解码后写入流
    rec_bool = pbDecode(&istream, proto_RoadBlockRequest_fields, &RoadBlockRequest);
    if (rec_bool == false)
    {
        printf("Road Block Info pbDecode failed!!!\n");
        return;
    }
	stRoadBlockInfo.RsuId = RoadBlockRequest.RsuId;
	stRoadBlockInfo.latitude = RoadBlockRequest.latitude;
	stRoadBlockInfo.longitude = RoadBlockRequest.longitude;
    printf("==========Recv from RoadBlockInfo! ============\n");
    printf("ObuId = %d\n", RoadBlockRequest.RsuId);
    printf("Timestamp = %" PRIu64 "\n", RoadBlockRequest.Timestamp);
    printf("SpeedLimit = %.02lf\n", RoadBlockRequest.SpeedLimit);
    printf("latitude = %.02lf\n", RoadBlockRequest.latitude);
    printf("longitude = %.02lf\n", RoadBlockRequest.longitude);
    printf("===============================================\n");
}

proto_RoadBlockRequest *RoadBlockInfoGet()
{
	memset(&stRoadBlockRequest, 0, sizeof(proto_RoadBlockRequest));
	stRoadBlockRequest.RsuId = stRoadBlockInfo.RsuId;
	stRoadBlockRequest.latitude = stRoadBlockInfo.latitude;
	stRoadBlockRequest.longitude = stRoadBlockInfo.longitude;
	memset(&stRoadBlockInfo, 0, sizeof(RoadBlockInfoMsg));
    return &stRoadBlockRequest;
}

static void SpeedlimitInfoRecved(acme_message_r msg)
{
    bool rec_bool;
    pb_istream_t istream;
	proto_RoadBlockRequest SpeedLimitRequest = {0};
    //数据流配置
    istream = pb_istream_from_buffer(msg.buf_ptr, msg.buf_len);
    //将buf数据解码后写入流
    rec_bool = pbDecode(&istream, proto_RoadBlockRequest_fields, &SpeedLimitRequest);
    if (rec_bool == false)
    {
        printf("peedlimitInfo pbDecode failed!!!\n");
        return;
    }
	stSpeedLimitInfo.RsuId = SpeedLimitRequest.RsuId;
	stSpeedLimitInfo.SpeedLimit = SpeedLimitRequest.SpeedLimit;
	stSpeedLimitInfo.latitude = SpeedLimitRequest.latitude;
	stSpeedLimitInfo.longitude = SpeedLimitRequest.longitude;
    printf("============Recv from SpeedlimitInfo! ==============\n");
    printf("ObuId = %d\n", SpeedLimitRequest.RsuId);
    printf("Timestamp = %" PRIu64 "\n", SpeedLimitRequest.Timestamp);
    printf("SpeedLimit = %.02lf\n", SpeedLimitRequest.SpeedLimit);
    printf("latitude = %.02lf\n", SpeedLimitRequest.latitude);
    printf("longitude = %.02lf\n", SpeedLimitRequest.longitude);
    printf("====================================================\n");
}

proto_RoadBlockRequest *SpeedLimitInfoGet()
{
	memset(&stSpeedLimitRequest, 0, sizeof(proto_RoadBlockRequest));
	stSpeedLimitRequest.RsuId = stSpeedLimitInfo.RsuId;
	stSpeedLimitRequest.SpeedLimit = stSpeedLimitInfo.SpeedLimit;
	stSpeedLimitRequest.latitude = stSpeedLimitInfo.latitude;
	stSpeedLimitRequest.longitude = stSpeedLimitInfo.longitude;
	memset(&stSpeedLimitInfo, 0, sizeof(SpeedLimitInfoMsg));
    return &stSpeedLimitRequest;
}

static void v2xMsgRecved(acme_message_r msg)
{
    printf("msg.v2x_family_id = %c\n",msg.v2x_family_id);
    switch (msg.v2x_family_id)
    {
    //RSU数据包接收
    case 'R':
        RsuInfoRecved(msg);
        break;
    //OBU对OBU的广播数据包接收
    case 'O':
        ObuInfoRecved(msg);
        break;
    //路障广播数据包接收
    case 'B':
        RoadBlockInfoRecved(msg);
        break;
    //限速牌广播数据包接收
    case 'S':
        SpeedlimitInfoRecved(msg);
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
	FuncName	getObuTrafficInfoToUpload
	function	获取proto_ObuTrafficLightsRequest结构体信息并返回地址
	return value	返回proto_ObuTrafficLightsRequest结构体指针
	EditHistory		2019-04-10 creat function buy wpz
-----------------------------------------------------------------------------*/
proto_ObuTrafficLightsRequest *ObuTrafficInfoGet(void)
{
    //proto_ObuTrafficLightsRequest *stRetPtr = NULL;
	memset(&stObuTrafficLightsRequest, 0, sizeof(stObuTrafficLightsRequest));
	stObuTrafficLightsRequest.TrafficId = stRsuID;
    stObuTrafficLightsRequest.VechicleHeading = stCurrVehicleInfo.flCurrHeading;
    stObuTrafficLightsRequest.VechicleSpeed = stCurrVehicleInfo.flCurrVelocity;
    stObuTrafficLightsRequest.VechicleLongitude = stCurrVehicleInfo.flAutoDriveLongitude;
    stObuTrafficLightsRequest.VechicleLatitude = stCurrVehicleInfo.flAutoDriveLatitude;
    stObuTrafficLightsRequest.VechicleId = CAR_ID;
    //stRetPtr = &stObuTrafficLightsRequest;
	stRsuID = 0;
    //return stRetPtr;
	return &stObuTrafficLightsRequest;
}

static void obuTxProcess(void)
{
    printf("obu to OBU!\n");
    gpsInfoUpdateSetup();
    v2xTxProcess(&v2xObuTxBufGet);
}

void itsProcessCreate(void)
{
    pthread_t threadidV2xTx = 0;
    pthread_t threadidNet = 0;
    pthread_t threadidUdpSend = 0;
    pthread_t threadidUdpRecv = 0;
    pthread_t threadidCan = 0;
    pthread_t threadidUwb = 0;

    v2xProcessInit();

    //注册回调函数
    v2xRxInit(&v2xMsgRecved);

    //新建OBU广播线程
    pthread_create(&threadidV2xTx, NULL, (void *)&obuTxProcess, NULL);
    pthread_detach(threadidV2xTx);

	//自动驾驶接收线程
	pthread_create(&threadidUdpRecv, NULL, (void *)&UdpRecvProcess, NULL);
    pthread_detach(threadidUdpRecv);

    //CAN车辆信息接收线程
    pthread_create(&threadidCan, NULL, (void *)&CANMessageUpdateProcess, NULL);
    pthread_detach(threadidCan);

    //UWB串口接收线程
    pthread_create(&threadidUwb, NULL, (void *)&UsartUpdateInfo, NULL);
    pthread_detach(threadidUwb);

	//自动驾驶/APP对接线程
	pthread_create(&threadidUdpSend, NULL, (void *)&UdpServerProcess, NULL);
	pthread_detach(threadidUdpSend);

    //CAN新建上传云服务端线程
	pthread_create(&threadidNet, NULL, (void *)&uploadCanUwbProcess, NULL);
	pthread_detach(threadidNet);

    //pthread_create(&threadidNet, NULL, (void *)&serverForAppProcess, NULL);
    //pthread_detach(threadidNet);

    while (1)
    {
        ;
    }
}

