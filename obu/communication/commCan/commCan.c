
#include <stdbool.h>
#include <stdint.h>
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
#include <sys/time.h>
#include <time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "../proto/pb_common.h"
#include "../proto/pb_decode.h"
#include "../proto/pb_encode.h"
#include "../proto/pb.h"
#include "../proto/test.pb.h"

#include "../../TargetSettings.h"
#include "../commNet/commNetUdp.h"

#define COMM_CAN_C
#include "commCan.h"

uint32_t SignalLeft = 0, SignalRight = 0;

static proto_CANMessage CANMessage;

extern CurrVehicleInfo stCurrVehicleInfo;

uint64_t getCurrTime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

void Can2Vcu1Msg(uint32_t Id, uint32_t time, proto_Vcu1 *Vcu1Msg, u_int8_t CanReciveBuf[])
{
	Vcu1Msg->CarId = Id;
	Vcu1Msg->Timestamp = time;
	Vcu1Msg->MotorControlInputVoltage = (CanReciveBuf[1] << 8 | CanReciveBuf[0]) * 0.1 - 10000;
	//printf("Vcu1Msg.MotorControlInputVoltage = %f\n", Vcu1Msg->MotorControlInputVoltage);
	Vcu1Msg->MotorControlInputCurrent = (CanReciveBuf[3] << 8 | CanReciveBuf[2]) * 0.1 - 10000;
	//printf("Vcu1Msg.MotorControlInputCurrent = %f\n", Vcu1Msg->MotorControlInputCurrent);
	Vcu1Msg->MotorSpeed = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 1 - 32000;
	//printf("Vcu1Msg.MotorSpeed = %d\n", Vcu1Msg->MotorSpeed);
	Vcu1Msg->MotorTorque = (CanReciveBuf[7] << 8 | CanReciveBuf[6]) * 0.5;
	//printf("Vcu1Msg.MotorTorque = %d\n", Vcu1Msg->MotorTorque);
}

void Can2Vcu2Msg(uint32_t Id, uint32_t time, proto_Vcu2 *Vcu2Msg, u_int8_t CanReciveBuf[])
{
	Vcu2Msg->CarId = Id;
	Vcu2Msg->Timestamp = time;
	Vcu2Msg->MotorTemperatur = CanReciveBuf[0] - 40;
	//printf("Vcu2Msg->MotorTemperatur = %d\n", Vcu2Msg->MotorTemperatur);
	Vcu2Msg->MotorControlTemperature = CanReciveBuf[1] - 40;
	//printf("Vcu2Msg->MotorControlTemperature = %d\n", Vcu2Msg->MotorControlTemperature);
	Vcu2Msg->HighVoltageSwitch = CanReciveBuf[5] >> 7 & 0x01;
	//printf("Vcu2Msg->HighVoltageSwitch = %d\n", Vcu2Msg->HighVoltageSwitch);
	Vcu2Msg->PowerMode = CanReciveBuf[5] >> 5 & 0x03;
	//printf("Vcu2Msg->PowerMode = %d\n", Vcu2Msg->PowerMode);
	Vcu2Msg->ClimbMode = CanReciveBuf[5] >> 4 & 0x01;
	//printf("Vcu2Msg->ClimbMode = %d\n", Vcu2Msg->ClimbMode);
	Vcu2Msg->Gear = CanReciveBuf[5] & 0x0f;
	//printf("Vcu2Msg->Gear = %d\n", Vcu2Msg->Gear);
	Vcu2Msg->EffectiveElectricBrak = CanReciveBuf[6] >> 7 & 0x01;
	//printf("Vcu2Msg->EffectiveElectricBrak = %d\n", Vcu2Msg->EffectiveElectricBrak);
	Vcu2Msg->ChargDetectionSignal = CanReciveBuf[6] >> 5 & 0x01;
	//printf("Vcu2Msg->ChargDetectionSignal = %d\n", Vcu2Msg->ChargDetectionSignal);
	Vcu2Msg->AirConditionerSignal = CanReciveBuf[6] >> 4 & 0x01;
	//printf("Vcu2Msg->AirConditionerSignal = %d\n", Vcu2Msg->AirConditionerSignal);
	Vcu2Msg->PrechargeContactor = CanReciveBuf[6] >> 3 & 0x01;
	//printf("Vcu2Msg->PrechargeContactor = %d\n", Vcu2Msg->PrechargeContactor);
	Vcu2Msg->MasterContactor = CanReciveBuf[6] >> 2 & 0x01;
	//printf("Vcu2Msg->MasterContactor = %d\n", Vcu2Msg->MasterContactor);
	Vcu2Msg->SystemDemo = CanReciveBuf[8] << 8 | CanReciveBuf[7];
	//printf("Vcu2Msg->SystemDemo = %d\n", Vcu2Msg->SystemDemo);
}

void Can2Vcu3Msg(uint32_t Id, uint32_t time, proto_Vcu3 *Vcu3Msg, u_int8_t CanReciveBuf[])
{
	Vcu3Msg->CarId = Id;
	Vcu3Msg->Timestamp = time;
	Vcu3Msg->TractionPedalPercent = CanReciveBuf[0] * 0.4;
	//printf("Vcu3Msg->TractionPedalPercent = %d\n", Vcu3Msg->TractionPedalPercent);
	Vcu3Msg->BrakePedalPercent = CanReciveBuf[1] * 0.4;
	//printf("Vcu3Msg->BrakePedalPercent = %d\n", Vcu3Msg->BrakePedalPercent);
}

void Can2Vcu4Msg(uint32_t Id, uint32_t time, proto_Vcu4 *Vcu4Msg, u_int8_t CanReciveBuf[])
{
	Vcu4Msg->CarId = Id;
	Vcu4Msg->Timestamp = time;
	Vcu4Msg->VehicleModel = CanReciveBuf[0];
	//printf("Vcu4Msg->VehicleModel = %d\n", Vcu4Msg->VehicleModel);
	Vcu4Msg->VehicleSystem = CanReciveBuf[1] >> 4 & 0x03;
	//printf("Vcu4Msg->VehicleSystem = %d\n", Vcu4Msg->VehicleSystem);
	Vcu4Msg->ElectricalMachinery = CanReciveBuf[1] & 0x03;
	//printf("Vcu4Msg->ElectricalMachinery = %d\n", Vcu4Msg->ElectricalMachinery);
}

void Can2Meter1Msg(uint32_t Id, uint32_t time, proto_Meter1 *Meter1Msg, u_int8_t CanReciveBuf[])
{
	Meter1Msg->CarId = Id;
	Meter1Msg->Timestamp = time;
	Meter1Msg->KeyFirstGearSignal = CanReciveBuf[0] >> 6 & 0x01;
	//printf("Meter1Msg->KeyFirstGearSignal = %d\n", Meter1Msg->KeyFirstGearSignal);
	Meter1Msg->KeyTwoGearSignal = CanReciveBuf[0] >> 5 & 0x01;
	//printf("Meter1Msg->KeyTwoGearSignal = %d\n", Meter1Msg->KeyTwoGearSignal);
	Meter1Msg->BackDoorOpenSignal = CanReciveBuf[0] >> 4 & 0x01;
	//printf("Meter1Msg->BackDoorOpenSignal = %d\n", Meter1Msg->BackDoorOpenSignal);
	Meter1Msg->FrontDoorOpenSignal = CanReciveBuf[0] >> 3 & 0x01;
	//printf("Meter1Msg->FrontDoorOpenSignal = %d\n", Meter1Msg->FrontDoorOpenSignal);
	Meter1Msg->RearCompartmentDoorOpenSignal = CanReciveBuf[0] >> 2 & 0x01;
	//printf("Meter1Msg->RearCompartmentDoorOpenSignal = %d\n", Meter1Msg->RearCompartmentDoorOpenSignal);
	Meter1Msg->ParkingSignal = CanReciveBuf[0] >> 1 & 0x01;
	//printf("Meter1Msg->ParkingSignal = %d\n", Meter1Msg->ParkingSignal);
	Meter1Msg->VehicleLowPressureAlarm = CanReciveBuf[0] & 0x01;
	//printf("Meter1Msg->VehicleLowPressureAlarm = %d\n", Meter1Msg->VehicleLowPressureAlarm);

	if(SignalLeft || (CanReciveBuf[1] >> 7 & 0x01)) {
		Meter1Msg->TurnLeftInstruction = 1;
	} else {
		Meter1Msg->TurnLeftInstruction = 0;
	}
	SignalLeft = CanReciveBuf[1] >> 7 & 0x01;
	//Meter1Msg->TurnLeftInstruction = CanReciveBuf[1] >> 7 & 0x01;
	//printf("Meter1Msg->TurnLeftInstruction = %d\n", Meter1Msg->TurnLeftInstruction);

	Meter1Msg->FarLightInstruction = CanReciveBuf[1] >> 6 & 0x01;
	//printf("Meter1Msg->FarLightInstruction = %d\n", Meter1Msg->FarLightInstruction);
	Meter1Msg->FrontMistInstruction = CanReciveBuf[1] >> 5 & 0x01;
	//printf("Meter1Msg->FrontMistInstruction = %d\n", Meter1Msg->FrontMistInstruction);

	if(SignalRight || (CanReciveBuf[1] >> 4 & 0x01)) {
		Meter1Msg->TurnRightInstruction = 1;
	} else {
		Meter1Msg->TurnRightInstruction = 0;
	}
	SignalRight = CanReciveBuf[1] >> 4 & 0x01;
	//Meter1Msg->TurnRightInstruction = CanReciveBuf[1] >> 4 & 0x01;
	//printf("Meter1Msg->TurnRightInstruction = %d\n", Meter1Msg->TurnRightInstruction);

	Meter1Msg->OilMassAlarm = CanReciveBuf[1] >> 3 & 0x01;
	//printf("Meter1Msg->OilMassAlarm = %d\n", Meter1Msg->OilMassAlarm);
	Meter1Msg->EnginePreheat = CanReciveBuf[1] >> 2 & 0x01;
	//printf("Meter1Msg->EnginePreheat = %d\n", Meter1Msg->EnginePreheat);
	Meter1Msg->NearLightInstruction = CanReciveBuf[1] >> 1 & 0x01;
	//printf("Meter1Msg->NearLightInstruction = %d\n", Meter1Msg->NearLightInstruction);
	Meter1Msg->BackMistInstruction = CanReciveBuf[1] & 0x01;
	//printf("Meter1Msg->BackMistInstruction = %d\n", Meter1Msg->BackMistInstruction);

	Meter1Msg->VehicleSpeed = CanReciveBuf[3] * 0.5;
	stCurrVehicleInfo.flCurrSpeed = CanReciveBuf[3] * 0.5;
	//printf("Meter1Msg->VehicleSpeed = %f\n", Meter1Msg->VehicleSpeed);
	Meter1Msg->AirPressureOne = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.1;
	//printf("Meter1Msg->AirPressureOne = %f\n", Meter1Msg->AirPressureOne);
	Meter1Msg->AirPressureTwo = (CanReciveBuf[7] << 8 | CanReciveBuf[6]) * 0.1;
	//printf("Meter1Msg->AirPressureTwo = %f\n", Meter1Msg->AirPressureTwo);
}

void Can2Meter2Msg(uint32_t Id, uint32_t time, proto_Meter2 *Meter2Msg, u_int8_t CanReciveBuf[])
{
	Meter2Msg->CarId = Id;
	Meter2Msg->Timestamp = time;
	Meter2Msg->MeterTotalMileage = (CanReciveBuf[3] << 8 | CanReciveBuf[2] << 8 | CanReciveBuf[1] << 8 | CanReciveBuf[0]) * 0.1;
	//printf("Meter2Msg->MeterTotalMileage = %f\n", Meter2Msg->MeterTotalMileage);
	Meter2Msg->MeterPartMileage = (CanReciveBuf[7] << 8 | CanReciveBuf[6] << 8 | CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.1;
	//printf("Meter2Msg->MeterPartMileage = %f\n", Meter2Msg->MeterPartMileage);
}

void Can2AirConditionMsg(uint32_t Id, uint32_t time, proto_AirCondition *AirConditionMsg, u_int8_t CanReciveBuf[])
{
	AirConditionMsg->CarId = Id;
	AirConditionMsg->Timestamp = time;
	AirConditionMsg->ActualAirConditioningLoad = CanReciveBuf[0] * 0.4;
	//printf("AirConditionMsg->ActualAirConditioningLoad = %f\n", AirConditionMsg->ActualAirConditioningLoad);
	AirConditionMsg->SetTemperature = CanReciveBuf[1] - 40;
	//printf("AirConditionMsg->SetTemperature = %d\n", AirConditionMsg->SetTemperature);
	AirConditionMsg->CarOutsideTemperature = CanReciveBuf[2] - 40;
	//printf("AirConditionMsg->CarOutsideTemperature = %d\n", AirConditionMsg->CarOutsideTemperature);
	AirConditionMsg->CarInsideTemperature = CanReciveBuf[3] - 40;
	//printf("AirConditionMsg->CarInsideTemperature = %d\n", AirConditionMsg->CarInsideTemperature);
	AirConditionMsg->DirectCurrentelEctricity = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.1 - 10000;
	//printf("AirConditionMsg->DirectCurrentelEctricity = %f\n", AirConditionMsg->DirectCurrentelEctricity);
	AirConditionMsg->AirConditionerStatusReady = CanReciveBuf[6] >> 7 & 0x01;
	//printf("AirConditionMsg->AirConditionerStatusReady = %d\n", AirConditionMsg->AirConditionerStatusReady);
	AirConditionMsg->AirConditionerStatusBreakdown = CanReciveBuf[6] >> 6 & 0x01;
	//printf("AirConditionMsg->AirConditionerStatusBreakdown = %d\n", AirConditionMsg->AirConditionerStatusBreakdown);
	AirConditionMsg->AirConditionerStatusStart = CanReciveBuf[6] & 0x01;
	//printf("AirConditionMsg->AirConditionerStatusStart = %d\n", AirConditionMsg->AirConditionerStatusStart);
	AirConditionMsg->LifeValue = CanReciveBuf[7];
	//printf("AirConditionMsg->LifeValue = %d\n", AirConditionMsg->LifeValue);
}

void Can2Bms1Msg(uint32_t Id, uint32_t time, proto_Bms1 *Bms1Msg, u_int8_t CanReciveBuf[])
{

	// Bms1Msg->CarId = Id;
	// Bms1Msg->Timestamp = time;
	// Bms1Msg->TotalBatteryVoltage = 10.9;
	// Bms1Msg->RechargeDischargeElectricity = 10.9;
	// Bms1Msg->SOCValue = 10.9;
	// Bms1Msg->PressureBig = 10;
	// Bms1Msg->TotalOvervoltage = 10;
	// Bms1Msg->TotalUndervoltage = 10;
	// Bms1Msg->InsideCommunicationFail = 10;
	// Bms1Msg->SOCLow = 10;
	// Bms1Msg->SOCHigt = 10;
	// Bms1Msg->MonocaseUndervoltage = 10;
	// Bms1Msg->MonocaseOvervoltage = 10;
	// Bms1Msg->TemperatureOverSize = 10;
	// Bms1Msg->TemperatureRiseOversize = 10;
	// Bms1Msg->TemperatureOverTop = 10;
	// Bms1Msg->RechargeElectricityOverSize = 10;
	// Bms1Msg->DischargeElectricityOverSize = 10;
	// Bms1Msg->InsulationAlarm = 10;
	// Bms1Msg->SmogAlarm = 10;
	// Bms1Msg->CellTemperatureOverSize = 10;
	// Bms1Msg->FuseStatus = 10;
	// Bms1Msg->CurrentRechargeStatus = 10;
	// Bms1Msg->RechargeConnectSureSignal = 10;
	// Bms1Msg->CommunicationWithBatteryCharger = 10;
	// Bms1Msg->HaveCapacitySmallCell = 10;
	// Bms1Msg->HaveResistanceBigCell = 10;
	// Bms1Msg->InsideContactorSignal = 10;
	// Bms1Msg->RadiatSystemRunning = 10;

	Bms1Msg->CarId = Id;
	Bms1Msg->Timestamp = time;
	Bms1Msg->TotalBatteryVoltage = (CanReciveBuf[1] << 8 | CanReciveBuf[0]) * 0.1 - 10000;
	//printf("Bms1Msg->TotalBatteryVoltage = %f\n", Bms1Msg->TotalBatteryVoltage);
	Bms1Msg->RechargeDischargeElectricity = (CanReciveBuf[3] << 8 | CanReciveBuf[2]) * 0.1 - 10000;
	//printf("Bms1Msg->RechargeDischargeElectricity = %f\n", Bms1Msg->RechargeDischargeElectricity);
	Bms1Msg->SOCValue = CanReciveBuf[4] * 0.4;
	//printf("Bms1Msg->SOCValue = %f\n", Bms1Msg->SOCValue);

	Bms1Msg->PressureBig = CanReciveBuf[5] >> 7 & 0x01;
	//printf("Bms1Msg->PressureBig = %d\n", Bms1Msg->PressureBig);
	Bms1Msg->TotalOvervoltage = CanReciveBuf[5] >> 6 & 0x01;
	//printf("Bms1Msg->TotalOvervoltage = %d\n", Bms1Msg->TotalOvervoltage);
	Bms1Msg->TotalUndervoltage = CanReciveBuf[5] >> 5 & 0x01;
	//printf("Bms1Msg->TotalUndervoltage = %d\n", Bms1Msg->TotalUndervoltage);
	Bms1Msg->InsideCommunicationFail = CanReciveBuf[5] >> 4 & 0x01;
	//printf("Bms1Msg->InsideCommunicationFail = %d\n", Bms1Msg->InsideCommunicationFail);
	Bms1Msg->SOCLow = CanReciveBuf[5] >> 3 & 0x01;
	//printf("Bms1Msg->SOCLow = %d\n", Bms1Msg->SOCLow);
	Bms1Msg->SOCHigt = CanReciveBuf[5] >> 2 & 0x01;
	//printf("Bms1Msg->SOCHigt = %d\n", Bms1Msg->SOCHigt);
	Bms1Msg->MonocaseUndervoltage = CanReciveBuf[5] >> 1 & 0x01;
	//printf("Bms1Msg->MonocaseUndervoltage = %d\n", Bms1Msg->MonocaseUndervoltage);
	Bms1Msg->MonocaseOvervoltage = CanReciveBuf[5] & 0x01;
	//printf("Bms1Msg->MonocaseOvervoltage = %d\n", Bms1Msg->MonocaseOvervoltage);

	Bms1Msg->TemperatureOverSize = CanReciveBuf[6] >> 7 & 0x01;
	//printf("Bms1Msg->TemperatureOverSize = %d\n", Bms1Msg->TemperatureOverSize);
	Bms1Msg->TemperatureRiseOversize = CanReciveBuf[6] >> 6 & 0x01;
	//printf("Bms1Msg->TemperatureRiseOversize = %d\n", Bms1Msg->TemperatureRiseOversize);
	Bms1Msg->TemperatureOverTop = CanReciveBuf[6] >> 5 & 0x01;
	//printf("Bms1Msg->TemperatureOverTop = %d\n", Bms1Msg->TemperatureOverTop);
	Bms1Msg->RechargeElectricityOverSize = CanReciveBuf[6] >> 4 & 0x01;
	//printf("Bms1Msg->RechargeElectricityOverSize = %d\n", Bms1Msg->RechargeElectricityOverSize);
	Bms1Msg->DischargeElectricityOverSize = CanReciveBuf[6] >> 3 & 0x01;
	//printf("Bms1Msg->DischargeElectricityOverSize = %d\n", Bms1Msg->DischargeElectricityOverSize);
	Bms1Msg->InsulationAlarm = CanReciveBuf[6] >> 2 & 0x01;
	//printf("Bms1Msg->InsulationAlarm = %d\n", Bms1Msg->InsulationAlarm);
	Bms1Msg->SmogAlarm = CanReciveBuf[6] >> 1 & 0x01;
	//printf("Bms1Msg->SmogAlarm = %d\n", Bms1Msg->SmogAlarm);
	Bms1Msg->CellTemperatureOverSize = CanReciveBuf[6] & 0x01;
	//printf("Bms1Msg->CellTemperatureOverSize = %d\n", Bms1Msg->CellTemperatureOverSize);

	Bms1Msg->FuseStatus = CanReciveBuf[7] >> 7 & 0x01;
	//printf("Bms1Msg->FuseStatus = %d\n", Bms1Msg->FuseStatus);
	Bms1Msg->CurrentRechargeStatus = CanReciveBuf[7] >> 6 & 0x01;
	//printf("Bms1Msg->CurrentRechargeStatus = %d\n", Bms1Msg->CurrentRechargeStatus);
	Bms1Msg->RechargeConnectSureSignal = CanReciveBuf[7] >> 5 & 0x01;
	//printf("Bms1Msg->RechargeConnectSureSignal = %d\n", Bms1Msg->RechargeConnectSureSignal);
	Bms1Msg->CommunicationWithBatteryCharger = CanReciveBuf[7] >> 4 & 0x01;
	//printf("Bms1Msg->CommunicationWithBatteryCharger = %d\n", Bms1Msg->CommunicationWithBatteryCharger);
	Bms1Msg->HaveCapacitySmallCell = CanReciveBuf[7] >> 3 & 0x01;
	//printf("Bms1Msg->HaveCapacitySmallCell = %d\n", Bms1Msg->HaveCapacitySmallCell);
	Bms1Msg->HaveResistanceBigCell = CanReciveBuf[7] >> 2 & 0x01;
	//printf("Bms1Msg->HaveResistanceBigCell = %d\n", Bms1Msg->HaveResistanceBigCell);
	Bms1Msg->InsideContactorSignal = CanReciveBuf[7] >> 1 & 0x01;
	//printf("Bms1Msg->InsideContactorSignal= %d\n", Bms1Msg->InsideContactorSignal);
	Bms1Msg->RadiatSystemRunning = CanReciveBuf[7] & 0x01;
	//printf("Bms1Msg->RadiatSystemRunning = %d\n", Bms1Msg->RadiatSystemRunning);
}

void Can2Bms2Msg(uint32_t Id, uint32_t time, proto_Bms2 *Bms2Msg, u_int8_t CanReciveBuf[])
{
	Bms2Msg->CarId = Id;
	Bms2Msg->Timestamp = time;
	Bms2Msg->CurrentMaxAllowRechargeElectricity = (CanReciveBuf[1] << 8 | CanReciveBuf[0]) * 0.1 - 10000;
	//printf("Bms2Msg->CurrentMaxAllowRechargeElectricity = %f\n", Bms2Msg->CurrentMaxAllowRechargeElectricity);
	Bms2Msg->CurrentMaxAllowDischargeElectricity = (CanReciveBuf[3] << 8 | CanReciveBuf[2]) * 0.1 - 10000;
	//printf("Bms2Msg->CurrentMaxAllowDischargeElectricity = %f\n", Bms2Msg->CurrentMaxAllowDischargeElectricity);
	Bms2Msg->LiPoSystemBreakdownClass = CanReciveBuf[4];
	//printf("Bms2Msg->LiPoSystemBreakdownClass = %d\n", Bms2Msg->LiPoSystemBreakdownClass);
	Bms2Msg->MonomerMeanVoltage = (CanReciveBuf[6] << 8 | CanReciveBuf[5]) * 0.01 - 10000;
	//printf("Bms2Msg->MonomerMeanVoltage = %d\n", Bms2Msg->MonomerMeanVoltage);
	Bms2Msg->MonomerMaxTemperature = CanReciveBuf[7] - 40;
	//printf("Bms2Msg->MonomerMaxTemperature = %d\n", Bms2Msg->MonomerMaxTemperature);
}

void Can2Bms3Msg(uint32_t Id, uint32_t time, proto_Bms3 *Bms3Msg, u_int8_t CanReciveBuf[])
{
	Bms3Msg->CarId = Id;
	Bms3Msg->Timestamp = time;
	Bms3Msg->PositiveInsulatingResistance = CanReciveBuf[1] << 8 | CanReciveBuf[0];
	//printf("Bms3Msg->PositiveInsulatingResistance = %d\n", Bms3Msg->PositiveInsulatingResistance);
	Bms3Msg->NegativeInsulatingResistance = CanReciveBuf[3] << 8 | CanReciveBuf[2];
	//printf("Bms3Msg->NegativeInsulatingResistance = %d\n", Bms3Msg->NegativeInsulatingResistance);
	Bms3Msg->CellVoltageMaxValue = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.01 - 10000;
	//printf("Bms3Msg->CellVoltageMaxValue = %f\n", Bms3Msg->CellVoltageMaxValue);
	Bms3Msg->CellVoltageMinValue = (CanReciveBuf[7] << 8 | CanReciveBuf[6]) * 0.01 - 10000;
	//printf("Bms3Msg->CellVoltageMinValue = %f\n", Bms3Msg->CellVoltageMinValue);
}

void Can2Bms4Msg(uint32_t Id, uint32_t time, proto_Bms4 *Bms4Msg, u_int8_t CanReciveBuf[])
{
	Bms4Msg->CarId = Id;
	Bms4Msg->Timestamp = time;
	Bms4Msg->VINAcceptStatus = CanReciveBuf[0] & 0x03;
	//printf("Bms4Msg->VINAcceptStatus = %d\n", Bms4Msg->VINAcceptStatus);
	Bms4Msg->AssistContactorGlue = CanReciveBuf[1] >> 7 & 0x01;
	//printf("Bms4Msg->AssistContactorGlue = %d\n", Bms4Msg->AssistContactorGlue);
	Bms4Msg->ChargingContactorStatus = CanReciveBuf[1] >> 6 & 0x01;
	//printf("Bms4Msg->ChargingContactorStatus = %d\n", Bms4Msg->ChargingContactorStatus);
	Bms4Msg->CellCathodeContactorGlue = CanReciveBuf[1] >> 5 & 0x01;
	//printf("Bms4Msg->CellCathodeContactorGlue = %d\n", Bms4Msg->CellCathodeContactorGlue);
	Bms4Msg->CellAnodeContactorGlue = CanReciveBuf[1] >> 4 & 0x01;
	//printf("Bms4Msg->CellAnodeContactorGlue = %d\n", Bms4Msg->CellAnodeContactorGlue);
	Bms4Msg->MSDStatus = CanReciveBuf[1] >> 3 & 0x01;
	//printf("Bms4Msg->MSDStatus = %d\n", Bms4Msg->MSDStatus);
	Bms4Msg->WirelessRechargeConnectSignal = CanReciveBuf[1] >> 2 & 0x01;
	//printf("Bms4Msg->WirelessRechargeConnectSignal = %d\n", Bms4Msg->WirelessRechargeConnectSignal);
	Bms4Msg->ChargingStationOverTemperatureTemperature = CanReciveBuf[1] >> 1 & 0x01;
	//printf("Bms4Msg->ChargingStationOverTemperatureTemperature = %d\n", Bms4Msg->ChargingStationOverTemperatureTemperature);
	Bms4Msg->FireExtinguisherAlarm = CanReciveBuf[1] & 0x01;
	//printf("Bms4Msg->FireExtinguisherAlarm = %d\n", Bms4Msg->FireExtinguisherAlarm);

	Bms4Msg->ChargingStationTemperature1 = CanReciveBuf[2] - 40;
	//printf("Bms4Msg->ChargingStationTemperature1 = %d\n", Bms4Msg->ChargingStationTemperature1);
	Bms4Msg->ChargingStationTemperature2 = CanReciveBuf[3] - 40;
	//printf("Bms4Msg->ChargingStationTemperature2 = %d\n", Bms4Msg->ChargingStationTemperature2);
	Bms4Msg->ChargingStationTemperature3 = CanReciveBuf[4] - 40;
	//printf("Bms4Msg->ChargingStationTemperature3 = %d\n", Bms4Msg->ChargingStationTemperature3);
	Bms4Msg->ChargingStationTemperature4 = CanReciveBuf[5] - 40;
	//printf("Bms4Msg->ChargingStationTemperature4 = %d\n", Bms4Msg->ChargingStationTemperature4);

	Bms4Msg->ActiveBreakdownCount = CanReciveBuf[6];
	//printf("Bms4Msg->ActiveBreakdownCount = %d\n", Bms4Msg->ActiveBreakdownCount);

	Bms4Msg->ChargeComplete = CanReciveBuf[7] >> 5 & 0x01;
	//printf("Bms4Msg->ChargeComplete = %d\n", Bms4Msg->ChargeComplete);
	Bms4Msg->WarmModuleTurnoff = CanReciveBuf[7] >> 4 & 0x01;
	//printf("Bms4Msg->WarmModuleTurnoff = %d\n", Bms4Msg->WarmModuleTurnoff);
	Bms4Msg->HighPressureTurnoff = CanReciveBuf[7] >> 3 & 0x01;
	//printf("Bms4Msg->HighPressureTurnoff = %d\n", Bms4Msg->HighPressureTurnoff);
	Bms4Msg->CellSystemPilesUnmatched = CanReciveBuf[7] >> 2 & 0x01;
	//printf("Bms4Msg->CellSystemPilesUnmatched = %d\n", Bms4Msg->CellSystemPilesUnmatched);
	Bms4Msg->CellOverRecharge = CanReciveBuf[7] >> 1 & 0x01;
	//printf("Bms4Msg->CellOverRecharge = %d\n", Bms4Msg->CellOverRecharge);
	Bms4Msg->SOCSaltusStep = CanReciveBuf[7] & 0x01;
	//printf("Bms4Msg->SOCSaltusStep = %d\n", Bms4Msg->SOCSaltusStep);
}

void Can2Bms5Msg(uint32_t Id, uint32_t time, proto_Bms5 *Bms5Msg, u_int8_t CanReciveBuf[])
{
	Bms5Msg->CarId = Id;
	Bms5Msg->Timestamp = time;
	Bms5Msg->CellSystemRatedCapacity = CanReciveBuf[1] << 8 | CanReciveBuf[0];
	//printf("Bms5Msg->CellSystemRatedCapacity = %d\n", Bms5Msg->CellSystemRatedCapacity);
	Bms5Msg->CellSystemRatedVoltage = (CanReciveBuf[3] << 8 | CanReciveBuf[2]) * 0.1;
	//printf("Bms5Msg->CellSystemRatedVoltage = %f\n", Bms5Msg->CellSystemRatedVoltage);
	Bms5Msg->CellMonomerCascadeNumber = CanReciveBuf[5] << 8 | CanReciveBuf[4];
	//printf("Bms5Msg->CellMonomerCascadeNumber = %d\n", Bms5Msg->CellMonomerCascadeNumber);
	Bms5Msg->CellMonomerMultipleNumber = CanReciveBuf[6];
	//printf("Bms5Msg->CellMonomerMultipleNumber = %d\n", Bms5Msg->CellMonomerMultipleNumber);
	Bms5Msg->CellSystemTemperatureProbeNumber = CanReciveBuf[7];
	//printf("Bms5Msg->CellSystemTemperatureProbeNumber = %d\n", Bms5Msg->CellSystemTemperatureProbeNumber);
}

void Can2Bms6Msg(uint32_t Id, uint32_t time, proto_Bms6 *Bms6Msg, u_int8_t CanReciveBuf[])
{
	Bms6Msg->CarId = Id;
	Bms6Msg->Timestamp = time;
	Bms6Msg->CellTemperatureMaxValue = CanReciveBuf[0] - 40;
	//printf("Bms6Msg->CellTemperatureMaxValue = %d\n", Bms6Msg->CellTemperatureMaxValue);
	Bms6Msg->CellMaxTemperatureCode = CanReciveBuf[1];
	//printf("Bms6Msg->CellMaxTemperatureCode = %d\n", Bms6Msg->CellMaxTemperatureCode);
	Bms6Msg->CellMaxTemperatureNum = CanReciveBuf[2];
	//printf("Bms6Msg->CellMaxTemperatureNum = %d\n", Bms6Msg->CellMaxTemperatureNum);
	Bms6Msg->CellTemperatureMinValue = CanReciveBuf[3] - 40;
	//printf("Bms6Msg->CellTemperatureMinValue = %d\n", Bms6Msg->CellTemperatureMinValue);
	Bms6Msg->CellMinTemperatureCode = CanReciveBuf[4];
	//printf("Bms6Msg->CellMinTemperatureCode = %d\n", Bms6Msg->CellMinTemperatureCode);
	Bms6Msg->CellMinTemperatureNum = CanReciveBuf[5];
	//printf("Bms6Msg->CellMinTemperatureNum = %d\n", Bms6Msg->CellMinTemperatureNum);
	Bms6Msg->CellTypeInfo = CanReciveBuf[6];
	//printf("Bms6Msg->CellTypeInfo = %d\n", Bms6Msg->CellTypeInfo);
	Bms6Msg->CellCollMode = CanReciveBuf[7];
	//printf("Bms6Msg->CellCollMode = %d\n", Bms6Msg->CellCollMode);
}

void Can2Bms7Msg(uint32_t Id, uint32_t time, proto_Bms7 *Bms7Msg, u_int8_t CanReciveBuf[])
{
	Bms7Msg->CarId = Id;
	Bms7Msg->Timestamp = time;
	Bms7Msg->CellMaxVoltageCode = CanReciveBuf[0];
	//printf("Bms7Msg->CellMaxVoltageCode = %d\n", Bms7Msg->CellMaxVoltageCode);
	Bms7Msg->CellMaxVoltageNum = CanReciveBuf[1];
	//printf("Bms7Msg->CellMaxVoltageNum = %d\n", Bms7Msg->CellMaxVoltageNum);
	Bms7Msg->CellMinVoltageCode = CanReciveBuf[2];
	//printf("Bms7Msg->CellMinVoltageCode = %d\n", Bms7Msg->CellMinVoltageCode);
	Bms7Msg->CellMinVoltageNum = CanReciveBuf[3];
	//printf("Bms7Msg->CellMinVoltageNum = %d\n", Bms7Msg->CellMinVoltageNum);
}

void Can2Bms8Msg(uint32_t Id, uint32_t time, proto_Bms8 *Bms8Msg, u_int8_t CanReciveBuf[])
{
	Bms8Msg->CarId = Id;
	Bms8Msg->Timestamp = time;
	Bms8Msg->ForwardCellNum1 = CanReciveBuf[6] << 8 | CanReciveBuf[0];
	//printf("Bms8Msg->ForwardCellNum1 = %d\n", Bms8Msg->ForwardCellNum1);
	Bms8Msg->ForwardCellVoltage1 = (CanReciveBuf[2] << 8 | CanReciveBuf[1]) * 0.01 - 10000;
	//printf("Bms8Msg->ForwardCellVoltage1 = %f\n", Bms8Msg->ForwardCellVoltage1);
	Bms8Msg->ForwardCellNum2 = CanReciveBuf[7] << 8 | CanReciveBuf[3];
	//printf("Bms8Msg->ForwardCellNum2 = %d\n", Bms8Msg->ForwardCellNum2);
	Bms8Msg->ForwardCellVoltage2 = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.01 - 10000;
	//printf("Bms8Msg->ForwardCellVoltage2 = %f\n", Bms8Msg->ForwardCellVoltage2);
}

void Can2Bms9Msg(uint32_t Id, uint32_t time, proto_Bms9 *Bms9Msg, u_int8_t CanReciveBuf[])
{
	Bms9Msg->CarId = Id;
	Bms9Msg->Timestamp = time;
	Bms9Msg->ForwardCellNum5 = CanReciveBuf[6] << 8 | CanReciveBuf[0];
	//printf("Bms9Msg->ForwardCellNum5 = %d\n", Bms9Msg->ForwardCellNum5);
	Bms9Msg->ForwardCellVoltage5 = (CanReciveBuf[2] << 8 | CanReciveBuf[1]) * 0.01 - 10000;
	//printf("Bms9Msg->ForwardCellVoltage5 = %f\n", Bms9Msg->ForwardCellVoltage5);
	Bms9Msg->NegativeCellNum1 = CanReciveBuf[7] << 8 | CanReciveBuf[3];
	//printf("Bms9Msg->NegativeCellNum1 = %d\n", Bms9Msg->NegativeCellNum1);
	Bms9Msg->NegativeCellVoltage1 = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.01 - 10000;
	//printf("Bms9Msg->NegativeCellVoltage1 = %f\n", Bms9Msg->NegativeCellVoltage1);
}

void Can2Bms10Msg(uint32_t Id, uint32_t time, proto_Bms10 *Bms10Msg, u_int8_t CanReciveBuf[])
{
	Bms10Msg->CarId = Id;
	Bms10Msg->Timestamp = time;
	Bms10Msg->CellNum1 = CanReciveBuf[6] << 8 | CanReciveBuf[0];
	//printf("Bms10Msg->CellNum1 = %d\n", Bms10Msg->CellNum1);
	Bms10Msg->CellVoltage1 = (CanReciveBuf[2] << 8 | CanReciveBuf[1]) * 0.01 - 10000;
	//printf("Bms10Msg->CellVoltage1 = %f\n", Bms10Msg->CellVoltage1);
	Bms10Msg->CellNum2 = CanReciveBuf[7] << 8 | CanReciveBuf[3];
	//printf("Bms10Msg->CellNum2 = %d\n", Bms10Msg->CellNum2);
	Bms10Msg->CellVoltage2 = (CanReciveBuf[5] << 8 | CanReciveBuf[4]) * 0.01 - 10000;
	//printf("Bms10Msg->CellVoltage2 = %f\n", Bms10Msg->CellVoltage2);
}

void Can2Bms11Msg(uint32_t Id, uint32_t time, proto_Bms11 *Bms11Msg, u_int8_t CanReciveBuf[])
{
	Bms11Msg->CarId = Id;
	Bms11Msg->Timestamp = time;
	Bms11Msg->ForwardSamplingSite1Number = CanReciveBuf[0];
	//printf("Bms11Msg->ForwardSamplingSite1Number = %d\n", Bms11Msg->ForwardSamplingSite1Number);
	Bms11Msg->ForwardSamplingSite1Temperature = CanReciveBuf[1] - 40;
	//printf("Bms11Msg->ForwardSamplingSite1Temperature = %d\n", Bms11Msg->ForwardSamplingSite1Temperature);
	Bms11Msg->ForwardSamplingSite2Number = CanReciveBuf[2];
	//printf("Bms11Msg->ForwardSamplingSite2Number = %d\n", Bms11Msg->ForwardSamplingSite2Number);
	Bms11Msg->ForwardSamplingSite2Temperature = CanReciveBuf[3] - 40;
	//printf("Bms11Msg->ForwardSamplingSite2Temperature = %d\n", Bms11Msg->ForwardSamplingSite2Temperature);
	Bms11Msg->ForwardSamplingSite3Number = CanReciveBuf[4];
	//printf("Bms11Msg->ForwardSamplingSite3Number = %d\n", Bms11Msg->ForwardSamplingSite3Number);
	Bms11Msg->ForwardSamplingSite3Temperature = CanReciveBuf[5] - 40;
	//printf("Bms11Msg->ForwardSamplingSite3Temperature = %d\n", Bms11Msg->ForwardSamplingSite3Temperature);
	Bms11Msg->ForwardSamplingSite4Number = CanReciveBuf[6];
	//printf("Bms11Msg->ForwardSamplingSite4Number = %d\n", Bms11Msg->ForwardSamplingSite4Number);
	Bms11Msg->ForwardSamplingSite4Temperature = CanReciveBuf[7] - 40;
	//printf("Bms11Msg->ForwardSamplingSite1Temperature = %d\n", Bms11Msg->ForwardSamplingSite4Temperature);
}

void Can2Bms12Msg(uint32_t Id, uint32_t time, proto_Bms12 *Bms12Msg, u_int8_t CanReciveBuf[])
{
	Bms12Msg->CarId = Id;
	Bms12Msg->Timestamp = time;
	Bms12Msg->ForwardSamplingSite5Number = CanReciveBuf[0];
	//printf("Bms12Msg->ForwardSamplingSite5Number = %d\n", Bms12Msg->ForwardSamplingSite5Number);
	Bms12Msg->ForwardSamplingSite5Temperature = CanReciveBuf[1] - 40;
	//printf("Bms12Msg-> ForwardSamplingSite5Temperature = %d\n", Bms12Msg->ForwardSamplingSite5Temperature);
	Bms12Msg->NegativeSamplingSite1Number = CanReciveBuf[2];
	//printf("Bms12Msg-> NegativeSamplingSite1Number = %d\n", Bms12Msg->NegativeSamplingSite1Number);
	Bms12Msg->NegativeSamplingSite1Temperature = CanReciveBuf[3] - 40;
	//printf("Bms12Msg->NegativeSamplingSite1Temperature = %d\n", Bms12Msg->NegativeSamplingSite1Temperature);
	Bms12Msg->NegativeSamplingSite2Number = CanReciveBuf[4];
	//printf("Bms12Msg->NegativeSamplingSite2Number = %d\n", Bms12Msg->NegativeSamplingSite2Number);
	Bms12Msg->NegativeSamplingSite2Temperature = CanReciveBuf[5] - 40;
	//printf("Bms12Msg->NegativeSamplingSite2Temperature = %d\n", Bms12Msg->NegativeSamplingSite2Temperature);
	Bms12Msg->NegativeSamplingSite3Number = CanReciveBuf[6];
	//printf("Bms12Msg->NegativeSamplingSite3Number= %d\n", Bms12Msg->NegativeSamplingSite3Number);
	Bms12Msg->NegativeSamplingSite3Temperature = CanReciveBuf[7] - 40;
	//printf("Bms12Msg->NegativeSamplingSite3Temperature = %d\n", Bms12Msg->NegativeSamplingSite3Temperature);
}

void Can2Bms13Msg(uint32_t Id, uint32_t time, proto_Bms13 *Bms13Msg, u_int8_t CanReciveBuf[])
{
	Bms13Msg->CarId = Id;
	Bms13Msg->Timestamp = time;
	Bms13Msg->NegativeSamplingSite4Number = CanReciveBuf[0];
	//printf("Bms13Msg->NegativeSamplingSite4Number = %d\n", Bms13Msg->NegativeSamplingSite4Number);
	Bms13Msg->NegativeSamplingSite4Temperature = CanReciveBuf[1] - 40;
	//printf("Bms13Msg-> NegativeSamplingSite4Temperature = %d\n", Bms13Msg->NegativeSamplingSite4Temperature);
	Bms13Msg->NegativeSamplingSite5Number = CanReciveBuf[2];
	//printf("Bms13Msg-> NegativeSamplingSite5Number = %d\n", Bms13Msg->NegativeSamplingSite5Number);
	Bms13Msg->NegativeSamplingSite5Temperature = CanReciveBuf[3] - 40;
	//printf("Bms13Msg->NegativeSamplingSite5Temperature = %d\n", Bms13Msg->NegativeSamplingSite5Temperature);
}

void Can2Bms14Msg(uint32_t Id, uint32_t time, proto_Bms14 *Bms14Msg, u_int8_t CanReciveBuf[])
{
	Bms14Msg->CarId = Id;
	Bms14Msg->Timestamp = time;
	Bms14Msg->SamplingSite1Number = CanReciveBuf[0];
	//printf("Bms14Msg->SamplingSite1Number= %d\n", Bms14Msg->SamplingSite1Number);
	Bms14Msg->SamplingSite1Temperature = CanReciveBuf[1] - 40;
	//printf("Bms14Msg->SamplingSite1Temperature = %d\n", Bms14Msg->SamplingSite1Temperature);
	Bms14Msg->SamplingSite2Number = CanReciveBuf[2];
	//printf("Bms14Msg->SamplingSite2Number = %d\n", Bms14Msg->SamplingSite2Number);
	Bms14Msg->SamplingSite2Temperature = CanReciveBuf[3] - 40;
	//printf("Bms14Msg->SamplingSite2Temperature = %d\n", Bms14Msg->SamplingSite2Temperature);
	Bms14Msg->SamplingSite3Number = CanReciveBuf[4];
	//printf("Bms14Msg->SamplingSite3Number = %d\n", Bms14Msg->SamplingSite3Number);
	Bms14Msg->SamplingSite3Temperature = CanReciveBuf[5] - 40;
	//printf("Bms14Msg->SamplingSite3Temperature = %d\n", Bms14Msg->SamplingSite3Temperature);
	Bms14Msg->SamplingSite4Number = CanReciveBuf[6];
	//printf("Bms14Msg->SamplingSite4Number= %d\n", Bms14Msg->SamplingSite4Number);
	Bms14Msg->SamplingSite4Temperature = CanReciveBuf[7] - 40;
	//printf("Bms14Msg->SamplingSite4Temperature = %d\n", Bms14Msg->SamplingSite4Temperature);
}

void Can2Bms15Msg(uint32_t Id, uint32_t time, proto_Bms15 *Bms15Msg, u_int8_t CanReciveBuf[])
{
	Bms15Msg->CarId = Id;
	Bms15Msg->Timestamp = time;
	Bms15Msg->CellPermissibleDisconnectionIdentification = CanReciveBuf[0];
	//printf("Bms15Msg->CellPermissibleDisconnectionIdentification= %d\n", Bms15Msg->CellPermissibleDisconnectionIdentification);
	Bms15Msg->MainContactorRequestStatus = CanReciveBuf[1];
	//printf("Bms15Msg->MainContactorRequestStatus= %d\n", Bms15Msg->MainContactorRequestStatus);
	Bms15Msg->ElectricDefrostingContactorRequestStatus = CanReciveBuf[2];
	//printf("Bms15Msg->ElectricDefrostingContactorRequestStatus= %d\n", Bms15Msg->ElectricDefrostingContactorRequestStatus);
	Bms15Msg->ElectricHeatingContactorRequestStatus = CanReciveBuf[3];
	//printf("Bms15Msg->ElectricHeatingContactorRequestStatus= %d\n", Bms15Msg->ElectricHeatingContactorRequestStatus);
	Bms15Msg->ACContactorRequestStatus = CanReciveBuf[4];
	//printf("Bms15Msg->ACContactorRequestStatus= %d\n", Bms15Msg->ACContactorRequestStatus);
}

void Can2OilPump1Msg(uint32_t Id, uint32_t time, proto_OilPump1 *OilPump1Msg, u_int8_t CanReciveBuf[])
{
	OilPump1Msg->CarId = Id;
	OilPump1Msg->Timestamp = time;
	OilPump1Msg->MotorSpeed = CanReciveBuf[1] << 8 | CanReciveBuf[0];
	//printf("OilPump1Msg->MotorSpeed= %d\n", OilPump1Msg->MotorSpeed);
	OilPump1Msg->HeartbeatSignal = CanReciveBuf[5];
	//printf("OilPump1Msg->HeartbeatSignal= %d\n", OilPump1Msg->HeartbeatSignal);
	OilPump1Msg->HighVoltageFaultMarkerBit = CanReciveBuf[6] & 0x03;
	//printf("OilPump1Msg->HighVoltageFaultMarkerBit= %d\n", OilPump1Msg->HighVoltageFaultMarkerBit);
	OilPump1Msg->HighVoltageFaultRating = CanReciveBuf[6] >> 4 & 0x03;
	//printf("OilPump1Msg->HighVoltageFaultRating= %d\n", OilPump1Msg->HighVoltageFaultRating);
}

void Can2OilPump2Msg(uint32_t Id, uint32_t time, proto_OilPump2 *OilPump2Msg, u_int8_t CanReciveBuf[])
{
	OilPump2Msg->CarId = Id;
	OilPump2Msg->Timestamp = time;
	OilPump2Msg->OilPumpEnabled = CanReciveBuf[0];
	//printf("OilPump2Msg->OilPumpEnabled= %d\n", OilPump2Msg->OilPumpEnabled);
	OilPump2Msg->VehicleSpeed = CanReciveBuf[1];
	//printf("OilPump2Msg->VehicleSpeed= %d\n", OilPump2Msg->VehicleSpeed);
}

void Can2MagneticGridMsg(u_int8_t CanReciveBuf[])
{
	uint8_t Latitude[4] = {0};
	uint8_t Longitude[4] = {0};
	Latitude[0] = CanReciveBuf[3];
	Latitude[1] = CanReciveBuf[2];
	Latitude[2] = CanReciveBuf[1];
	Latitude[3] = CanReciveBuf[0];
	Longitude[0] = CanReciveBuf[7];
	Longitude[1] = CanReciveBuf[6];
	Longitude[2] = CanReciveBuf[5];
	Longitude[3] = CanReciveBuf[4];
//#if CURR_GPS == MAGNETICGRID_GPS
    stCurrVehicleInfo.flMagneticLongitude = *((float*)Longitude);
    stCurrVehicleInfo.flMagneticLatitude = *((float*)Latitude);
//#endif
	//printf("MagneticGrid Longitude = %f\n", stCurrVehicleInfo.flMagneticLongitude);
	//printf("MagneticGrid Latitude = %f\n", stCurrVehicleInfo.flMagneticLatitude);
}

static void canFrameAnalysis(struct can_frame *frame)   
{
	switch (frame->can_id & CAN_EFF_MASK) //或 frame->can_id - CAN_EFF_FLAG
	{
	case VCU1_ID: 
		Can2Vcu1Msg(CAR_ID, 0, &CANMessage.Vcu1Msg, frame->data);
		break;
	case VCU2_ID: 
		pthread_mutex_lock(&mutex2);
		Can2Vcu2Msg(CAR_ID, 0, &CANMessage.Vcu2Msg, frame->data);
		pthread_mutex_unlock(&mutex2);
		break;
	//case VCU3_ID: 
	//	Can2Vcu3Msg(CAR_ID, 0, &CANMessage.Vcu3Msg, frame->data);
	//	break;
	//case VCU4_ID: 
	//	Can2Vcu4Msg(CAR_ID, 0, &CANMessage.Vcu4Msg, frame->data);
	//	break;
	case METER1_ID:
		pthread_mutex_lock(&mutex2);
		Can2Meter1Msg(CAR_ID, 0, &CANMessage.Meter1Msg, frame->data); 
		pthread_mutex_unlock(&mutex2);
		break;
	case METER2_ID: 
		Can2Meter2Msg(CAR_ID, 0, &CANMessage.Meter2Msg, frame->data); 
		break;
	//case AIRCONDITION_ID: 
	//	Can2AirConditionMsg(CAR_ID, 0, &CANMessage.AirConditionMsg, frame->data);
	//	break;
	case BMS1_ID: 
		Can2Bms1Msg(CAR_ID, 0, &CANMessage.Bms1Msg, frame->data);
		break;
#if 0
	case BMS2_ID: 
		Can2Bms2Msg(CAR_ID, 0, &CANMessage.Bms2Msg, frame->data);
		break;
	case BMS3_ID: 
		Can2Bms3Msg(CAR_ID, 0, &CANMessage.Bms3Msg, frame->data);
		break;
	case BMS4_ID: 
		Can2Bms4Msg(CAR_ID, 0, &CANMessage.Bms4Msg, frame->data);
		break;
	case BMS5_ID: 
		Can2Bms5Msg(CAR_ID, 0, &CANMessage.Bms5Msg, frame->data);
		break;
	case BMS6_ID: 
		Can2Bms6Msg(CAR_ID, 0, &CANMessage.Bms6Msg, frame->data);
		break;
	case BMS7_ID: 
		Can2Bms7Msg(CAR_ID, 0, &CANMessage.Bms7Msg, frame->data);
		break;
	case BMS8_ID:
		Can2Bms8Msg(CAR_ID, 0, &CANMessage.Bms8Msg, frame->data);
		break;
	case BMS9_ID:
		Can2Bms9Msg(CAR_ID, 0, &CANMessage.Bms9Msg, frame->data);
		break;
	case BMS10_ID:
		Can2Bms10Msg(CAR_ID, 0, &CANMessage.Bms10Msg, frame->data);
		break;
	case BMS11_ID:
		Can2Bms11Msg(CAR_ID, 0, &CANMessage.Bms11Msg, frame->data);
		break;
	case BMS12_ID: 
		Can2Bms12Msg(CAR_ID, 0, &CANMessage.Bms12Msg, frame->data);
		break;
	case BMS13_ID: 
		Can2Bms13Msg(CAR_ID, 0, &CANMessage.Bms13Msg, frame->data);
		break;
	case BMS14_ID: 
		Can2Bms14Msg(CAR_ID, 0, &CANMessage.Bms14Msg, frame->data);
		break;
	case BMS15_ID: 
		Can2Bms15Msg(CAR_ID, 0, &CANMessage.Bms15Msg, frame->data);
		break;
	case OILPUMP1_ID: //油泵1
		Can2OilPump1Msg(CAR_ID, 0, &CANMessage.OilPump1Msg, frame->data);
		break;
	case OILPUMP2_ID: //油泵2
		Can2OilPump2Msg(CAR_ID, 0, &CANMessage.OilPump2Msg, frame->data);
		break;
#endif
	case MAGNETICGRID_ID:
		Can2MagneticGridMsg(frame->data);
		break;
	default:
		break;
	}
}

void CANMessageUpdateProcess(void)
{

	int s, nbytes;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct can_frame frame;
	struct can_filter rfilter[1];
	memset(&CANMessage, 0, sizeof(CANMessage));
	system("ip  link set can0 down");
	system("ip  link set can0 type can bitrate 250000 loopback off");
	system("ip  link set can0 up");

	s = socket(PF_CAN, SOCK_RAW, CAN_RAW); //创建套接字
	strcpy(ifr.ifr_name, "can0");
	ioctl(s, SIOCGIFINDEX, &ifr); //指定can0设备
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	bind(s, (struct sockaddr *)&addr, sizeof(addr)); //将套接字与can0绑定
													 //定义接收规则，只接收表示符等于m0x11的报文
	// rfilter[0].can_id = 0x18F105EF;
	//  rfilter[0].can_mask = CAN_EFF_MASK;
	////设置过滤规则
	//	setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
	uint32_t count = 0;
	while (1)
	{
		//printf("read redy\n");
		nbytes = read(s, &frame, sizeof(frame)); //接收报文
		/*printf("nbytes = %d\n", nbytes); 
		if ((frame.can_id & 0x7fffffff) == 0x180328f6)
		{
			printf("ID = 0x%x DLC = %d \ndata = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n", (frame.can_id & 0x7fffffff),
				   frame.can_dlc, frame.data[0],
				   frame.data[1],
				   frame.data[2],
				   frame.data[3],
				   frame.data[4],
				   frame.data[5],
				   frame.data[6],
				   frame.data[7]);
		}*/
		if (nbytes <= 0)
		{
			printf("read failed!\n");
			sleep(1);
			continue;
		}
		//count++;
		//printf("count = %d\n", count);
		canFrameAnalysis(&frame);
		//usleep(10000);
	}
	close(s);
}

proto_CANMessage *getCANMessage(void)
{
	return &CANMessage;
}

struct GearSpeed *getGearSpeed(void)
{
	static struct GearSpeed gearspeed;
	gearspeed.Gear = CANMessage.Vcu2Msg.Gear;
	gearspeed.VehicleSpeed = CANMessage.Meter1Msg.VehicleSpeed;
	gearspeed.MotorSpeed = CANMessage.Vcu1Msg.MotorSpeed;
	gearspeed.MeterTotalMileage = CANMessage.Meter2Msg.MeterTotalMileage;
	gearspeed.SOCValue = CANMessage.Bms1Msg.SOCValue;
	gearspeed.BackDoorOpenSignal = CANMessage.Meter1Msg.BackDoorOpenSignal;
	gearspeed.FrontDoorOpenSignal = CANMessage.Meter1Msg.FrontDoorOpenSignal;
	gearspeed.TurnLeftInstruction = CANMessage.Meter1Msg.TurnLeftInstruction;
	gearspeed.FarLightInstruction = CANMessage.Meter1Msg.FarLightInstruction;
	gearspeed.TurnRightInstruction = CANMessage.Meter1Msg.TurnRightInstruction;
	return &gearspeed;
}
