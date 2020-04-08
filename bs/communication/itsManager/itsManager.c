/*-----------------------------------------------------------------------------*/
/*

	FileName	itsManager

	EditHistory
	2019-04-09	creat file 
    2019-04-17  stm32------TCP protobuf ----- RSU -----PC5 protobuf -------OBU ------- TCP protobuf ------ server 调通

*/
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* general include                                                             */
/*-----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>

#include <unistd.h>

/*-----------------------------------------------------------------------------*/
/* module include                                                              */
/*-----------------------------------------------------------------------------*/
#include "../proto/pb_common.h"
#include "../proto/pb_decode.h"
#include "../proto/pb_encode.h"
#include "../proto/pb.h"
#include "../proto/test.pb.h"

#include "../TargetSettings.h"

#include "../commNet/commNet.h"
#include "../commV2X/commV2XTx/commV2XTx.h"
#include "../commRoadBlock/commRoadBlock.h"

#include "../logic/position.h"

#define ITS_MANAGER_C
#include "itsManager.h" /* self head file */
/*-----------------------------------------------------------------------------*/
/* private macro define                                                        */
/*-----------------------------------------------------------------------------*/
#if CURR_MODULE == RSU_MODULE
#define SEND_BUF_TEST 0
#define SEND_BUF_TEST_LEN 2000
#endif

#define V2X_SEND_BUF_SIZE 1000

#define TEST_CLEAR_ITS

static proto_RoadBlockRequest stRoadBlockRequest = {0};

/*-----------------------------------------------------------------------------*/
/* private typedef define                                                      */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* private parameters                                                          */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* private function                                                  		   */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	FuncName	getLocationInfo
	function	获取当前信息并赋值给proto_RoadBlockRequest
	EditHistory		2019-06-04 creat
-----------------------------------------------------------------------------*/
static void getRoadBlockInfo(void)
{
    static proto_position_data_t *ptrCarLocationRequest = NULL;
    //
    //从position获取数据
    //
    ptrCarLocationRequest = carLocationGet();
    if (ptrCarLocationRequest == NULL)
    {
        printf("GPS idle\n");
        return;
    }

    stRoadBlockRequest.longitude = ptrCarLocationRequest->longitude;
    stRoadBlockRequest.latitude = ptrCarLocationRequest->latitude;
    //stRoadBlockRequest.longitude = 118.19139;
    //stRoadBlockRequest.latitude = 24.48663;

    printf("======================GPS INFO START==========================\n");
    printf("longitude = %5.5f\n", ptrCarLocationRequest->longitude);
    printf("latitude = %5.5f\n", ptrCarLocationRequest->latitude);
    printf("======================GPS INFO END==========================\n");
}

/*-----------------------------------------------------------------------------
	FuncName	getRoadBlockInfoToUpload
	function	获取proto_RoadBlockRequest结构体信息并返回地址
	return value	返回proto_RoadBlockRequest结构体指针
	EditHistory		2019-06-04 creat
-----------------------------------------------------------------------------*/
static proto_RoadBlockRequest *getRoadBlockInfoToUpload(void)
{
    proto_RoadBlockRequest *stRetPtr = NULL;
    getRoadBlockInfo();
    sleep(1);
    stRetPtr = &stRoadBlockRequest;
    return stRetPtr;
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName v2xObuTxBufGet	

    获取组装好的Obu数据，广播出去。。。
    包括位置信息，车辆ID ，前方转向信息等  

	EditHistory
	2019-04-19  create by WPz.
*/
static uint8_t *v2xObuTxBufGet(uint16_t *bufLen, uint64_t time)
{
    bool rec_bool;
    pb_istream_t istream;
    pb_ostream_t ostream;
    static uint8_t sendBuf[V2X_SEND_BUF_SIZE];
    proto_RoadBlockRequest *stRetPtr = NULL;
    memset(sendBuf, 0, sizeof(sendBuf));

    *bufLen = 0;

    stRetPtr = &stRoadBlockRequest;
    stRetPtr->Time = time;

    //
    //数据流配置
    //
    ostream = pb_ostream_from_buffer(sendBuf, V2X_SEND_BUF_SIZE);

    //
    //将buf数据编码后写入流
    //
    rec_bool = pbEncode(&ostream, proto_RoadBlockRequest_fields, stRetPtr);
    if (rec_bool == false)
    {
        printf("pbEncode failed!!!\n");
        return NULL;
    }

    //
    //写入数据长度
    //
    *bufLen = ostream.bytes_written;

    return sendBuf;
}

/*-----------------------------------------------------------------------------*/
/* public function                                                             */
/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/*
	FuncName v2xProcessCreate	

	EditHistory
	2019-04-11  creat func
    2019-04-17  stm32 ------ TCP protobuf ----- RSU -----PC5 protobuf -------OBU ------- TCP protobuf ------ server 调通
*/
void itsProcessCreate(void)
{
    pthread_t threadidV2xTx = 0;
    pthread_t threadidNet = 0;
    pthread_t threadidRoadBlock = 0;

#ifndef TEST_CLEAR_ITS 
    printf("uint8_t size = %d\n", sizeof(uint8_t));
    printf("uint32_t size = %d\n", sizeof(uint32_t));
    printf("double size = %d\n", sizeof(double));
#endif
    //设置初始ID和限速值，路障限速值设为0
#if CURR_MODULE == RB_MODULE
    stRoadBlockRequest.SpeedLimit = 0;
    stRoadBlockRequest.RsuId = 300;
#elif CURR_MODULE == SL_MODULE
    stRoadBlockRequest.SpeedLimit = 15.0;
    stRoadBlockRequest.RsuId = 200;
#else
    stRoadBlockRequest.SpeedLimit = 0;
    stRoadBlockRequest.RsuId = 0;
#endif

    //新建上传云服务端线程
    pthread_create(&threadidNet, NULL, (void *)&uploadRoadBlockProcess, &getRoadBlockInfoToUpload);
    pthread_detach(threadidNet);

    sleep(1);

    //新建OBU广播线程
    pthread_create(&threadidV2xTx, NULL, (void *)&v2xTxProcess, &v2xObuTxBufGet);
    pthread_detach(threadidV2xTx);
	
#if CURR_MODULE == SL_MODULE
    //新建上传限速牌线程
    pthread_create(&threadidRoadBlock, NULL, (void *)&RoadBlockProcess, NULL);
    pthread_detach(threadidRoadBlock);
#endif
    /* do nothing */
    while (1)
    {
        ;
    }
}

/* end of file */

