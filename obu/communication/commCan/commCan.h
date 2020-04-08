#ifndef COMM_CAN_H
#define COMM_CAN_H

#ifdef COMM_CAN_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* COMM_CAN_C */
#include "../../TargetSettings.h"

struct GearSpeed  
{
	float VehicleSpeed; //整车车速
	float SOCValue; //电池电量
	uint32_t Gear; //挡位
	uint32_t MotorSpeed; //电机转速
	uint32_t MeterTotalMileage; //总里程
	uint32_t BackDoorOpenSignal; //后门
	uint32_t FrontDoorOpenSignal; //前门
	uint32_t TurnLeftInstruction; //左转向
	uint32_t FarLightInstruction; //远光灯
	uint32_t TurnRightInstruction; //右转向
};

PUBLIC void CANMessageUpdateProcess(void);
PUBLIC proto_CANMessage *getCANMessage(void);
PUBLIC struct GearSpeed *getGearSpeed(void);
PUBLIC uint64_t getCurrTime(void);

#undef PUBLIC

#endif /* COMM_CAN_H */

/* end of file */
