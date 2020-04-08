/*-----------------------------------------------------------------------------*/
/*

	FileName	position

	EditHistory
	2019-04-09	creat file 

*/
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* general include                                                             */
/*-----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <linux/in6.h>
#include <signal.h>
#include <v2x_kinematics_api.h>

/*-----------------------------------------------------------------------------*/
/* module include                                                              */
/*-----------------------------------------------------------------------------*/
//#include "../communication/proto/test.pb.h"
#include "../TargetSettings.h"

#define POSITION_C
#include "position.h" /* self head file */
/*-----------------------------------------------------------------------------*/
/* private macro define                                                        */
/*-----------------------------------------------------------------------------*/
#define errExit(msg)   do{perror(msg); return ;}while (0)     
    
// Number of microseconds that a kinematics fix is valid before timing out
#define KINEMATICS_TIMEOUT (1000000)

#define TEST_POSITION_CLEAR
// #define UWB_TEST
/*-----------------------------------------------------------------------------*/
/* private typedef define                                                      */
/*-----------------------------------------------------------------------------*/

typedef struct _kinematics_data_t
{
    bool inuse;
    bool has_fix;
    bool initialized;
    uint64_t timestamp;
    v2x_location_fix_t *latest_fix;
} kinematics_data_t;

// Location data relevant to acme messages
typedef struct _location_data_t
{
    bool isvalid;
    v2x_fix_mode_t fix_mode;
    double latitude;
    double longitude;
    double altitude;
    uint32_t qty_SV_used;
    bool has_SemiMajorAxisAccuracy;
    double SemiMajorAxisAccuracy;
    bool has_heading;
    double heading; /* Track degrees relative to true north  */
    bool has_velocity;
    double velocity; /* Speed  over  ground  in meters/second */
} location_data_t;

/*-----------------------------------------------------------------------------*/
/* private parameters                                                          */
/*-----------------------------------------------------------------------------*/

// Timer for kinematics initialization retry
static __timer_t init_kinematics_timer;
static int init_kinematics_timer_signo;

// Kinematics fields
static v2x_kinematics_handle_t h_v2x = V2X_KINEMATICS_HANDLE_BAD;
static bool kinematics_initialized = false;
static kinematics_data_t kinematics_data = {.inuse = false, .has_fix = false};
static uint64_t numFixesReceived = 0ull;
// static proto_CarInfoRequest stCarInfoRequest;
static location_data_t location = {0};

static proto_position_data_t currPosition = {0};
/*-----------------------------------------------------------------------------*/
/* private function                                                  		   */
/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2x_kinematics_newfix_callBack
    
    return current time stamp in milliseconds

	EditHistory
	2019-04-18  create func from acme 
*/
static __inline uint64_t timestampNow(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	load_location_data
    
    Checks if latest fix is valid and copies a subset of location fields into the location
    data struct. These fields include latitude, longitude, altitude, fix_mode,
    qty_SV_used, heading, velocity, and SemiMajorAxisAccuracy.
    Returns true if data copied successfully.

	EditHistory
	2019-04-18  create func from acme 
*/
/*
 *
 * @return bool
 */
static bool load_location_data(location_data_t *location, kinematics_data_t *kinematics_data)
{
    location->isvalid = false;
    // Copy the kinematics data into the msg
    if (kinematics_data->has_fix)
    {
        // printf("kinematics_data->has_fix == true\n");
        if (timestampNow() - kinematics_data->timestamp < KINEMATICS_TIMEOUT)
        {
            location->isvalid = true;
            location->latitude = kinematics_data->latest_fix->latitude;
            location->longitude = kinematics_data->latest_fix->longitude;
            location->altitude = kinematics_data->latest_fix->altitude;
            location->fix_mode = kinematics_data->latest_fix->fix_mode;
            location->qty_SV_used = kinematics_data->latest_fix->qty_SV_used;
            if (kinematics_data->latest_fix->has_heading)
            {
                location->has_heading = true;
                location->heading = kinematics_data->latest_fix->heading;
            }
            if (kinematics_data->latest_fix->has_velocity)
            {
                location->has_velocity = true;
                location->velocity = kinematics_data->latest_fix->velocity;
            }
            if (kinematics_data->latest_fix->has_SemiMajorAxisAccuracy)
            {
                location->has_SemiMajorAxisAccuracy = true;
                location->SemiMajorAxisAccuracy =
                    kinematics_data->latest_fix->SemiMajorAxisAccuracy;
            }
#ifndef TEST_POSITION_CLEAR
            printf(" \nGPS info latitude = %f  \n", location->latitude);
            printf(" GPS info longitude = %f  \n", location->longitude);
            printf(" GPS info altitude = %f  \n", location->altitude);
#endif
        }
    }
    return location->isvalid;
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2x_kinematics_newfix_callBack

	EditHistory
	2019-04-18  create func from acme 
*/
static void v2x_kinematics_newfix_callBack(v2x_location_fix_t *new_fix, void *context)
{
    static uint32_t fix_count = 0u;
    // Don't do anything if SVs used 0.
    if (V2X_GNSS_MODE_NO_FIX == new_fix->fix_mode)
    {
        return;
    }
    // printf("GPS info updated!\n");
    kinematics_data.has_fix = true;
    kinematics_data.latest_fix = new_fix;
    kinematics_data.timestamp = timestampNow();
    if (new_fix->utc_fix_time)
    {
        ++numFixesReceived;
    }
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2x_kinematics_init_callBack

	EditHistory
	2019-04-18  create func from acme 
*/
static void v2x_kinematics_init_callBack(v2x_status_enum_type status, void *context)
{
    if (V2X_STATUS_SUCCESS == status)
    {
        {
            printf("v2x callback - initialized successfully. \n");
        }
        kinematics_initialized = true;
    }
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2x_kinematics_final_callBack

	EditHistory
	2019-04-18  create func from acme 
*/
static void v2x_kinematics_final_callBack(v2x_status_enum_type status, void *context)
{
    printf("v2x final callback, status=%d\n", status);
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	kinematics_initialize
    Try to initialize the kinematics client, called on timer trigger.
    If initialized, register the callback.  Disarm timer if initialization
    is successful.

	EditHistory
	2019-04-18  create func from acme 
*/
static void kinematics_initialize(int signum)
{

    v2x_init_t v2x_init;
    v2x_init.log_level_mask = 0xffffffff;
    struct itimerspec its;

    snprintf(v2x_init.server_ip_addr, sizeof(v2x_init.server_ip_addr), "%s", "192.168.100.1");

    static uint32_t retryCount = 0u;
    if (!kinematics_initialized)
    { // Try initialize
        // Deallocate handle if it failed to initialize previously
        if (V2X_KINEMATICS_HANDLE_BAD != h_v2x)
        {
            v2x_kinematics_final(h_v2x, &v2x_kinematics_final_callBack, NULL);
        }
        if (retryCount)
        {
            {
                printf("Retrying ... \n");
            }
        }
        else
        {
            {
                printf("Initializing Kinematics API.\n");
            }
        }
        h_v2x = v2x_kinematics_init(&v2x_init, v2x_kinematics_init_callBack, NULL);
        ++retryCount;
    }

    if (kinematics_initialized)
    {
        // The Kinematics API has been initialized
        {
            printf("Kinematics API initialization successful.\n");
        }
        // Register the new_fix listener
        if (V2X_STATUS_SUCCESS ==
            v2x_kinematics_register_listener(h_v2x, v2x_kinematics_newfix_callBack, NULL))
        {
            {
                printf("Kinematics listener registration successful.\n");
            }
            // Disarm timer
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_nsec = 0;
            its.it_interval.tv_sec = 0;
            if (timer_settime(init_kinematics_timer, 0, &its, NULL) == -1)
            {
                errExit("disarm_timer");
            }
        }
        else
        {
            {
                printf("Kinematics listener registration unsuccessful.\n");
            }
        }
    }
}

/*-----------------------------------------------------------------------------*/
/* public function                                                             */
/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/*
	FuncName	start_kinematics
    Setup initializing of the kinematics client.  Starts a timer to trigger intitialization attempts,
    will retry until successful completion.

	EditHistory
	2019-04-18  create func from acme 
*/
static void start_kinematics(void)
{
    /* Initialize the kinematics API */
    struct itimerspec its;
    struct sigevent sev;
    // Setup up the timer and signal
    init_kinematics_timer_signo = SIGUSR2;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = init_kinematics_timer_signo;
    sev.sigev_value.sival_ptr = &init_kinematics_timer;
    if (timer_create(CLOCK_REALTIME, &sev, &init_kinematics_timer) == -1)
    {
        errExit("timer_create");
    }

    signal(init_kinematics_timer_signo, kinematics_initialize);

    /* Start the timer */
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(init_kinematics_timer, 0, &its, NULL) == -1)
    {
        errExit("arm_timer");
    }

    kinematics_initialize(0); // call once now to print header and initialze delta timers
}


/*-----------------------------------------------------------------------------
	FuncName	gpsInfoUpdateSetup
	function	获取location_data_t结构体的GPS信息
	return value	返回void
	EditHistory		2019-04-10 creat function buy wpz
-----------------------------------------------------------------------------*/
void gpsInfoUpdateSetup(void)
{
    memset(&currPosition, 0, sizeof(currPosition));
    memset(&location, 0, sizeof(location));
    start_kinematics();
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	carLocationGet

    获取车辆

	EditHistory
	2019-04-19  create func by WPz
*/
proto_position_data_t *carLocationGet(void)
{
    proto_position_data_t *retPtr = NULL;
    if (load_location_data(&location, &kinematics_data))
    {
        if (location.has_heading)
        {
            currPosition.heading = location.heading;
        }
        else
        {
            currPosition.heading = -1;
        }

        currPosition.latitude = location.latitude;
        currPosition.longitude = location.longitude;
        currPosition.velocity = location.velocity;
        retPtr = &currPosition;
    }
    else
    {
        printf("load location fail.\n");
    }

    return retPtr;
}
#ifdef UWB_TEST
#include "../driver/serial_port.h"

void uwbDataTest(void)
{
    int8_t fd = open_serial_port("/dev/ttymxc2");
    uint8_t u8PrintBuf[100];
    if (fd > 0)
    {
        if (serial_port_init(fd, 115200, 8, 'N', 1) == 0)
        {
            while(1)
            {
                uint32_t len = read(fd, u8PrintBuf, sizeof(u8PrintBuf));
                if (len > 0)
                {
                    for (int i = 0; i < len; i++)
                    {
                        printf("0x%0x,", u8PrintBuf[i]);
                        if ((i % 10 == 0) || (i == len - 1))
                        {
                            printf("\n");
                        }
                    }
                }
            }
        }
    }
}
#endif
/* end of file */
