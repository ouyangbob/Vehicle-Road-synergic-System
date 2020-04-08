#ifndef TARGET_SETTINGS_H
#define TARGET_SETTINGS_H

/*-----------------------------------------------------------------------------*/
/* target optiion                                                              */
/*-----------------------------------------------------------------------------*/
#define RSU_MODULE  1
#define OBU_MODULE  2
#define RB_MODULE   3
#define SL_MODULE   4
#define TEST_MODULE 5

#define CURR_MODULE SL_MODULE

/*-----------------------------------------------------------------------------*/
/* server to upload setup                                                      */
/*-----------------------------------------------------------------------------*/
#define PROTOBUF_SERVER_IP "47.111.188.230"
#define PROTOBUF_SERVER_PORT 8989

/*-----------------------------------------------------------------------------*/
/* server to roadblock setup                                                      */
/*-----------------------------------------------------------------------------*/
#define ROADBLOCK_SERVER_IP "192.168.1.123"
#define ROADBLOCK_SERVER_PORT 4001

#endif
