#ifndef TARGET_SETTINGS_H
#define TARGET_SETTINGS_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "communication/proto/test.pb.h"

#define CAR_ID 1821

/*-----------------------------------------------------------------------------*/
/* target optiion                                                              */
/*-----------------------------------------------------------------------------*/
#define RSU_MODULE 1
#define OBU_MODULE 2
#define TEST_MODULE 3

#define CURR_MODULE OBU_MODULE

/*-----------------------------------------------------------------------------*/
/* server for app                                                              */
/*-----------------------------------------------------------------------------*/
// #define SERVER_FOR_APP_PORT (123)
#define SERVER_FOR_APP_PORT (19880)

/*-----------------------------------------------------------------------------*/
/* server to upload setup                                                      */
/*-----------------------------------------------------------------------------*/
#define PROTOBUF_SERVER_IP "47.111.188.230"
#define PROTOBUF_SERVER_PORT 8989

//#define PROTOBUF_SERVER_IP "144.34.213.81"
//#define PROTOBUF_SERVER_PORT 5000

/*-----------------------------------------------------------------------------*/
/* UWB setup                                                                   */
/*-----------------------------------------------------------------------------*/
#define UWB_ENALBE 0

#define CAR_GPS 1
#define UWB_GPS 2
#define AUTODRIVE_GPS 3
#define MAGNETICGRID_GPS 4

#define CURR_GPS AUTODRIVE_GPS

typedef struct _CurrVehicleInfo
{
	bool NorthHemisphere;
	bool EastHemisphere;
	float flCurrVelocity;
	float flCurrHeading;
    float flCarLongitude;
    float flCarLatitude;
    float flAutoDriveLongitude;
    float flAutoDriveLatitude;
    float flMagneticLongitude;
    float flMagneticLatitude;
	float flCurrSpeed;
	float flCurrTrace;
} CurrVehicleInfo;

//
//发送给自动驾驶和APP的数据
//
typedef struct _CurrInfoForUdp
{
	uint32_t u32RsuId;             /* 前方红绿灯ID (红绿灯位置需从后台服务器获取) */
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
    uint32_t u32SensorRain;        /* 雨量传感器 */
    uint32_t u32SensorWaterLogged; /* 积水传感器 */
    uint32_t u32SensorVisibility;  /* 能见度传感器 */

	uint32_t u32SpeedLimitId;       /* 限速牌ID */
    uint32_t u32SpeedLimit;         /* 限速值 */
    float flSpeedLimitLatitude;     /* 限速牌纬度 */
    float flSpeedLimitLongitude;    /* 限速牌经度 */

    uint32_t u32RoadblockId;        /* 路障ID */
    float flRoadblockLatitude;      /* 路障纬度 */
    float flRoadblockLongitude;     /* 路障经度 */

	float flCarDirection; /* 当前方向 */
    float flCarLongitude;  /* 当前位置-经度 */
    float flCarLatitude;   /* 当前位置-纬度 */
	float flCarTrace;     /* 前方转向 */
	float flCarSpeed;    /* 当前车速(最高255km/h) */
	float flCarSOCValue; /* 电池电量 */
	uint32_t u32CarGear; /* 挡位 */
	uint32_t u32CarMotorSpeed; /* 电机转速 */
	uint32_t u32CarMeterTotalMileage; /* 总里程 */
	uint32_t u32CarBackDoorOpenSignal; /* 后门 */
	uint32_t u32CarFrontDoorOpenSignal; /* 前门 */
	uint32_t u32CarTurnLeftInstruction; /* 左转向 */
	uint32_t u32CarFarLightInstruction; /* 远光灯 */
	uint32_t u32CarTurnRightInstruction; /* 右转向 */
    float flAutoDriveLongitude;  /* 自动驾驶经度 */
    float flAutoDriveLatitude;   /* 自动驾驶纬度 */
    float flMagneticLongitude;  /* 磁栅经度 */
    float flMagneticLatitude;   /* 磁栅纬度 */
    uint32_t u32ObuCarId;  /* 附近车辆ID */
    float flObuLatitude;  /* 附近车辆纬度 */
    float flObuLongitude;  /* 附近车辆经度 */
    float flObuHeading;  /* 附近车辆方向 */
    float flObuSpeed;  /* 附近车辆车速 */
    float flObuTrace;  /* 附近车辆转向 */
} CurrInfoForUdp;

typedef struct _SignalAndSensorInfo
{
	uint32_t u32RsuId;             /* 前方红绿灯ID (红绿灯位置需从后台服务器获取) */
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
    uint32_t u32SensorRain;        /* 雨量传感器 */
    uint32_t u32SensorWaterLogged; /* 积水传感器 */
    uint32_t u32SensorVisibility;  /* 能见度传感器 */
} SignalAndSensorInfo;

typedef struct _RoadBlockInfoMsg {
    uint32_t RsuId;
    float latitude;
    float longitude;
} RoadBlockInfoMsg;

typedef struct _SpeedLimitInfoMsg {
    uint32_t RsuId;
    float SpeedLimit;
    float latitude;
    float longitude;
} SpeedLimitInfoMsg;

typedef struct _ObuToObuInfoMsg {
    uint32_t CarId;
    float latitude;
    float longitude;
    float heading;
    float velocity;
    float speed;
    float trace;
} ObuToObuInfoMsg;

#define VCU1_ID  0x0C03A1A7
#define VCU2_ID  0x0C04A1A7
#define VCU3_ID  0x0C06A1A7
#define VCU4_ID  0x0C0AA1A7
#define METER1_ID  0x0C19A7A1
#define METER2_ID  0x0C1AA7A1
#define AIRCONDITION_ID 0x0C08A7F4
#define BMS1_ID 0x1818D0F3
#define BMS2_ID 0x1819D0F3
#define BMS3_ID 0x181AD0F3
#define BMS4_ID 0x181BD0F3
#define BMS5_ID 0x181CD0F3
#define BMS6_ID 0x181DD0F3 
#define BMS7_ID 0x181ED0F3 
#define BMS8_ID 0x180028F3
#define BMS9_ID 0x180228F3
#define BMS10_ID 0x180528F3
#define BMS11_ID 0x180028F4
#define BMS12_ID 0x180128F4
#define BMS13_ID 0x180228F4
#define BMS14_ID 0x180328F4
#define BMS15_ID 0x0CFFF3A7
#define OILPUMP1_ID 0x18F602A0
#define OILPUMP2_ID 0x0C0A88EF
#define MAGNETICGRID_ID 0x180328F6
/*
#define VCU_STATE_ID 0x18F101EF
#define VCU_CONTROL_ID 0x18F103EF
#define MOTOR_STATE1_ID 0x18F105EF
#define MOTOR_STATE2_ID 0x18F106EF
#define VCU_FAULT1_ID 0x18F107EF
#define VCU_FAULT2_ID 0x18F108EF
#define VCU_FAULTCODE_ID 0x18FECAEF
#define BATTERY_STATE1_ID 0x18F114F4
#define BATTERY_STATE2_ID 0x18F115F4
#define BATTERY_FAULT_ID 0x18F116F4
#define MAXPERMISS_CURRENT_ID 0x18F117F4
#define CELL_VOLTBOX_ID 0x18F118F4
#define CELL_TEMPBOX_ID 0x18F119F4
#define CELL_VOLT1_ID 0x18F11AF4
#define CELL_VOLT2_ID 0x18F11BF4
#define CELL_VOLT3_ID 0x18F11CF4
#define CELL_VOLT4_ID 0x18F11DF4
#define CELL_TEMP_ID 0x18F11EF4
#define CHARGING_ID 0x18F11FF4
#define INSULATE_RESISTER_ID 0x18F121E5
#define AIR_CONDITION_ID 0x18F5229E
#define METER1_ID 0x18FEF117
#define METER2_ID 0x18FEF217
*/
#endif
