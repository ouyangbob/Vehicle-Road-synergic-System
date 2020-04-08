/*
 *  Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/in6.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <v2x_radio_api.h>
#include <v2x_log.h>
#include <v2x_kinematics_api.h>
#include <math.h>
#include <glib.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include "controller.h"

#define MAX_QTY_PAYLOADS (1000)
#define MAX_SPS_FLOWS (2)
#define MAX_FLOWS (100)

/* Regarding these sample port#: any unused ports could be selected, and additional ports
 * could also be set-up for additional reservations and near unlimited number of event flows. */
#define SAMPLE_SPS_PORT   (2500)
#define SAMPLE_EVENT_PORT (2600)
#define DEFAULT_PAYLOAD_QTY (5)

#define DEFAULT_SERVICE_ID (1)
/* Simulated secured BSM lengths with 5 path history points */
#define PAYLOAD_SIZE_CERT (287)
#define PAYLOAD_SIZE_DIGEST (187)

/* Two possible reservation sizes, if only one made, it uses the latgest of the two */
#define DEFAULT_SPS_RES_BYTES_0 (PAYLOAD_SIZE_CERT)
#define DEFAULT_SPS_RES_BYTES_1 (PAYLOAD_SIZE_DIGEST)

#define DEFAULT_PERIODICITY_MS (100)

// 255 unique RV's tracked for IPG measurement, one byte UE ID
#define MAX_UEID (255)

// Maximum length of iface
#define MAX_IFACE_NAME_LEN (50)

// Some limits beyond which warnings will be generated, based on current known support
#define TOP_TYPICAL_PRECONFIG_SERVICE_ID (10)
#define WARN_MIN_PERIODICITY_MS (100)

const char v2x_event_strings[V2X_TXRX_SUSPENDED + 1][20] = {
    "Inactive",
    "Active",
    "TX Suspended",
    "RX Suspended",
    "TX + RX Suspended"
};

const char *event_name(v2x_event_t event)
{
    return (event <= V2X_TXRX_SUSPENDED ? v2x_event_strings[event] : "ERR: Unkonwn event.");
}

// NaN representation in csv
#define NaN " "

// Number of microseconds that a kinematics fix is valid before timing out
#define KINEMATICS_TIMEOUT (1000000)
// Interface names
const char default_loopback_iface_name[] = "lo";
const char default_v2x_ip_iface_name[] = "rmnet_data0";
const char default_v2x_non_ip_iface_name[] = "rmnet_data1";
const char default_db820_ethernet_name[] = "enP2p1s0";
const char default_proxy_if_name[] = "rndis0";

// Max config file line length
#define MAX_CONF_LINELEN (1024)
int recv_flag = 0; //recv finish flag == 1 meas finished
char default_conf_path[] = "/data/acme.conf";

// Header rows for csv file
const char csv_header_row_location[] = "fix_mode,latitude,longitude,altitude,qty_SV_used,"
                                       "SemiMajorAxisAccuracy,heading,velocity";
const char csv_header_row_message[] = "v2x_family_id,equipment_id,seq_num,timestamp";
const char csv_header_row_receiver[] = "rx timestamp (us),rx receiver fix mode,rx latitude,"
                                       "rx longitude,rx altitude (m),rx quantity of SV used,"
                                       "rx Semi Major Axis Accuracy,rx heading,rx velocity,"
                                       "tx family id,tx equipment id,tx seq num,tx timestamp (us),"
                                       "tx fix_mode,tx latitude,tx longitude,tx altitude (m),"
                                       "tx quantity SV used,tx Semi Major Axis Accuracy,"
                                       "tx heading,tx velocity,distance (m),latency (ms),"
                                       "per_ue_loss_pct (%),ipg (ms)";
const char csv_header_row_sender[] = "v2x_family_id,equipment_id,seq_num,timestamp,fix_mode,"
                                     "latitude,longitude,altitude,qty_SV_used,"
                                     "SemiMajorAxisAccuracy,heading,velocity";

#define errExit(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef enum {
    NoReg = 0,
    SpsOnly = 1,
    EventOnly = 2,
    ComboReg = 3
} FlowType_t;

char *iface;
char iface_buffer[MAX_IFACE_NAME_LEN];
char *proxy_if_name;
int dump_opt = 0;//control whether print recv_buf data 1 print
int ascii_dump_opt = 0;
int sps_event_opt = 0;
int gVerbosity = 3;
int g_use_syslog = 0;
v2x_priority_et event_tx_prio = V2X_PRIO_2;
int qty = -1; // number of packets after which to quit, either tx/rx or echo. -1 = indefinite.
unsigned long interval_btw_pkts_ns = -1;
int payload_len[MAX_QTY_PAYLOADS] = {0};
int payload_qty = 0;
v2x_radio_handle_t handle;
v2x_iface_capabilities_t gCapabilities;

v2x_radio_calls_t radio_calls;
v2x_per_sps_reservation_calls_t sps_function_calls;

/* g_neg_latency_warns is a warning counter of how many "negative latencies" were observed, which means time on at least one pair of
 * sender/receiver was not synchronized, and invalidates all latency maeasurements, its content if non-zero is reported
 * at the end of the receiver's test run. */
int g_neg_latency_warns = 0;

FlowType_t flow_type[MAX_FLOWS];
int sps_sock[MAX_FLOWS];
int event_sock[MAX_FLOWS];
struct sockaddr_in6 sps_sockaddr[MAX_FLOWS];
struct sockaddr_in6 event_sockaddr[MAX_FLOWS];
struct sockaddr_in6 dest_sockaddr;

int rx_sock;
struct sockaddr_in6 rx_sockaddr;

int rx_count = 0; // number of packets to RX or TX. -1 = indefinite.
int tx_count = 0; // number of tx packets

/* The reservation request includes the service id, priority, interval, byte size */
v2x_tx_bandwidth_reservation_t demo_res_req[MAX_FLOWS] = {0};
uint16_t sps_port[MAX_FLOWS] = {0};
uint16_t evt_port[MAX_FLOWS] = {0};

/* counter FlowCnt keeps track of how many total flows (event only, sps, etc) have been set-up.
 * starts from 0, and used as an index into demo_res_req[], sps_port[],evt_port[] etc
 * */
unsigned int FlowCnt = 0;

/* counter FlowCnt keeps track of how many SPS resrvations have been set-up.
 * This is a natural counting number, 0 means no SPS, 2 means two actual SPS flows were requested*/
unsigned int SpsFlowCnt = 0;

/*Var that remembers the index of the SPS flow that had the largest value */
int LargestSpsIndex = 0;

v2x_sps_mac_details_t details;

static char buf[MAX_DUMMY_PACKET_LEN];
char recv_buf[MAX_DUMMY_PACKET_LEN] = {0};//recvive obu sendback data
unsigned int user_data_len = 0;//user's data
int total_recv_len = 0;

unsigned int user_data_to_server_len = 0;//user's data


static int rx_len = 0;

extern char pbuf[PBUF_LEN];                                                                                                                                                  
extern int pbuf_len;        
int send_len = 0;


// We can possibly be setting up both directions with one running ACME instance
int do_air2net_proxy = 0; // "air2net" being kind of downlink to "wire" where net is UDP network stack, say ethernet.
int do_net2air_proxy = 0;

/*  Global variables related to proxy of network to radio */
/* default_proxy_ip_addr could contain a ipv6 or IPV4 address */
char default_proxy_ip_addr[] = "127.0.0.1";
char *proxy_remote_net_address = default_proxy_ip_addr;
int remote_ai_family = -1; // cache whether caller specified IPV4 or IPV6

uint16_t proxy_net2air_listen_port = 2498;
uint16_t proxy_air2net_forward_to_port = 2499;

static int listen_net_socket =  -1;
static int forward_net_socket =  -1;
fd_set rread;

// FIXME should be array, union of ip6 or ip4
static struct sockaddr_in6 proxy_listen_sin;
static struct sockaddr_in6 proxy_remote_sin;
static struct  sockaddr_in6 proxy6_local;
static struct sockaddr_in proxy_remote_sin4;

static uint16_t  g_tx_seq_num = 0;
static unsigned int  g_missed_packets = 0;
static unsigned int  g_out_of_order_packets = 0;
static unsigned int g_timer_misses = 0;  // should invalidate a test run.  something on HLOS overloaded

unsigned char g_testverno_magic = 'R';  // Magic test message proto version number
unsigned char g_ueid = 1; // UE

uint64_t rv_rx_timestamp[MAX_UEID]; // Keep track of last RX'ed timestamp of each RV for IPG
uint32_t rv_rx_cnt[MAX_UEID]; // Keep track of total packets received for each RV
uint32_t rv_presumed_sent[MAX_UEID]; // Keep track  how many packets we think other side has
uint16_t rv_seq_num[MAX_UEID]; // Keep track of last RV seq_num received for each RV
uint16_t rv_missed_packets[MAX_UEID]; // Keep track of last RV seq_num received for each RV
uint16_t rv_seq_start_num[MAX_UEID]; // Keep track of sequence start for each RV, so we can calculate PER

float min_latency[MAX_UEID];
float max_latency[MAX_UEID];
double latency_sum[MAX_UEID]; // Running total , for ongoing average calculation
float avg_latency[MAX_UEID];
double overall_sum_latency = 0.0 ;   // summed over all the received packets, from all RV's (UEID's)
double overall_avg_latency = 0.0 ;   // averaged over all the received packets, from all RV's (UEID's)
typedef enum {
    ModeTx = 0,
    ModeRx,
    ModeEcho
} TestMode_et;

const char g_test_mode_names[][5] = {"Tx", "Rx", "Echo" };

TestMode_et test_mode = ModeTx;  // Transmit test mode by default

const char *g_curr_test_str;  // Just a pointer to one of the g_test_mode_name strings

typedef enum {
    tx_test_name_sps   = 0,
    tx_test_name_event = 1,
    tx_test_name_combo = 2
} tx_test_name_et;

const char g_tx_mode_names[][12] = {"SPS", "Event", "sps+event" };
const char *tx_mode_str = g_tx_mode_names[tx_test_name_sps];
char dest_ipv6_addr_str[50] = "ff02::1";

uint16_t cla_portnum = 0; // CLA command line argument for port spec, 0 indicates use default

// Machine Readable Log file
char *g_ml_fn = NULL;
FILE   *g_ml_fp = NULL;


///////////////////////////
extern void *pthread_sendto_cloud(void* ptr);
extern sem_t buf_number;



long long  gReportInterval_ns = 1000000000;
double gdReportInterval_sec = 1;

typedef struct _kinematics_data_t {
    bool       inuse;
    bool       has_fix;
    bool       initialized;
    uint64_t    timestamp;
    v2x_location_fix_t *latest_fix;
} kinematics_data_t;

// Location data relevant to acme messages
typedef struct _location_data_t {
    bool isvalid;
    v2x_fix_mode_t fix_mode;
    double latitude;
    double longitude;
    double altitude;
    uint32_t qty_SV_used;
    bool has_SemiMajorAxisAccuracy;
    double SemiMajorAxisAccuracy;
    bool has_heading;
    double heading;
    bool has_velocity;
    double velocity;
} location_data_t;

// Acme message contents
typedef struct _acme_message_t {
    char v2x_family_id;
    char equipment_id;
    bool has_seq_num;
    uint16_t seq_num;
    bool has_timestamp;
    uint64_t timestamp;
    bool has_kinematics;
    location_data_t location;
    uint32_t payload_len;
} acme_message_t;

// Timer for kinematics initialization retry
static timer_t init_kinematics_timer;
static int init_kinematics_timer_signo;

// Kinematics fields
static v2x_kinematics_handle_t h_v2x = V2X_KINEMATICS_HANDLE_BAD;
static bool v2x_kinematics_initialized = false;
static kinematics_data_t kinematics_data = {.inuse = false, .has_fix = false};
static uint64_t numFixesReceived = 0ull;

// Default IP for kinematics server
#ifdef MACHINE_HAS_MDM_PCIE_EP
char server_ip_str[20] = "qti-modem";
#else
char server_ip_str[20] = "127.0.0.1";
#endif

float g_cbp_meas = 99.0; // Just a odd measurement to assume, till we hear otherwise via callback
uint64_t last_cbp_meas_time = 0;
v2x_event_t g_status = V2X_INACTIVE; // recent Event, from listner: effectively the status (TX/RX suspended/Active/etc)
uint64_t last_status_change_time = 0;
int lost_tx_cnt = 0; // Count how many times the status indicated loss of timing precision, causin gTx suspend
int lost_rx_cnt = 0; // Count the number of times, status indicated a loss of RX timing

void init_per_rv_stats(void)
{
    int i;

    for (i = 0; i < MAX_UEID; i++) {
        rv_rx_timestamp[i] = 0;
        rv_rx_cnt[i] = 0;
        rv_seq_num[i] = 0;
        rv_seq_start_num[i] = 0;
        rv_missed_packets[i] = 0;
        rv_presumed_sent[0];
    }
}

void ShowResults(FILE *fp)
{
    int i;

    if (test_mode != ModeTx) {
        fprintf(fp, "Per RV packet stats:\n");
        fprintf(fp, "====================\n");

        for (i = 0; i < MAX_UEID; i++) {
            if (rv_rx_cnt[i]) {
                float missed_packet_pct = 0;

                if (rv_presumed_sent[i]) {
                    missed_packet_pct = rv_missed_packets[i] * 100.0 / (float)rv_presumed_sent[i];
                }

                fprintf(fp, "UE Id#%d|Rx=%d|TX=%d|%8.1f lost| min latency=%3.2f ms | avg latency=%3.2f ms | max latency=%3.2f ms|\n",
                        i,
                        rv_rx_cnt[i],
                        rv_presumed_sent[i],
                        (float)missed_packet_pct,
                        min_latency[i] / 1000.0,
                        (float) avg_latency[i] / 1000.0,
                        (float)(max_latency[i] / 1000.0)
                       );
            }
        }

        fprintf(fp, "Total missed packets from all RV: %d\n", g_missed_packets);
        fprintf(fp, "Total out of order packets from all RV: %d\n", g_out_of_order_packets);
    }

    fprintf(fp, "Transmit Timer Misses: %d\n", g_timer_misses);
    fprintf(fp, "Packets Received: %d\n", rx_count);
    fprintf(fp, "Packets Transmitted: %d\n", tx_count);

    if (g_neg_latency_warns)
        fprintf(fp, "*** WARNING: %d negative latency packets:  All latency results invalid.\n",
                g_neg_latency_warns);

    if (lost_tx_cnt) {
        fprintf(fp, "*** WARNING: %d ocassions of lost MAC timing good enough for TX\n", lost_tx_cnt);
    }

    if (lost_rx_cnt) {
        fprintf(fp, "*** WARNING: %d ocassions of lost MAC timing good enough support RX\n", lost_rx_cnt);
    }

}

/*******************************************************************************
 * return current time stamp in milliseconds
 * @return long long
 ******************************************************************************/
static __inline uint64_t timestamp_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/* Stubs for the various callbacks made on the radio interface */
void init_complete(v2x_status_enum_type status, void *ctx)
{
    LOGD("Init complete callback. status=%d ctx=%08lx\n", status, *(unsigned long *)ctx);
}

void radio_listener(v2x_event_t event, void *ctx)
{
	

    if (ctx) {
/*
        LOGD("====Radio event listener callback: status=%d (%s)  ctx=%08lx\n",
             event,
             event_name(event),
             *(unsigned long *) ctx);
     */
        if (event != g_status) {
            g_status = event;
            last_status_change_time = timestamp_now();
            LOGW("TX/RX Status Changed to <%s> **** \n", event_name(event));
            switch (event) {
            case V2X_TXRX_SUSPENDED:
                lost_tx_cnt++;
                lost_rx_cnt++;
                break;

            case V2X_TX_SUSPENDED:
                lost_tx_cnt++;
                break;

            case V2X_RX_SUSPENDED:
                lost_rx_cnt++;
                break;
            }
        } // Status actually changed.

    } else {
        LOGE("NULL Context on radio_listener\n");
    }
}

void meas_listener(v2x_chan_measurements_t *meas_p, void *ctx)
{
	//sample_rx(rx_sock);

    if (meas_p) {
        g_cbp_meas = meas_p->channel_busy_percentage;
        last_cbp_meas_time = timestamp_now();
        LOGD("Radio measurement callback. CBR=%3.0f\% \n", g_cbp_meas);
    } else {
        LOGE("NULL ptr on meas_listner() callback\n");
    }
}

void l2_changed_listener(int new_l2_address, void *ctx)
{
    LOGI("l2_addr-changed listener, l2addr=%06lx ctx=%08lx\n", new_l2_address,
         *(unsigned long *)ctx);
}

/*******************************************************************************
 * Usage message
 ******************************************************************************/
static void print_usage(const char *prog_name)
{
    printf("Usage: %s [-i <interface>] ... \n", prog_name);
    printf("  A generic test tool used as either a packet sender(default), reciver(-R) or echo (-e)\n");
    printf("  can also optionally be used as a UDP -> PC5 proxy or vice-versa\n\n");

    printf("  Defaults: interface=%s, no dump, SPS TX, v2x_id=1, sps_port=%d event_port=%d UEID=%d dest_ip=%s:%d\n",
           iface, sps_port[FlowCnt], evt_port[FlowCnt], g_ueid, dest_ipv6_addr_str, V2X_RX_WILDCARD_PORTNUM);
    printf("\t-A              ASCII dump packet data after sequence#\n");
    printf("\t-a              Additional SPS/Event Flow Pair. Subsequent -E,-I,-r -o flow params will apply to new reservation\n");
    printf("\t-b <interval sec>  Reporting interval for receive stats, set to 0 to disable.\n");
    printf("\t-C              Config File Path to set IP Address and Interface\n");
    printf("\t-d              dump raw packet\n");
    printf("\t-D <dest ip6>   Destination IPV6 address[ Default=%s]\n", dest_ipv6_addr_str);
    printf("\t-p <dest port>  port number that the destination traffic is sent to.[Default=%d]\n",
           (int)V2X_RX_WILDCARD_PORTNUM);
    printf("\t-e              Echo Mode. Re-TX each received message (via event port)\n");
    printf("\t-E              Event flow mode instead of SPS\n");
    printf("\t-F              Use event port of SPS flow registration\n");
    printf("\t-g              Include GNSS data, velocity, and heading in packets if possible\n");
    printf("\t-s <sps_port>   SPS port#.  Overrides the default of %d. +1 for each added flow\n", SAMPLE_SPS_PORT);
    printf("\t-t <evt_port>   Event driven port# .  Overrides the default of %d +1 for each added flow.\n",
           SAMPLE_EVENT_PORT);
    printf("\t-i <if_name>    Interface name as found in listing by ifconfig\n");
    printf("\t-I <milliseconds> interval for generating packets & any initial SPS reservation flow\n");
    printf("\t-u <microseconds> Alternative to -I, microseconds for finer control.\n");
    printf("\t-N <nanoseconds>  Alternative to -I, nanosecond interval.\n");
    printf("\t-k <qty>        Quit after <qty> packets received or transmitted\n");
    printf("\t-l <bytes [bytes] ... >   List of Payload Lengths in bytes, at least one length required\n");
    printf("\t                          specify multiple payloads. TX  will send one packet of each lengths in sequence.\n");
    printf("\t                           maximum number of length sequence %d different lengths.\n", MAX_QTY_PAYLOADS);
    printf("\t-M <millsec>    Specify reservation in milliseconds, multiple allowed when used with -a for multi-flow.\n");
    printf("\t-m <filename>   Log file, Machine readable of each Tx/Rx packet.\n");
    printf("\t-o <priority>   Traffic ID (typically between 0-2)\n");
    printf("\t-P <V2X ID>     V2X session ID to be used\n");
    printf("\t-q              Quiet\n");
    printf("\t-r <res size>   Reservation size for SPS flow in bytes\n");
    printf("\t-R              RECEIVE mode, default is TX\n");
    printf("\t-S <seq#>       optional sequence number to start on.\n");
    printf("\t-U <UEID>       1 Byte UE Identifier, prepended on TX payload, default=1, give each Tx a unique ID\n");
    printf("\t-V              Increase verbosity level +1 for each -V, default=0\n");
    printf("\t-Z              Use syslog instead of stdout for SDK debug/info/errors\n");
    printf("\n\n UDP proxy related flags:\n");
    printf("\t-L <port>       UDP proxy listen port for:  UDP -> PC5 forward \n");
    printf("\t-X <IP addr>    Proxy UDP IP address, specifying this turns on TX proxy.\n");
    printf("\t-x <interface>  Proxy interface name.\n");
    printf("\t-Y <dest port> Destination port for TX proxy\n");
    printf("\n\n");
    printf("Examples: \n");
    printf(" %s -RV          # Receives and displays all received packets.\n", prog_name);
    printf(" %s -RVd         # same as above but dumps the hex content.\n", prog_name);
    printf(" %s -VV -o1 -r300 -M500 -a -r200 -I100 -M100 -o1 # Will setup 2 SPS flows ala J2945/1\n", prog_name);
    printf("\n");

#ifdef SIM_BUILD
    printf(" SIM BUILD= %d\n", SIM_BUILD);
#endif
}

/**
 * returns 0 if parameters (global) are set within acceptable params
 * prints warnings
 * @param[in]  pointer to the structure containing the retrieved modem capabilities
 */
static int v2x_test_param_check(v2x_iface_capabilities_t *caps)
{
    int result = 0;
    int mtu = 0;
    int i = 0;
    int largest_sps_res_bytes = -1;

    if (!caps) {
        goto bail;
    }
    mtu = caps->link_ip_MTU_bytes;

    if (!strncmp(default_v2x_non_ip_iface_name, iface, strlen(iface))) {
        mtu = caps->link_non_ip_MTU_bytes;

        if (gVerbosity > 1) {
            printf("Using non-IP interface & MTU\n");
        }
    }

    if (test_mode == ModeTx) {
        // If not payload specified, load a default set
        if (payload_qty < 1) {
            payload_qty = DEFAULT_PAYLOAD_QTY;
            payload_len[0] = PAYLOAD_SIZE_CERT;
            payload_len[1] = PAYLOAD_SIZE_DIGEST;
            payload_len[2] = PAYLOAD_SIZE_DIGEST;
            payload_len[3] = PAYLOAD_SIZE_DIGEST;
            payload_len[4] = PAYLOAD_SIZE_DIGEST;
        }

        for (i = 0; i < payload_qty; i++) {
            if (payload_len[i] > mtu) {
                printf("** WARN ** payload #%d requested exceeds MTU of %d\n", i, mtu);
            }
        }
    }

    if (event_tx_prio > caps->highest_priority_value) {
        printf("** WARN ** test packet priority value higher than modem known to support(%d)\n",
               caps->highest_priority_value);
    }

    if (event_tx_prio < caps->lowest_priority_value) {
        printf("** WARN ** test packet priority value lower than modem known to support(%d)\n",
               caps->lowest_priority_value);
    }

    if (test_mode != ModeRx) {
        for (i = 0; i <= FlowCnt; i++) {
            if (demo_res_req[i].v2xid > TOP_TYPICAL_PRECONFIG_SERVICE_ID)  {
                printf("** WARN ** PSID/V2XID selected may not be a common preconfigured TQ service ID's\n");
            }


            if ((demo_res_req[i].period_interval_ms < 0) && (interval_btw_pkts_ns > 0))  {
                demo_res_req[i].period_interval_ms = interval_btw_pkts_ns / 1000000 ;
                printf("# SPS Interval periodicity not specified, using packet-gen interval: %d ms.\n",
                       demo_res_req[i].period_interval_ms);
            }

            // SPS Periodicity warnings
            if ((flow_type[i] != EventOnly) && (demo_res_req[i].period_interval_ms > 0)) {

                if (demo_res_req[i].period_interval_ms < WARN_MIN_PERIODICITY_MS)  {
                    printf("** WARN ** periodcity less than TQ supported.\n");
                }

                if (demo_res_req[i].tx_reservation_size_bytes > largest_sps_res_bytes) {
                    largest_sps_res_bytes = demo_res_req[i].tx_reservation_size_bytes;
                    LargestSpsIndex = i;
                }
            }
            printf("Flow#%d: type=%d file-desciptors:(%2d %2d) sps_port=%d evt_port=%d, %d ms, %d bytes\n",
                    i, flow_type[i], sps_sock[i],  event_sock[i], sps_port[i],
                    evt_port[i], demo_res_req[i].period_interval_ms,
                    demo_res_req[i].tx_reservation_size_bytes);
        }

    }

    goto bail;

error:
    result = -1;
bail:
    return (result);
}

/* Adding support for multiple reservations
 * Assume will be SPS, but latter switches could change
 * */
void AddNewDefaultFlow(int idx)
{
    if ((idx >= 0) && (idx < MAX_FLOWS)) {
        demo_res_req[idx].v2xid = DEFAULT_SERVICE_ID; // Must be 1, 2, 3, 4 for default config in most pre-configured units
        demo_res_req[idx].priority = V2X_PRIO_2 ; // 2 = lowest priority supported in TQ.
        demo_res_req[idx].period_interval_ms = -1; // Invalid Periodicity signaling to use Interval if not specified
        demo_res_req[idx].tx_reservation_size_bytes = (idx ? DEFAULT_SPS_RES_BYTES_1 : DEFAULT_SPS_RES_BYTES_0);
        flow_type[idx] = ComboReg; // By default we register an event with each SPS
        sps_port[idx] = SAMPLE_SPS_PORT + idx;
        evt_port[idx] = SAMPLE_EVENT_PORT + idx;
    } else {
        LOGE("Max number of compile time supported flows exceeded.\n");
        exit(-1);
    }
}

static void radio_query_and_print_param(const char *iface)
{
    v2x_radio_query_parameters(iface, &gCapabilities);
    if (gVerbosity) {
        printf("Modem %s capabilities:\n", iface);
        printf("\tnon IP MTU: %d (%s)\n", gCapabilities.link_non_ip_MTU_bytes, default_v2x_non_ip_iface_name);
        printf("\tIP MTU: %d (%s)", gCapabilities.link_ip_MTU_bytes, default_v2x_ip_iface_name);
        printf("\tmin periodicity : %d ms\n", gCapabilities.int_min_periodicity_multiplier_ms);
        printf("\tmax periodicity (lowest reserved Freq): %d ms\n", gCapabilities.int_maximum_periodicity_ms);
        printf("\thighest priority number supported: %d (lower # is more urgent)\n", gCapabilities.highest_priority_value);
        printf("\tlowest priority number supported: %d (lower # is more urgent)\n", gCapabilities.lowest_priority_value);
    }
}

static int parse_config()
{
    FILE *file;
    int ret = 0;

    // FIXME -- should have  a method to relocate the config file
    // and output/create one if missing.
    if (!access(default_conf_path, F_OK)) {
        printf("CONFIG FILE: %s\n", default_conf_path);
        file = fopen(default_conf_path, "r");
        if (NULL != file) {
            char data[MAX_CONF_LINELEN];
            size_t len = 0;

            // Can add more parameters to read as the CONFIG file evolves
            while (fgets(data, sizeof(data), file) != NULL) {
                char *parameter;
                if (parameter = strstr((char *)data, "IP=")) {
                    parameter = parameter + 3;
                    data[strcspn(data, "\n")] = 0;
                    g_strlcpy(dest_ipv6_addr_str, parameter, sizeof(dest_ipv6_addr_str));
                } else if (parameter = strstr((char *)data, "INTERFACE=")) {
                    parameter = parameter + 10;
                    data[strcspn(data, "\n")] = 0;
                    g_strlcpy(iface_buffer, parameter, sizeof(iface_buffer));
                    iface = iface_buffer;
                    radio_query_and_print_param(iface);
                } else if (parameter = strstr((char *)data, "IP_KINEMATICS=")) {
                    parameter = parameter + 14;
                    data[strcspn(data, "\n")] = 0;
                    g_strlcpy(server_ip_str, parameter, sizeof(server_ip_str));
                }
            }
            fclose(file);
        } else {
            ret = -1;
        }
    } else {
        ret = -1;
    }
    return ret;
}

static int parse_opts(int argc, char *argv[])
{
    int c;
    int rc = 0;

    /* defaults  config values, evnetually set via command line*/
#ifdef SIM_BUILD
    iface = (char *)default_loopback_iface_name;
#else
    iface = (char *)default_v2x_non_ip_iface_name;//rmnet_data1
#endif
    proxy_if_name = (char *)default_proxy_if_name;//rndis0

    AddNewDefaultFlow(FlowCnt);

    /* Timer interval, used for TX packet timing, derrived from reservation */
    interval_btw_pkts_ns = DEFAULT_PERIODICITY_MS * 1000000;
    //acme -G 127.0.0.1 
    while ((c = getopt(argc, argv,
                       "?aAb:dD:eEFgG:hi:I:k:l:M:m:N:o:P:p:qr:Rs:S:t:u:U:VL:x:X:Y:y:Z")) != -1) {
        switch (c) {
        case 'a':
            FlowCnt++;
            AddNewDefaultFlow(FlowCnt);
            printf("\tAdditional Flow #%d added\n", FlowCnt);
            break;
        case 'A':
            ascii_dump_opt = 1;
            break;
        case 'b':
            gdReportInterval_sec = atoll(optarg);

            gReportInterval_ns = (double)gdReportInterval_sec * 1000000000.0;

            if (gVerbosity) {
                if (gReportInterval_ns) {
                    printf("\tPeriodic reporting every %f sec \n", gdReportInterval_sec);
                } else {
                    printf("\tPeriodic packet count reporting disabled\n");
                }
            }
            break;
        case 'd':
            dump_opt = 1;
            break;
        case 'D':
            g_strlcpy(dest_ipv6_addr_str, optarg, sizeof(dest_ipv6_addr_str));
            printf("\tDesination IPV6 address set to %s\n", dest_ipv6_addr_str);
            break;
        case 'e':
            test_mode = ModeEcho;
            if (gVerbosity) {
                printf("Echo mode enabled. each RX will be sent back\n");
            }
            break;
        case 'E':
            flow_type[FlowCnt] = EventOnly;
            tx_mode_str = g_tx_mode_names[tx_test_name_event];
            if (gVerbosity) {
                printf("Event only flow\n");
            }
            break;
        case 'F':
            sps_event_opt = 1;
            if (gVerbosity) {
                printf("Event port #%d of SPS flow registration used for all TX\n", evt_port[FlowCnt]);
            }
            break;
        case 'g':
            printf("Include kinematics data in TX/RX\n");
            kinematics_data.inuse = true;
            break;
        case 'G':
            g_strlcpy(server_ip_str, optarg, sizeof(server_ip_str));//optarg == para
			
            if (gVerbosity) {
                printf("Setting kinematics server ip to %s, gVerbosity : %d\n", server_ip_str, gVerbosity);
            }
            break;
        case 'i':
            iface = optarg;
            radio_query_and_print_param(iface);
            break;
        case 'M':
            demo_res_req[FlowCnt].period_interval_ms =  strtoul(optarg, NULL, 10);
            printf("SPS Interval periodicity: %d ms.\n", demo_res_req[FlowCnt].period_interval_ms);
            break;
        case 'I':
            interval_btw_pkts_ns = 1000 * 1000 * strtoul(optarg, NULL, 10);

            if (gVerbosity) {
                printf("Generate a new packet every %lu ms (alternating flows) .\n",
                       interval_btw_pkts_ns / 1000000lu);
            }
            break;
        case 'u':
            interval_btw_pkts_ns = 1000 * strtoul(optarg, NULL, 10);
            demo_res_req[FlowCnt].period_interval_ms =  interval_btw_pkts_ns / 1000000;
            if (gVerbosity) {
                printf(" # Intra-packet TX interval=%d ns\n", (int) interval_btw_pkts_ns);
            }
            break;
        case 'N':
            interval_btw_pkts_ns =  strtoul(optarg, NULL, 10);
            demo_res_req[FlowCnt].period_interval_ms =  interval_btw_pkts_ns / 1000000000;
            if (gVerbosity) {
                printf(" # Intra-packet TX interval=%d ns\n", (int) interval_btw_pkts_ns);
            }
            break;
        case 'p':
            cla_portnum = atoi(optarg);
            printf("Portnum set to %d, for RX port when receving or dest port when transmitting\n", cla_portnum);
            break;
        case 'U':
            g_ueid =  atoi(optarg);
            if (gVerbosity) {
                printf(" # UE (User Equipment) identifier=%d will be added to all test messages\n", g_ueid);
            }
            break;
        case 'y':
            interval_btw_pkts_ns =  1000000000 / strtoul(optarg, NULL, 10);
            if (gVerbosity) {
                printf(" # Intra-packet TX interval=%lu ns\n", interval_btw_pkts_ns);
            }
            break;
        case 'k':
            qty = atoi(optarg);
            if (gVerbosity) {
                printf("Will exit after %d packets processed (TX or RX)\n", qty);
            }
            break;
        case 'l':
            optind--;
            for (; optind < argc && *argv[optind] != '-'; optind++) {
                if (payload_qty < MAX_QTY_PAYLOADS) {
                    payload_len[payload_qty] = atoi(argv[optind]);
                    if (payload_len[payload_qty] > MAX_DUMMY_PACKET_LEN) {
                        LOGE("max supported test length: %d\n", MAX_DUMMY_PACKET_LEN);
                        goto badparam;
                    }
                    if (gVerbosity) {
                        printf("(payload #%d len=%d\n}", payload_qty, payload_len[payload_qty]);
                    }
                    payload_qty++;
                } else {
                    LOGE("Max total number of unique payloads exceeded (%d)\n", MAX_QTY_PAYLOADS);
                    goto badparam;
                }
            }
            break;
        case 'm':
            g_ml_fn = (optarg);
            if (gVerbosity) {
                printf("# Machine readable log file: %s\n", g_ml_fn);
            }
            break;
        case 'o':
            // Priority Set for both Event & SPS, unless -O flag used.
            demo_res_req[FlowCnt].priority = event_tx_prio = atoi(optarg);
            break;
        case 'P':
            demo_res_req[FlowCnt].v2xid = atoi(optarg);
            if (gVerbosity) {
                printf("PSID/Service ID set to %d\n", demo_res_req[FlowCnt].v2xid);
            }
            break;
        case 'q':
            gVerbosity = 0;
            break;
        case 'r':
            demo_res_req[FlowCnt].tx_reservation_size_bytes = atoi(optarg);
            break;
        case 'R':
            test_mode = ModeRx; // Rx mode suspends automatic sending. though Echo mode could still be activated via -e
            break;
        case 's':
            sps_port[FlowCnt] = atoi(optarg);
            break;
        case 'S':
            g_tx_seq_num = atoi(optarg);
            break;
        case 't':
            evt_port[FlowCnt] = atoi(optarg);
            break;
        /* Proxy related swtiches /params  grouped below */
        case 'L':
            proxy_net2air_listen_port = atoi(optarg);
            do_net2air_proxy = 1; // take network packets and broadcast on v2x link
            printf("*** Doing wire to air proxy, lisen port %d\n", proxy_net2air_listen_port);
            break;
        case 'x':
            proxy_if_name = optarg;
            break;
        case 'X':
            /* Could be IPV4 or IPV6 address */
            proxy_remote_net_address = optarg;
            do_air2net_proxy = 1;
            printf(" Enabling PC5 air link to UDP downlink proxy\n");
            break;
        // REVISIT -- COMBINE X & Y to just take IP and port always?
        //
        case 'Y':
            proxy_air2net_forward_to_port = atoi(optarg);
            do_air2net_proxy = 1;
            break;
        case 'V':
            gVerbosity++;
            break;
        case 'Z':
            g_use_syslog = 1;
            break;
        case '?':
        case 'h':
        default:
badparam:
            print_usage(argv[0]);
            exit(-1);
            goto bail;
        }
    }

bail:
    return rc;
}

/*
 * Returns the approximate distance between two (lat, long) positions
 * with the haversine formula.
 * @return double
 */
double calculate_distance(location_data_t *loc_a, location_data_t *loc_b)
{
    // Uses the haversine formula to calculate the great-circle distance betwen two points
    double R, a, c, d, lat_a, lat_b, long_a, long_b;
    R = 6371000.0; // Radius of the earth (in meters)

    // Convert lat and long to radians
    lat_a = loc_a->latitude * (M_PI / 180.0);
    long_a = loc_a->longitude * (M_PI / 180.0);
    lat_b = loc_b->latitude * (M_PI / 180.0);
    long_b = loc_b->longitude * (M_PI / 180.0);

    // Apply the haversine formula
    a = pow((sin(lat_b - lat_a) / 2.0), 2) +
        (cos(lat_a) * cos(lat_b) * pow(sin(long_b - long_a) / 2.0, 2));
    c = 2 * atan2(sqrt(a), sqrt(1.0 - a));
    d = R * c;

    return d;
}

/*
 * Checks if latest fix is valid and copies a subset of location fields into the location
 * data struct. These fields include latitude, longitude, altitude, fix_mode,
 * qty_SV_used, heading, velocity, and SemiMajorAxisAccuracy.
 * Returns true if data copied successfully.
 *
 * @return bool
 */
bool load_location_data(location_data_t *location, kinematics_data_t *kinematics_data)
{
    location->isvalid = false;
	
	printf("=======   %d  ========\n",__LINE__);
    // Copy the kinematics data into the msg
    if (kinematics_data->has_fix) {
		
		printf("=======   %d  ========\n",__LINE__);
        if (timestamp_now() - kinematics_data->timestamp < KINEMATICS_TIMEOUT) {
			
			printf("=======   %d  ========\n",__LINE__);
            location->isvalid = true;
            location->latitude = kinematics_data->latest_fix->latitude;
            location->longitude = kinematics_data->latest_fix->longitude;
            location->altitude = kinematics_data->latest_fix->altitude;
            location->fix_mode = kinematics_data->latest_fix->fix_mode;
            location->qty_SV_used = kinematics_data->latest_fix->qty_SV_used;
            if (kinematics_data->latest_fix->has_heading) {
                location->has_heading = true;
                location->heading = kinematics_data->latest_fix->heading;
            }
            if (kinematics_data->latest_fix->has_velocity) {
                location->has_velocity = true;
                location->velocity = kinematics_data->latest_fix->velocity;
            }
            if (kinematics_data->latest_fix->has_SemiMajorAxisAccuracy) {
                location->has_SemiMajorAxisAccuracy = true;
                location->SemiMajorAxisAccuracy =
                    kinematics_data->latest_fix->SemiMajorAxisAccuracy;
            }
        }
    }
    return location->isvalid;
}

/*
 * Writes location data to file in .csv format.
 * Returns true if location data is valid and written
 * @return bool
 */
bool write_location_to_file(FILE *fp, location_data_t *location)
{
    bool success = false;

    if (location && location->isvalid) {
        fprintf(fp, "%i,", location->fix_mode);
        if (location->fix_mode == V2X_GNSS_MODE_2D) {
            fprintf(fp, "%lf,%lf," NaN ",", location->latitude, location->longitude);
        } else if (location->fix_mode == V2X_GNSS_MODE_3D) {
            fprintf(fp, "%lf,%lf,%lf,", location->latitude, location->longitude,
                    location->altitude);
        } else {
            fprintf(fp, NaN "," NaN "," NaN ",");
        }
        fprintf(fp, "%u,", location->qty_SV_used);

        if (location->has_SemiMajorAxisAccuracy) {
            fprintf(fp, "%lf,", location->SemiMajorAxisAccuracy);
        } else {
            fprintf(fp, NaN ",");
        }

        if (location->has_heading) {
            fprintf(fp, "%lf,", location->heading);
        } else {
            fprintf(fp, NaN ",");
        }

        if (location->has_velocity) {
            fprintf(fp, "%lf,", location->velocity);
        } else {
            fprintf(fp, NaN ",");
        }
        success = true;
    } else {
        fprintf(fp, NaN "," NaN "," NaN "," NaN "," NaN "," NaN "," NaN "," NaN ",");
    }
    return success;
}

/*
 * Writes given message to file in .csv format.
 */
void write_message_to_file(FILE *fp, acme_message_t *msg)
{
    fprintf(fp, "%u,%u,", msg->v2x_family_id, msg->equipment_id);

    if (msg->has_seq_num) {
        fprintf(fp, "%u,", msg->seq_num);
    } else {
        fprintf(fp, NaN ",");
    }

    if (msg->has_timestamp) {
        fprintf(fp, "%lu,", msg->timestamp);
    } else {
        fprintf(fp, NaN ",");
    }

    // Write messages location information
    write_location_to_file(fp, &msg->location);
}

/*
 * Write reciever data to file in .csv format
 */
void write_receiver_data(FILE *fp,
                         uint64_t rx_timestamp,
                         acme_message_t *msg,
                         location_data_t *location,
                         double distance,
                         int64_t latency,
                         float per_ue_loss_pct,
                         uint64_t ipg)
{
    // Write rx_timestamp
    fprintf(fp, "%lu,", rx_timestamp);
    // Write most recent loaction data to file
    write_location_to_file(fp, location);
    // Write received message data to file
    write_message_to_file(fp, msg);
    // Write distance if valid
    if (distance) {
        fprintf(fp, "%lf,", distance);
    } else {
        fprintf(fp, NaN ",");
    }
    // Write receiver statistics
    fprintf(fp, "%lf,", (double)latency / 1000.0);
    fprintf(fp, "%8.1f,", per_ue_loss_pct);
    fprintf(fp, "%6.2f,", (double)ipg / 1000.0);
    fprintf(fp, "\n");
}

/*
 * Prints message contents to stdout.
 */
void print_acme_message(acme_message_t *msg)
{
    printf("------------------------MSG-------------------------\n");
    printf("Family ID: %u\n", msg->v2x_family_id);
    printf("Equipment ID: %u\n", msg->equipment_id);

    if (msg->has_seq_num) {
        printf("Seq Num: %u\n", msg->seq_num);
    }
    if (msg->has_timestamp) {
        printf("Timestamp: %lu\n", msg->timestamp);
    }
    if (msg->has_kinematics) {
        printf("(%f,%f) \n", msg->location.latitude, msg->location.longitude);
        printf("Altutude: %f\n", msg->location.altitude);
        printf("Qty SV Used: %u\n", msg->location.qty_SV_used);

        if (msg->location.has_heading) {
            printf("Heading: %f\n", msg->location.heading);
        }
        if (msg->location.has_velocity) {
            printf("Velocity: %f\n", msg->location.velocity);
        }
        if (msg->location.has_SemiMajorAxisAccuracy) {
            printf("SemiMajorAxisAccuracy: %lf\n", msg->location.SemiMajorAxisAccuracy);
        }
    }
    printf("----------------------------------------------------\n");
}

/*
 * Serialize acme message and copy to buffer.
 * Return the number of bytes copied.
 * @return uint32_t
 */
 #if 0
uint32_t serialize_acme_message(acme_message_t *msg, uint8_t *buffer, uint32_t msg_len)
{
    uint8_t *buf_ptr;
    uint32_t i, temp_len;
    buf_ptr = buf;
    unsigned char val = 'A';
    printf("====msg_len is buffer's len :=====%d\n",msg_len);
    // Very first payload is test Magic number, this is  where V2X Family ID would normally be.
    if (msg_len) {
        *buf_ptr++ = msg->v2x_family_id;
        msg_len--;
    }

    // Next-byte is the UE Equipment ID.
    if (msg_len) {
        *buf_ptr++ = msg->equipment_id;
        msg_len--;
    }

    // Only add the timestamp and sequence number of requested length long enough
    if (msg_len >= sizeof(uint16_t)) {
        uint16_t short_seq_num =  htons(msg->seq_num);
        memcpy(buf_ptr, (unsigned char *)&short_seq_num, sizeof(uint16_t));
        buf_ptr += (sizeof(uint16_t));
        msg_len -= (sizeof(uint16_t));
    }

    // Attempt adding timestamp
    char timestamp_format[] = "<%" PRIu64 ">";
    temp_len = snprintf(NULL, 0, timestamp_format, msg->timestamp);
    if (msg_len > temp_len && msg->has_timestamp) {
        buf_ptr += snprintf(buf_ptr, msg_len, timestamp_format, msg->timestamp);
        msg_len -= temp_len;
    }

    // Check if message has kinematics data
    if (msg->has_kinematics) {
        // If there is enough space, copy in all of location data
        char location_format[] = "m=%u (%3.8f,%3.8f) q=%u a=%5.3f ";
        temp_len = snprintf(NULL, 0, location_format,
                            msg->location.fix_mode,
                            msg->location.latitude,
                            msg->location.longitude,
                            msg->location.qty_SV_used,
                            msg->location.altitude);
        if (msg_len > temp_len) {
            buf_ptr += snprintf(buf_ptr, msg_len, location_format,
                                msg->location.fix_mode,
                                msg->location.latitude,
                                msg->location.longitude,
                                msg->location.qty_SV_used,
                                msg->location.altitude
                               );
            msg_len -= temp_len;
        }
        // Attempt to SemiMajorAxisAccuracy
        char Semimajoraxisaccuracy_format[] = "s=%4.2lf ";
        if (msg->location.has_SemiMajorAxisAccuracy) {
            temp_len = snprintf(NULL, 0, Semimajoraxisaccuracy_format,
                                msg->location.SemiMajorAxisAccuracy);
            if (msg_len > temp_len) {
                buf_ptr += snprintf(buf_ptr, msg_len, Semimajoraxisaccuracy_format,
                                    msg->location.SemiMajorAxisAccuracy);
                msg_len -= temp_len;
            }
        }
        // Attempt to add heading
        char heading_format[] = "h=%3.4f ";
        if (msg->location.has_heading) {
            temp_len = snprintf(NULL, 0, heading_format, msg->location.heading);
            if (msg_len > temp_len) {
                buf_ptr += snprintf(buf_ptr, msg_len, heading_format, msg->location.heading);
                msg_len -= temp_len;
            }
        }
        // Attempt to add velocity
        char velocity_format[] = "v=%4.5f ";
        if (msg->location.has_velocity) {
            temp_len = snprintf(NULL, 0, velocity_format, msg->location.velocity);
            if (msg_len > temp_len) {
                buf_ptr += snprintf(buf_ptr, msg_len, velocity_format, msg->location.velocity);
                msg_len -= temp_len;
            }
        }
    }
    // Add data if there is still room left in packet
    for (i = 0; i < msg_len; i++) {//add buffer data here
        *buf_ptr++ = val;
        // val goes from  A-Z, a-z
        val = (val + 1) ;
        if (val > 'Z') {
            val = 'A';    // roll over
        }
    }

    return msg_len;
}
#endif

/*
 * Serialize acme message and copy to buffer.
 * Return the number of bytes copied.
 * @return uint32_t
 */
 //buf_ptr point to global buf, buffer is user's date,msg_len is buffer's len
uint32_t serialize_acme_message(acme_message_t *msg, uint8_t *buffer, uint32_t msg_len)
{
    uint8_t *buf_ptr = NULL;
    uint32_t i, temp_len;
    buf_ptr = buf;
    unsigned char val = 'A';
    printf("====msg_len is buffer's len :=====%d\n",msg_len);
    // Very first payload is test Magic number, this is  where V2X Family ID would normally be.
    *buf_ptr++ = msg->v2x_family_id;

    // Next-byte is the UE Equipment ID.
    *buf_ptr++ = msg->equipment_id;

    // Only add the timestamp and sequence number of requested length long enough
    uint16_t short_seq_num =  htons(msg->seq_num);
    memcpy(buf_ptr, (unsigned char *)&short_seq_num, sizeof(uint16_t));
    buf_ptr += (sizeof(uint16_t));

    // Attempt adding timestamp
  
    char timestamp_format[] = "<%" PRIu64 ">";
    temp_len = snprintf(NULL, 0, timestamp_format, msg->timestamp);
    buf_ptr += snprintf(buf_ptr, MAX_DUMMY_PACKET_LEN-4, timestamp_format, msg->timestamp);
	
	printf("==========line: %d ==========\n",__LINE__);
    #if 0
    // Check if message has kinematics data
    if (msg->has_kinematics) {
        // If there is enough space, copy in all of location data
        char location_format[] = "m=%u (%3.8f,%3.8f) q=%u a=%5.3f ";
        temp_len = snprintf(NULL, 0, location_format,
                            msg->location.fix_mode,
                            msg->location.latitude,
                            msg->location.longitude,
                            msg->location.qty_SV_used,
                            msg->location.altitude);
        if (msg_len > temp_len) {
            buf_ptr += snprintf(buf_ptr, msg_len, location_format,
                                msg->location.fix_mode,
                                msg->location.latitude,
                                msg->location.longitude,
                                msg->location.qty_SV_used,
                                msg->location.altitude
                               );
            msg_len -= temp_len;
        }
        // Attempt to SemiMajorAxisAccuracy
        char Semimajoraxisaccuracy_format[] = "s=%4.2lf ";
        if (msg->location.has_SemiMajorAxisAccuracy) {
            temp_len = snprintf(NULL, 0, Semimajoraxisaccuracy_format,
                                msg->location.SemiMajorAxisAccuracy);
            if (msg_len > temp_len) {
                buf_ptr += snprintf(buf_ptr, msg_len, Semimajoraxisaccuracy_format,
                                    msg->location.SemiMajorAxisAccuracy);
                msg_len -= temp_len;
            }
        }
        // Attempt to add heading
        char heading_format[] = "h=%3.4f ";
        if (msg->location.has_heading) {
            temp_len = snprintf(NULL, 0, heading_format, msg->location.heading);
            if (msg_len > temp_len) {
                buf_ptr += snprintf(buf_ptr, msg_len, heading_format, msg->location.heading);
                msg_len -= temp_len;
            }
        }
        // Attempt to add velocity
        char velocity_format[] = "v=%4.5f ";
        if (msg->location.has_velocity) {
            temp_len = snprintf(NULL, 0, velocity_format, msg->location.velocity);
            if (msg_len > temp_len) {
                buf_ptr += snprintf(buf_ptr, msg_len, velocity_format, msg->location.velocity);
                msg_len -= temp_len;
            }
        }
    }
	#endif
    // Add data if there is still room left in packet
  
    for (i = 0; i < msg_len; i++) {//fill data to global buf 
        *buf_ptr++ = buffer[i];
    }
	printf("==========line: %d ==========\n",__LINE__);
	
	printf("===line :%d...\n", __LINE__);  
    return (msg_len + temp_len + 4);
}



/*
 * Deserialize acme message, copy data from buffer into msg struct.
 * Return the number of bytes remaining in the buffer.
 * @return uint32_t
 */
uint32_t deserialize_acme_message(acme_message_t *msg, uint8_t *buf, uint32_t msg_len)
{
    uint8_t *buf_ptr = buf;
    msg->payload_len = msg_len;
    char *tok;
    char *saveptr;
    char cpy[msg_len + 1];
	printf("===line: %d =====\n",__LINE__);
    // family id
    if (msg_len) {
        msg->v2x_family_id = *buf_ptr++;
        msg_len--;
    }
	printf("===line: %d =====\n",__LINE__);
    // UE equipment id
    if (msg_len) {
        msg->equipment_id = *buf_ptr++;
        msg_len--;
    }
	printf("===line: %d =====\n",__LINE__);
    // Sequence number
    if (msg_len >= 2) {
        msg->seq_num = (ntohs(*(uint16_t *) buf_ptr));
        msg->has_seq_num = true;
        buf_ptr += sizeof(uint16_t);
        msg_len -= sizeof(uint16_t);
    }
	printf("===line: %d =====\n",__LINE__);
    // Attempt to desrialize timestamp
    msg->has_timestamp = false;

    if (msg_len && *buf_ptr == '<') {
        unsigned char *ts_end_p;
        buf_ptr++;
        msg_len--;
        ts_end_p = strstr(buf_ptr, ">");
		printf("===line: %d =====\n",__LINE__);
        if (ts_end_p) {

#if SIM_BUILD
            sscanf(buf_ptr, "%ld", &msg->timestamp);
#else
            sscanf(buf_ptr, "%Ld", &msg->timestamp);
#endif
            msg->has_timestamp = true;
			printf("===line: %d =====\n",__LINE__);

            msg_len -= (ts_end_p + 1) - buf_ptr;
            buf_ptr = ts_end_p + 1;
			
			printf("===line: %d =====\n",__LINE__);
        }
    }

    // Try kinematics data
    msg->has_kinematics = false;
    msg->location.isvalid = false;
	#if 0  //line 1408 case segment fault!!!
    if (msg_len) {
        // Kinematics data is present, deserialize it
        
		printf("===line: %d =====\n",__LINE__);
        memcpy(cpy, buf_ptr, msg_len);
		
		printf("===line: %d =====\n",__LINE__);
        tok = strtok_r(cpy, " ", &saveptr);
		
		printf("===line: %d =====\n",__LINE__);
        while (tok != NULL && strlen(tok) > 0) {
			
			printf("===line: %d =====\n",__LINE__);
            if (tok[0] == 'm') {
                // GNSS mode
                tok += 2;
                sscanf(tok, "%u", &msg->location.fix_mode);
                msg->has_kinematics = true;
                msg->location.isvalid = true;
            } else if (tok[0] == '(') {
                // lat, long
                tok++;
                sscanf(tok, "%lf", &msg->location.latitude);
                sscanf(strchr(tok, ',') + 1, "%lf", &msg->location.longitude);
            } else if (tok[0] == 'q') {
                // SV quantity
                tok += 2;
                sscanf(tok, "%u", &msg->location.qty_SV_used);
            } else if (tok[0] == 'a') {
                // altitude
                tok += 2;
                sscanf(tok, "%lf", &msg->location.altitude);
            } else if (tok[0] == 's') {
                // Semi Major Axis Accuracy
                tok += 2;
                sscanf(tok, "%lf", &msg->location.SemiMajorAxisAccuracy);
                msg->location.has_SemiMajorAxisAccuracy = true;
            } else if (tok[0] == 'h') {
                // heading
                tok += 2;
                sscanf(tok, "%lf", &msg->location.heading);
                msg->location.has_heading = true;
            } else if (tok[0] == 'v') {
                // velocity
                tok += 2;
                sscanf(tok, "%lf", &msg->location.velocity);
                msg->location.has_velocity = true;
            }
            tok = strtok_r(NULL, " ", &saveptr);
        }
    }
	#endif 
    return msg_len;
}

/*******************************************************************************
 * Function: print_buffer
 * Description: Print out a buffer in hex
 * Input Parameters:
 *      buffer: buffer to print
 *      buffer_len: number of bytes in buffer
 ******************************************************************************/
void print_buffer(uint8_t *buffer, int buffer_len)
{
    uint8_t *pkt_buf_ptr;
    int items_printed = 0;

    pkt_buf_ptr = buffer;

    while (items_printed < buffer_len) {
        if (items_printed  && items_printed % 16 == 0) {
            printf("\n");
        }
        printf("%02x ", *pkt_buf_ptr);
        pkt_buf_ptr++;
        items_printed++;
    }
    printf("\n");
}

int sample_rx(int rx_sock)
{
    int rc = 0;
    int flags = 0;
    int len = sizeof(recv_buf);
    int missed_packets;
    struct ethhdr *eth ;
    unsigned char *buf_p;
    unsigned char senders_magic;
    unsigned char senders_ueid;
    uint64_t rx_timestamp = 0;
    uint64_t tx_timestamp = 0;
    int64_t latency = 0;
    uint64_t ipg_us = 0; // Interpacket gap in microseconds
    float per_ue_loss_pct  = 0.f;
    static bool first_packet = true;
    double distance = 0;
    location_data_t location = {0};
    acme_message_t msg = {0};

    if (gVerbosity > 1) {
        printf("RX...\n");
    }
    rc = recv(rx_sock, recv_buf, len, flags);// flags == 0 non block
    rx_timestamp = timestamp_now();
	total_recv_len = rc;
	
    if (first_packet) {
        first_packet = false;
        system("echo \"acme: Received V2X packet\" > /dev/kmsg");
    }

    if (rc < 0) {
        LOGE("Error receiving message!\n");
        goto bail;
    }
    rx_count++;
    buf_p = recv_buf;

    if (do_air2net_proxy) {
        int bytes_proxied = 0;
        int bytes_sent = 0;
        struct msghdr msg = {0};
        struct iovec iov[1] = {0};

        /* Send data using sendmsg to provide IPV6_TCLASS per packet */
        iov[0].iov_base = buf;
        iov[0].iov_len = rc;

        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = 0;
        msg.msg_controllen = 0;

        if ((bytes_proxied = sendmsg(forward_net_socket, &msg, 0)) < 0) {
            LOGE("error on air2net sendmsg()\n");
        }

        if (gVerbosity > 1) {
            printf("[proxied %d]", bytes_proxied);
        }
    } else {

        // Total Rx packet count (not per RV)
        if (gVerbosity) {
            printf("===|#%-4d|l=%3d|===\n", rx_count, rc);
        }
		printf("===line: %d =====\n",__LINE__);
        user_data_len = deserialize_acme_message(&msg, buf_p, rc);//user_data_len is  - headers len { 1+1+2+timestamp}
        senders_magic = msg.v2x_family_id;
        senders_ueid = msg.equipment_id;
		printf("from obu data_len is:%d \n",user_data_len);
		//user_data_to_server_len = user_data_len;
		
		printf("=====line: %d =====\n",__LINE__);
		//sem_post(&buf_number);
		send_to_stm32_flag = 1;
		
		printf("=====line: %d =====\n",__LINE__);
        if (gVerbosity > 2) {
            char magic_ver_str[4];
            magic_ver_str[0] = 0;

            if (senders_magic >= '1' && senders_magic <= 'z') {
                snprintf(magic_ver_str, sizeof(magic_ver_str), "(%c)", senders_magic);
            }

            fprintf(stdout, "Ver=%d%s|", senders_magic, magic_ver_str);
        }

        if (gVerbosity) {
            fprintf(stdout, "UE#%d|",
                    senders_ueid);
        }

        buf[rc + 1 ] = 0 ; // null terminate the string.
		printf("=====line: %d =====\n",__LINE__);
        // FIXME -- could add flag to force seq#/timestamp checking, even if verno mismatch
        if (senders_magic == g_testverno_magic) {
            uint16_t  seq_num = 0;

            rv_rx_cnt[senders_ueid]++; // Per RV Count incremented.
            rv_presumed_sent[senders_ueid]++; // Per RV TX count increment.
            seq_num = msg.seq_num;

            // only tabulate lost/missed sequence after the first packet
            if (rv_rx_cnt[senders_ueid] > 1) {
                missed_packets = seq_num - rv_seq_num[senders_ueid] - 1;

                // 16 bit roll-over can be ignored
                if (missed_packets < 0) {
                    if (missed_packets == -1) {
                        LOGE("ERR: Duplicated seq_num packet from RV#%d\n", senders_ueid);
                        missed_packets = 1; //We'll count as a single error
                        rv_presumed_sent[senders_ueid]--; // don't count duplicates as sent
                    } else {
                        // If Way, way large neg number, then seq# rolled over
                        if (missed_packets < -32768)  {
                            // these two additions will result in 0, if roll-over was 65536 -> 0
                            missed_packets = seq_num ;
                            missed_packets += 65535 - rv_seq_num[senders_ueid] ;
                            rv_presumed_sent[senders_ueid] += missed_packets; //could be zero
                        } else {
                            // Consider any sequence # within 100 an out-of order packet
                            if (missed_packets < -20) {
                                rv_seq_start_num[senders_ueid] = seq_num ;
                                fprintf(stderr, "# RV restarted, sequence jumped \n");
                                missed_packets = 1;
                            } else {
                                missed_packets++; // compensate for -1 added initially
                                missed_packets *= -1; //change sign to positive.
                                fprintf(stderr, "# ERR: Out of order sequence by %d \n", missed_packets);
                                g_out_of_order_packets++;
                            }
                        }
                    }
                } else {
                    // Boring positive number of missed packets.
                    rv_presumed_sent[senders_ueid] += missed_packets; //could be zero
                }

                if (gVerbosity > 1) {
                    fprintf(stdout, "#%d|seq#%d|", rv_rx_cnt[senders_ueid],
                            seq_num);
                }

                if (missed_packets > 0) {
                    rv_missed_packets[senders_ueid] += missed_packets;
                    g_missed_packets += missed_packets;
                    per_ue_loss_pct = rv_missed_packets[senders_ueid] * 100.0 ;
                    per_ue_loss_pct /= (float)rv_presumed_sent[senders_ueid];

                    if (gVerbosity)
                        if (gVerbosity)
                            fprintf(stderr, "# Err: %d missed, per UE: %d lost(%8.1f%%)\n",
                                    missed_packets,
                                    rv_missed_packets[senders_ueid],
                                    per_ue_loss_pct
                                   );
                }
            } else {
                if (gVerbosity > 1) {
                    printf("RV #%d seq# start initalized at %d\n", senders_ueid, seq_num);
                }
                rv_seq_start_num[senders_ueid] = seq_num ;
                min_latency[senders_ueid] = 99999999999;
                max_latency[senders_ueid] = 0.0;
                latency_sum[senders_ueid] = 0.0;
                avg_latency[senders_ueid] = 0.0;
            }

            per_ue_loss_pct = rv_missed_packets[senders_ueid] * 100.0 ;
            per_ue_loss_pct /= (float)rv_presumed_sent[senders_ueid];

            /* For each RV, record the last received sequence number */
            rv_seq_num[senders_ueid] = seq_num;

            // If message contains a timestamp
            if (msg.has_timestamp) {

                if (gVerbosity > 2) {
                    printf("[has ts brackets]");
                }

                tx_timestamp = msg.timestamp;

                if (gVerbosity > 2) {
                    printf(" senders stamp:<%" PRIu64 "> ", tx_timestamp);
                }

                latency = rx_timestamp - tx_timestamp;

                if (latency < 0) {
                    g_neg_latency_warns++;
                } else if (latency < 50000000) { // If latency too large, over 500ms, then throw-out

                    // Set minimum and maximum latency
                    if (latency < min_latency[senders_ueid]) {
                        min_latency[senders_ueid] = latency;
                    }

                    if (latency > max_latency[senders_ueid]) {
                        max_latency[senders_ueid] = latency;
                    }

                    latency_sum[senders_ueid] += latency;
                    avg_latency[senders_ueid] = latency_sum[senders_ueid] /  rv_rx_cnt[senders_ueid];
                    overall_sum_latency += latency;
                    overall_avg_latency = overall_sum_latency / rx_count;
                }

                if (gVerbosity) {
                    printf("<latency=%6.2f ms> ", (double) latency / 1000.0);
                    if (latency < 0) {
                        printf("!!! WARN NEG LATENCY !!!  ");
                    }
                }

                if (rv_rx_cnt[senders_ueid] > 1) {
                    ipg_us = rx_timestamp - rv_rx_timestamp[senders_ueid];
                    if (gVerbosity) {
                        printf("<ipg=%6.2f ms>", (double)ipg_us / 1000.0);
                    }
                }
                rv_rx_timestamp[senders_ueid] = rx_timestamp;
            }
        }
		printf("=====line: %d =====\n",__LINE__);
        // Calculate the distance if the necessary data is available
        if (kinematics_data.inuse) {
            load_location_data(&location, &kinematics_data);
			
			printf("=====line: %d =====\n",__LINE__);
            // Calculate distance and write to file if necessary data is available
            if (location.isvalid && msg.has_kinematics && msg.location.isvalid) {
                distance = calculate_distance(&msg.location, &location);
            } else {
                distance = 0;
            }
        }

        if (ascii_dump_opt) {
            printf("[%s] ", buf_p);
        }

        if (gVerbosity || dump_opt) {
            printf("|total missed=%d| per UE lost/sent=|%d|%d|%8.1f%%|",
                   g_missed_packets,
                   rv_missed_packets[senders_ueid],
                   rv_presumed_sent[senders_ueid],
                   per_ue_loss_pct
                  );
            if (distance) {
                // Print distance if its valid
                printf(" Distance=%.3lf|", distance);
            }
        }
        if (gVerbosity > 3) {
            printf("\n");
            print_acme_message(&msg);
        }

        if (g_ml_fp) {
            write_receiver_data(g_ml_fp, rx_timestamp, &msg, &location, distance, latency,
                                per_ue_loss_pct, ipg_us);
            fflush(g_ml_fp);
        }

    } // Not a Proxy packet

    if (dump_opt) {
        printf("===receive: \n");
        print_buffer(recv_buf, rc);
    }

    if (ascii_dump_opt || dump_opt || gVerbosity) {
        printf("\n");
    }

    rx_len = rc;

bail:
    return rc;
}

uint8_t *demo_payload_tx_setup(uint8_t *buf,
                               uint16_t seq_num,
                               uint32_t demo_len,
                               uint64_t timestamp,
                               uint8_t *payload)
{
    uint8_t *buf_ptr, val;
    uint32_t i, temp_len;

    buf_ptr = buf;//buf_ptr is not used in this func, and buf is filled in serialize_acme_message
    acme_message_t msg = {0};

    /* Copy in supplied payload or generate the payload, if its not already
       supplied through ptr + payload_len */
    #if 0
    if (payload) {
        memcpy(buf_ptr, payload, demo_len);
        buf_ptr += demo_len;
    } else 
    #endif
    {
        // Very first payload is test Magic number, this is  where V2X Family ID would normally be.
        msg.v2x_family_id = g_testverno_magic;
        // Next-byte is the UE Equipment ID.
        msg.equipment_id = g_ueid;
        msg.seq_num =  seq_num;
        msg.has_seq_num = true;

        if (timestamp) {
            msg.has_timestamp = true;
            msg.timestamp = timestamp;
        }

        msg.has_kinematics = false;
		printf("==========line: %d ==========\n",__LINE__);
        // If kinematics data is in use, add it to msg
        if (kinematics_data.inuse) {
			
			printf("=======   %d  ========\n",__LINE__);
            if (load_location_data(&msg.location, &kinematics_data)) {
				printf("=======   %d  ========\n",__LINE__);
                msg.has_kinematics = true;
            }
        }

        if (g_ml_fp) {
            write_message_to_file(g_ml_fp, &msg);
            fprintf(g_ml_fp, "\n");
            fflush(g_ml_fp);
        }

        if (gVerbosity > 3) {
            print_acme_message(&msg);
        }
        //demo_len = serialize_acme_message(&msg, buf, demo_len);
        send_len = serialize_acme_message(&msg, payload, demo_len);///len of payload == demo_len
		printf("===line :%d...\n", __LINE__);  

	}
    // For more advanced testing, you could add a CRC here
    return buf_ptr;
}

int sample_tx(int socket, int n_bytes)
{
    int bytes_sent = 0;
    struct msghdr message = {0};
    struct iovec iov[1] = {0};
    struct cmsghdr *cmsghp = NULL;
    char control[CMSG_SPACE(sizeof(int))];


    /* Send data using sendmsg to provide IPV6_TCLASS per packet */
    iov[0].iov_base = buf;
    iov[0].iov_len = n_bytes;
    message.msg_name = &dest_sockaddr;
    message.msg_namelen = sizeof(dest_sockaddr);
    message.msg_iov = iov;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);

    /* Fill ancillary data */
    cmsghp = CMSG_FIRSTHDR(&message);
    cmsghp->cmsg_level = IPPROTO_IPV6;
    cmsghp->cmsg_type = IPV6_TCLASS;
    cmsghp->cmsg_len = CMSG_LEN(sizeof(int));
    *((int *)CMSG_DATA(cmsghp)) = v2x_convert_priority_to_traffic_class(event_tx_prio);

    bytes_sent = sendmsg(socket, &message, 0);

    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending message: %d\n", bytes_sent);
        bytes_sent = -1;
    } else {
        if (bytes_sent == n_bytes) {
            tx_count++;
        } else {
            printf("TX bytes sent were short\n");
        }
    }

    if (gVerbosity || (dump_opt && bytes_sent > 0)) {
        printf("TX count: %d, len = %d\n", tx_count, bytes_sent);

        if (dump_opt) {
            print_buffer(buf, bytes_sent);
        }
    }

    return bytes_sent;
}

void thread_recv_func(void)
{
while(1){
	printf("==line %d ======\n", __LINE__);

	sample_rx(rx_sock);
	
	printf("==line %d ======\n", __LINE__);
}
}

void close_all_v2x_sockets(void)
{
    int i;

    if (gVerbosity) {
        printf("closing all V2X Sockets \n");
    }
	printf("==line %d ======\n", __LINE__);
    if (rx_sock >= 0) {
        v2x_radio_sock_close(&rx_sock);
    }

    for (i = 0; i < MAX_FLOWS; i++) {
        if (flow_type[i]) {
            printf("closing Flow#%d: type=%d %d %d sps_port=%d evt_port=%d, %d ms, %d bytes\n",
                   i,
                   flow_type[i],
                   sps_sock[i],
                   event_sock[i],
                   sps_port[i],
                   evt_port[i],
                   demo_res_req[i].period_interval_ms,
                   demo_res_req[i].tx_reservation_size_bytes
                  );

            if (sps_sock[i] >= 0) {
                v2x_radio_sock_close(sps_sock + i);
            }
            if (event_sock[i] >= 0) {
                v2x_radio_sock_close(event_sock + i);
            }
        }
    }
	printf("==line %d == =====\n", __LINE__);
}

void termination_handler(int signum)
{
    int i;
    printf("Got signal %d, tearing down all services\n", signum);

    close_all_v2x_sockets();

    if (handle != V2X_RADIO_HANDLE_BAD) {
        v2x_radio_deinit(handle);
    }

    ShowResults(stdout);

    signal(signum, SIG_DFL);
    raise(signum);
}

void install_signal_handler()
{
    struct sigaction sig_action;

    sig_action.sa_handler = termination_handler;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;

    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGHUP, &sig_action, NULL);
    sigaction(SIGTERM, &sig_action, NULL);
}

/*
   print_rx_deltas  to allow exteral triggering of packet count printing
   called from signal handler at an interval (1 sec default) specified on commandline
*/
static void print_rx_deltas(int signum)
{
    int i;
    int delta;
    double delta_t, pps;
    static int prev_count = 0;
    static double prev_time = 0;
    long long t_now_ms = 0;
    int active_RV_cnt = 0;
    static uint32_t prev_cnt[MAX_UEID] = {0}; // Keep track of total packets at this point on previous interval
    v2x_event_t recent_status;
    uint64_t age_usec;

    if (gVerbosity > 3) {
        recent_status = cv2x_status_poll(&age_usec);
        printf("|status=%dm %d ms ago|", recent_status, (int)(age_usec / 1000));
    }

    // Only bother with the RX reports, if actually receiving
    if (g_status == V2X_ACTIVE || g_status == V2X_TX_SUSPENDED) {

        t_now_ms = timestamp_now();

        for (i = 0; i < MAX_UEID; i++) {
            if (rv_rx_cnt[i]) {
                if (prev_cnt[i] != rv_rx_cnt[i]) {
                    active_RV_cnt ++;
                }

                prev_cnt[i] = rv_rx_cnt[i];
            }
        }

        if (prev_time > 0) {
            delta = rx_count - prev_count;
            delta_t = t_now_ms - prev_time;
            pps = ((double)delta * 1000000.0) / delta_t;
            if (gVerbosity) {
                printf("<%" PRIu64 "> | %4d | +%4d packets | %6.2f packets per second (PPS)| %3.2f ms avg latency | RV Count=%d | CBP=%3.0f\%\n",
                       (unsigned long int) t_now_ms,
                       rx_count,
                       delta,
                       pps,
                       overall_avg_latency / 1000,
                       active_RV_cnt,
                       g_cbp_meas);
            }
            prev_count = rx_count;
        } else {
            // Print a banner on first run
            if (gVerbosity)
                printf(" Epoch-ms     |  Tot-pkts   | New-pkts  |   PPS  | Latency | RV's | CBP %\n");
        }

        prev_time = t_now_ms;
    }
}

void setup_periodic_reports(void)
{
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    int timer_signo = SIGUSR1;

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = timer_signo;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        errExit("timer_create");
    }

    /* Start the timer */

    its.it_value.tv_sec = gReportInterval_ns / 1000000000;
    its.it_value.tv_nsec = gReportInterval_ns % 1000000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        errExit("timer_settime");
    }

    print_rx_deltas(0); // call once now to print header and initialze delta timers

}

static void v2x_kinematics_init_cb(v2x_status_enum_type status, void *context)
{
    if (V2X_STATUS_SUCCESS == status) {
        if (gVerbosity > 2) {
            printf("v2x callback - initialized successfully. \n");
        }
        v2x_kinematics_initialized = true;
    }
}

static void v2x_kinematics_newfix_cb(v2x_location_fix_t *new_fix, void *context)
{
    static uint32_t fix_count = 0u;
    // Don't do anything if SVs used 0.
    if (V2X_GNSS_MODE_NO_FIX == new_fix->fix_mode) {
        return;
    }
	printf("====v2x_kinematics_newfix_cb %d=====\n", __LINE__);
    kinematics_data.has_fix = true;
    kinematics_data.latest_fix = new_fix;
    kinematics_data.timestamp = timestamp_now();
    if (new_fix->utc_fix_time) {
        ++numFixesReceived;
    }
}

static void v2x_kinematics_final_cb(v2x_status_enum_type status, void *context)
{
    if (gVerbosity > 3) {
        printf("v2x final callback, status=%d\n", status);
    }
}

/*
 * Try to initialize the kinematics client, called on timer trigger.
 * If initialized, register the callback.  Disarm timer if initialization
 * is successful.
 */
static void try_kinematics_initialize(int signum)
{

    v2x_init_t v2x_init;
    v2x_init.log_level_mask = 0xffffffff;
    struct itimerspec its;

    if (gVerbosity) {
        printf("Kinematics server IP address is %s\n", server_ip_str);
    }

    snprintf(v2x_init.server_ip_addr, sizeof(v2x_init.server_ip_addr), "%s", server_ip_str);

    static uint32_t retryCount = 0u;
    if (!v2x_kinematics_initialized) { // Try initialize
        // Deallocate handle if it failed to initialize previously
        if (V2X_KINEMATICS_HANDLE_BAD != h_v2x) {
            v2x_kinematics_final(h_v2x, &v2x_kinematics_final_cb, NULL);
        }
        if (retryCount) {
            if (gVerbosity) {
                printf("Retrying ... \n");
            }
        } else {
            if (gVerbosity) {
                printf("Initializing Kinematics API.\n");
            }
        }
        h_v2x = v2x_kinematics_init(&v2x_init, v2x_kinematics_init_cb, NULL);
        ++retryCount;
		
        printf("====v2x_kinematics_init %d=====\n",__LINE__);
    }
	printf("====v2x_kinematics_initialized:%d line:%d=====\n",v2x_kinematics_initialized,__LINE__);
    if (v2x_kinematics_initialized) {
        // The Kinematics API has been initialized
        if (gVerbosity) {
            printf("Kinematics API initialization successful.\n");
        }
		
        // Register the new_fix listener
        if (V2X_STATUS_SUCCESS ==
                v2x_kinematics_register_listener(h_v2x, v2x_kinematics_newfix_cb, NULL)) {
            if (gVerbosity) {
                printf("Kinematics listener registration successful.\n");
            }
			// Disarm timer
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_nsec = 0;
            its.it_interval.tv_sec = 0;
            if (timer_settime(init_kinematics_timer, 0, &its, NULL) == -1) {
                errExit("disarm_timer");
            }
        } else {
            if (gVerbosity) {
                printf("Kinematics listener registration unsuccessful.\n");
            }
        }
    }
}

/*
 * Setup initializing of the kinematics client.  Starts a timer to trigger intitialization attempts,
 * will retry until successful completion.
 */

void start_kinematics()
{
    /* Initialize the kinematics API */
    struct itimerspec its;
    struct sigevent sev;
    // Setup up the timer and signal
    init_kinematics_timer_signo = SIGUSR2;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = init_kinematics_timer_signo;
    sev.sigev_value.sival_ptr = &init_kinematics_timer;
    if (timer_create(CLOCK_REALTIME, &sev, &init_kinematics_timer) == -1) {
        errExit("timer_create");
    }

    signal(init_kinematics_timer_signo, try_kinematics_initialize);

	
    /* Start the timer */
    its.it_value.tv_sec =  1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(init_kinematics_timer, 0, &its, NULL) == -1) {
        errExit("arm_timer");
    }

    try_kinematics_initialize(0); // call once now to print header and initialze delta timers
}

/*******************************************************************************
 * Signal handler to print statistics.
 *
 * @param sig_num
 ******************************************************************************/
static
void acme_tx_sigint_handler(int sig_num)
{
    printf("sent: %d packets, received: %d,  signal %d\n", tx_count, rx_count, sig_num);
}




//acme -G 127.0.0.1
void main(void)
{
    int i; // packet loop counter
    v2x_concurrency_sel_t mode = V2X_WWAN_NONCONCURRENT;
    unsigned long  test_ctx = 0xfeedbeef; // Just a dummy test context to make sure is maintained properly
    uint8_t pseudo_mac[8] = {0x70, 0xb3, 0xd5, 0x04, 0x00, 0xe6};
	char *argv[] ={"aaa", "-gG", "192.168.100.1"};//add -g to print gps use gps
	//char *argv[] ={"aaa", "-gG", "127.0.0.1"};//add -g to print gps
    v2x_api_ver_t  verinfo;
    int tx_sock;
    uint64_t timestamp = 0;
    int traffic_class = -1;
    int signal_which_alarmed = 0;
    sigset_t alarm_wait;  // A signal set used for the timer waits for synchronous packet generation
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    sigset_t mask;
    int using_tx_timer = 1; // Assume using timer based transmit period unless otherwise determined
    int log_level;
    int paynum = 0;
    int timer_signo = SIGRTMIN;
    struct sockaddr_in *ipv4;
    struct addrinfo *result = NULL;
    char ip[INET_ADDRSTRLEN];
	int ret;
	int argc = 3;		
    int  err;
	pthread_t rabbit;	
	pthread_t rabbit1;	
	pthread_t rabbit2;
	pthread_t rabbit3;
	
	pthread_attr_t attr;	
	pthread_attr_t attr1;	
	pthread_attr_t attr2;
	pthread_attr_t attr3;
	
	pthread_attr_init(&attr);
	pthread_attr_init(&attr1);	
	pthread_attr_init(&attr2);
	pthread_attr_init(&attr3);
	
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);	
	if((err=pthread_create(&rabbit,NULL,pthread_protobuf_func,(void *)0))!=0)
	{
		perror("pthread_create pthread_protobuf_func  error");
	}

	pthread_attr_setdetachstate(&attr1,PTHREAD_CREATE_DETACHED);	
	if((err=pthread_create(&rabbit1,NULL,pthread_sendto_cloud,(void *)0))!=0)
	{
		perror("pthread_create pthread_sendto_cloud  error");
	}

#ifdef HIK_TRAFFIC
	pthread_attr_setdetachstate(&attr1,PTHREAD_CREATE_DETACHED);	
	if((err=pthread_create(&rabbit3,NULL,pthread_stm32_date_func,(void *)0))!=0)
	{
		perror("pthread_create pthread_stm32_date_func  error");
	}
	printf("pthread_create pthread_stm32_date_func success!!!\n");
#endif

    /* Block timer signal temporarily */
    printf("Blocking signal %d\n", timer_signo);
    sigemptyset(&mask);
    sigaddset(&mask, timer_signo);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        errExit("sigprocmask");
    }
    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = timer_signo;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        errExit("timer_create");
    }

    sigemptyset(&(alarm_wait));
    sigaddset(&(alarm_wait), timer_signo);
    signal(SIGINT, acme_tx_sigint_handler);
    signal(SIGUSR1, print_rx_deltas);

    install_signal_handler();

    radio_calls.v2x_radio_init_complete = init_complete;
    radio_calls.v2x_radio_status_listener = radio_listener;
    radio_calls.v2x_radio_chan_meas_listener =  meas_listener;
    radio_calls.v2x_radio_l2_addr_changed_listener = l2_changed_listener;

    /* parse config file if it exists */
   /// parse_config();    //      "/data/acme.conf"  to get config info
    parse_opts(argc, argv);//  if input -R ,moidfy test_mode to  ModeRx, default is test_mode==ModeTx

    /* Convert server hostname to ip */
    ret = getaddrinfo(server_ip_str, NULL, NULL, &result);
    if (ret || !result) {
        printf("Unable to resolve hostname %s", server_ip_str);
        return -ENXIO;
    }

    ipv4 = (struct sockaddr_in *)result->ai_addr;
    inet_ntop(AF_INET, &(ipv4->sin_addr), ip, INET_ADDRSTRLEN);//ip ==127.0.0.1 
    g_strlcpy(server_ip_str, ip, strlen(ip) + 1);//server_ip_str ==127.0.0.1 
    freeaddrinfo(result);

    if (gVerbosity == 0) {//gVerbosity == 3
        log_level = LOG_ERR;
    } else if (gVerbosity == 1) {
        log_level = LOG_NOTICE;
    } else if (gVerbosity == 2) {
        log_level = LOG_INFO;
    } else {
        log_level = LOG_DEBUG;
    }
    v2x_radio_set_log_level(log_level, g_use_syslog);//log_level  7
	
	
    if (g_ml_fn) {
        if (strcmp(g_ml_fn, "stdout") == 0) {
            g_ml_fp = fdopen(STDOUT_FILENO, "w+") ;
        } else if (strcmp(g_ml_fn, "stderr") == 0) {
            g_ml_fp = fdopen(STDERR_FILENO, "w+") ;
        } else {
            // Note: O_TRUNC not specified, since we might want to read from file until new data arrives
            g_ml_fp = fopen(g_ml_fn, "w") ;
            if (! g_ml_fp) {
                LOGE("Error opening the output file.\n");
                goto exit;
            }
        }
    }

    verinfo = v2x_radio_api_version();
    LOGI("API Version#%d, built on <%s> @ %s \n", verinfo.version_num,
         verinfo.build_date_str,
         verinfo.build_time_str);
#if 0
    if (test_mode != ModeTx) { //default is ModeTx, if no iput -R
        if (gVerbosity > 2) {
            LOGI("Skipping set-up of tx interval timer, since RX or Echo mode.\n");
        }
        using_tx_timer = 0;

        if (gReportInterval_ns) {
            setup_periodic_reports();
        }

    }
    if (do_net2air_proxy) {// 0
        using_tx_timer = 0 ;
    }
#endif
    // If not special dual-priority Case, use "-o" priority for Event
    if (FlowCnt < MAX_FLOWS) {//FlowCnt 0
        event_tx_prio =  demo_res_req[FlowCnt].priority ;
    }

    g_curr_test_str = g_test_mode_names[test_mode];

    if (!using_tx_timer) {
        interval_btw_pkts_ns = 0;
    }

    /* network => air Uplink Proxy */
#if 0
    if (do_net2air_proxy) {

        memset(&proxy_listen_sin, 0, sizeof(struct sockaddr_in6));

        if ((listen_net_socket = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)  {
            fprintf(stderr, "%s() cannot create socket", __FUNCTION__);
            exit(3);
        }

        proxy_listen_sin.sin6_addr = in6addr_any;
        proxy_listen_sin.sin6_family = AF_INET6;
        proxy_listen_sin.sin6_scope_id = 0;
        proxy_listen_sin.sin6_port = htons((uint16_t)proxy_net2air_listen_port);

        if (bind(listen_net_socket, (struct sockaddr *)&proxy_listen_sin, sizeof(proxy_listen_sin)) < 0) {
            fprintf(stderr, "Error binding proxy listen socket,  err = %s\n", strerror(errno));
        }
    }
#endif
#if 0
    if (do_air2net_proxy) {// 0
        struct in_addr ip_address;
        struct addrinfo hint;
        struct addrinfo *details_p = NULL;
        int family;

        memset(&hint, 0, sizeof(hint));
        hint.ai_flags = AI_NUMERICHOST;
        hint.ai_family = PF_UNSPEC;

        if (getaddrinfo(proxy_remote_net_address, NULL, &hint, &details_p)) {
            fprintf(stderr, "Bad Air2net remote IP address %s.\n", proxy_remote_net_address);
            exit(3);
        }

        family = details_p->ai_family;

        if ((forward_net_socket = socket(family, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
            fprintf(stderr, "Proxy to network requested, socket failed.\n");
            exit(3);
        }

        /* vehicle => network */
        switch (family) {
        case AF_INET:
            printf("Destination address is IPv4\n");
            memset((void *)&proxy_remote_sin4, 0, sizeof(proxy_remote_sin4));
            proxy_remote_sin4.sin_family = family;
            proxy_remote_sin4.sin_addr.s_addr = htonl(INADDR_ANY);
            proxy_remote_sin4.sin_port = htons((uint16_t)proxy_air2net_forward_to_port);

            if (setsockopt(forward_net_socket, SOL_SOCKET, SO_BINDTODEVICE, (void *)proxy_if_name, strlen(proxy_if_name)) < 0) {
                fprintf(stderr, "Error setting socket options, err = %s\n", strerror(errno));
                exit(0);
            }

            /* Sending received packets to: proxy_ip_addr proxy_air2net_forward_to_port */
            if (bind(forward_net_socket, (struct sockaddr *)&proxy_remote_sin4, sizeof(proxy_remote_sin4)) < 0) {
                fprintf(stderr, "Error binding sps socket, err = %s\n", strerror(errno));
            } else  {
                printf("SPS Socket setup success fd=%d, port=%d\n", forward_net_socket, ntohs(proxy_remote_sin.sin6_port));
            }

            inet_pton(family, proxy_remote_net_address, &proxy_remote_sin4.sin_addr.s_addr);

            // Set default destination address
            if (connect(forward_net_socket, (struct sockaddr *)&proxy_remote_sin4, sizeof(struct sockaddr_in)) < 0) {
                LOGE("Error connecting sps socket to default address, err = %s\n", strerror(errno));
                exit(4);
            } else {
                printf("Successfully setup default destination for sps socket | Errno = %d\n", errno);
            }
            break;
        case AF_INET6:
            printf("Destination address is IPv6\n");
            proxy_remote_sin.sin6_addr = in6addr_any;
            proxy_remote_sin.sin6_port = htons((uint16_t)proxy_air2net_forward_to_port);
            proxy_remote_sin.sin6_family = AF_INET6;
            proxy_remote_sin.sin6_scope_id = if_nametoindex(proxy_if_name);

            if (setsockopt(forward_net_socket, SOL_SOCKET, SO_BINDTODEVICE, (void *)proxy_if_name, strlen(proxy_if_name)) < 0) {
                fprintf(stderr, "Error setting socket options, err = %s\n", strerror(errno));
                exit(0);
            }

            /* Sending received packets to: proxy_ip_addr proxy_air2net_forward_to_port */
            if (bind(forward_net_socket, (struct sockaddr *)&proxy_remote_sin, sizeof(struct sockaddr)) < 0) {
                fprintf(stderr, "Error binding sps socket, err = %s\n", strerror(errno));
            } else  {
                printf("SPS Socket setup success fd=%d, port=%d\n", forward_net_socket, ntohs(proxy_remote_sin.sin6_port));
            }

            inet_pton(family, proxy_remote_net_address, &proxy_remote_sin.sin6_addr);

            // Set default destination address
            if (connect(forward_net_socket, (struct sockaddr *)&proxy_remote_sin, sizeof(struct sockaddr_in6)) < 0) {
                LOGE("Error connecting sps socket to default address, err = %s\n", strerror(errno));
                exit(4);
            } else {
                LOGI("Successfully setup default destination for sps socket | Errno = %d\n", errno);
            }
            break;
        default:
            LOGE("WARNING. address family of specified address not recognized.\n");
        }
    } // do_air2net_proxy
#endif
    // Init will fail if CV2X mode is "INACTIVE" but not if just suspended due to timing.
    handle = v2x_radio_init(iface, mode, &radio_calls, &test_ctx);
    if (handle == V2X_RADIO_HANDLE_BAD || handle >= V2X_MAX_RADIO_SESSIONS) {
        LOGE("Error initializing the V2X radio, bail\n");
        exit(2);
    }
    radio_query_and_print_param(iface);
    v2x_test_param_check(&gCapabilities);

    /* Disable the socket connection */
    v2x_disable_socket_connect();
    /* Set destination information */
    inet_pton(AF_INET6, dest_ipv6_addr_str, (void *)&dest_sockaddr.sin6_addr);
    dest_sockaddr.sin6_family = AF_INET6;
    dest_sockaddr.sin6_scope_id = if_nametoindex(iface);

    // If kinematics processing is in use, initialize the client.
    if (kinematics_data.inuse) {
        printf("Initialize kinematics client\n");
        start_kinematics();
    }
    v2x_radio_set_log_level(log_level, g_use_syslog);

    // Command line argument(CLA) for portnumber could be either
    if (cla_portnum) {

        // Transmitting, so treat the CLA as a destination port.
        if (test_mode != ModeRx) {
            v2x_set_dest_port(cla_portnum);

			 v2x_set_rx_port(cla_portnum);///hxq add for support rx ,if not use, please delete
        } else {
        
            v2x_set_rx_port(cla_portnum);
        }
    }
	
    if (test_mode != ModeRx) {//default is ModeTx
        int f;
        int i;
        int socknum = 0;
        struct sockaddr_in6 sockaddr;
        int esocknum = 0;
        struct sockaddr_in6 esockaddr = {0};

        // If this is not a RX client, configure the dest IPV6 addr & dest port (if requested)
        v2x_set_dest_ipv6_addr(dest_ipv6_addr_str);//after set , dest_ipv6_addr_str is  ff02::1
        /* Set default TCLASS on event socket */
        traffic_class = v2x_convert_priority_to_traffic_class(event_tx_prio);
        fprintf(stdout, "# traffic_class=%d\n", traffic_class);//traffic_class = 3

        for (f = 0; f <= FlowCnt; f++) {
            switch (flow_type[f]) {
            case EventOnly:

                if (v2x_radio_tx_event_sock_create_and_bind(iface, demo_res_req[f].v2xid,
                        evt_port[f], &esockaddr, &esocknum) != V2X_STATUS_SUCCESS) {
                    LOGE("Error creating Event socket\n");
                    exit(2);
                }
                event_sock[f] = esocknum;
                event_sockaddr[f] = esockaddr;

                break;

            case SpsOnly:
                if (v2x_radio_tx_sps_sock_create_and_bind(handle, &demo_res_req[f],
                        &sps_function_calls, sps_port[f], 0, &socknum,
                        &sockaddr, NULL, NULL) != V2X_STATUS_SUCCESS) {
                    fprintf(stderr, "Error creating SPS socket\n");
                    exit(2);
                }
                sps_sock[f] = socknum;
                sps_sockaddr[f] = sockaddr; //Shallow struc copy
                SpsFlowCnt++;
                break;

            case ComboReg:
                if (v2x_radio_tx_sps_sock_create_and_bind(handle, &demo_res_req[f],
                        &sps_function_calls, sps_port[f], evt_port[f],
                        &socknum,
                        &sockaddr,
                        &esocknum,
                        &esockaddr
                                                         ) != V2X_STATUS_SUCCESS) {
                    fprintf(stderr, "Error creating SPS socket\n");
                    exit(2);
                }
                sps_sock[f] = socknum;
                sps_sockaddr[f] = sockaddr;
                event_sock[f] = esocknum;
                event_sockaddr[f] = esockaddr;
                SpsFlowCnt++;
                break;

            case NoReg:
            default:
                fprintf(stderr, "WARNING:  unprocessed TX Flow Type");

            }

            if (esocknum) {
                if (setsockopt(esocknum, IPPROTO_IPV6, IPV6_TCLASS,
                               (void *)&traffic_class, sizeof(traffic_class)) < 0) {
                    fprintf(stderr, "setsockopt(IPV6_TCLASS) on event socket failed err=%d\n", errno);
                } else {
                    printf("Setup traffic class=%d on the event socket completed.\n", traffic_class);
                }
            }

            if (socknum) {
                if (setsockopt(socknum, IPPROTO_IPV6, IPV6_TCLASS,
                               (void *)&traffic_class, sizeof(traffic_class)) < 0) {
                    fprintf(stderr, "setsockopt(IPV6_TCLASS) on SPS socket failed err=%d\n", errno);
                } else {
                    printf("Setup traffic class =%d completed the SPS flow socket.\n", traffic_class);
                }
            }

        }

        for (i = 0; i <= FlowCnt; i++) {//tx size bytes ==287
            printf("Flow#%d: type=%d %d %d sps_port=%d evt_port=%d, %d ms, %d bytes\n",
                   i,
                   flow_type[i],
                   sps_sock[i],
                   event_sock[i],
                   sps_port[i],
                   evt_port[i],
                   demo_res_req[i].period_interval_ms,
                   demo_res_req[i].tx_reservation_size_bytes
                  );
        }

    }
    if (SpsFlowCnt < 2) {//SpsFlowCnt 1
        if ((flow_type[0] == EventOnly) || sps_event_opt || SpsFlowCnt < 1) {
            tx_sock = event_sock[0];
            dest_sockaddr.sin6_port = htons((uint16_t)evt_port[0]);
            if (gVerbosity) {
                printf("Event Socket: ");
            }
        } else {
            tx_sock = sps_sock[0];
            dest_sockaddr.sin6_port = htons((uint16_t)sps_port[0]);
        }
    } else {
        // So its an SPS Flow,  If we only have one flow set-up, choice is easy
        // if there are more, we'll do it based on a counting pattern or packet size
        // depending on what commandline args were used,
        // Exactly which flow is determined later, per packet
        // However, we set-up here and now, incase this is an Echo use case

        tx_sock = sps_sock[0];
        dest_sockaddr.sin6_port = htons((uint16_t)sps_port[0]);
    }

    //   if (test_mode != ModeTx) {
        if (1) {///modify  by hxq for rx, because sample_rx need rx sock and bind 
        if (v2x_radio_rx_sock_create_and_bind(handle, &rx_sock, &rx_sockaddr)) {
            fprintf(stderr, "Error creating RX socket");
            exit(2);
        }
    }

    // Also  cancel tx_timer if doing tx proxy FIXME
    if (using_tx_timer) {
        /* Start the timer */
        its.it_value.tv_sec = interval_btw_pkts_ns / 1000000000;
        its.it_value.tv_nsec = interval_btw_pkts_ns % 1000000000;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;

        if (timer_settime(timerid, 0, &its, NULL) == -1) {
            errExit("timer_settime");
        } else {
            if (gVerbosity > 2) {
                printf("#  Timer set-up\n ");//print
            }
        }

        if (gVerbosity && interval_btw_pkts_ns) {//print
            printf(" # interval=%d ns ( approximately %d per second)\n", (int) interval_btw_pkts_ns,
                   (int)(1000000000 / interval_btw_pkts_ns));
        }
    }

    init_per_rv_stats();

	
	pthread_attr_setdetachstate(&attr2,PTHREAD_CREATE_DETACHED);	
	if((err=pthread_create(&rabbit2,&attr2,thread_recv_func,(void *)0))!=0)
	{
		perror("pthread_create pthread_sendto_cloud  error");
	}
#if 0	
    if (g_ml_fp) {// not print
    
	printf("==line %d =======\n", __LINE__);
        fprintf(g_ml_fp,
                "acme,SDK_ver#,SDK_build_date,build_time,direction,UE_ID,qty,v2x_id,sps|event,sps_prio,sps_interval,sps_res_len,period_ns,event_traffic_class\n");
        fprintf(g_ml_fp, "acme,%d,%s,%s,", verinfo.version_num, verinfo.build_date_str, verinfo.build_time_str);

        // direction, UE_ID, qty, v2x_id,
        fprintf(g_ml_fp, "%s,%d,%d,%d,", g_curr_test_str, g_ueid, qty, demo_res_req[FlowCnt].v2xid);
        // sps|event, sps_prio, sps_interval, sps_res_len, period_ns, payload_len,event_prio\n");
        fprintf(g_ml_fp, "%s,%d,%d,%d,%ld,%d\n",
                tx_mode_str,
                demo_res_req[FlowCnt].priority,
                demo_res_req[FlowCnt].period_interval_ms,
                demo_res_req[FlowCnt].tx_reservation_size_bytes,
                interval_btw_pkts_ns,
                traffic_class
               );
        fprintf(g_ml_fp, "payloads:");
        for (i = 0; i < payload_qty; i++) {
            fprintf(g_ml_fp, ",%d", payload_len[i]);
        }
        fprintf(g_ml_fp, "\n");

        if (test_mode != ModeRx) {
            fprintf(g_ml_fp, "%s\n", csv_header_row_receiver);
        } else if ((test_mode == ModeTx) && !do_net2air_proxy) {
            fprintf(g_ml_fp, "%s\n", csv_header_row_sender);
        }
        fflush(g_ml_fp);
    }
#endif
    printf("====rx_sock %d===\n",rx_sock);
    while (qty != 0) {
		//sem_wait(&gsem);
        timestamp = timestamp_now();
        if (test_mode == ModeEcho) {

			
            if (sample_rx(rx_sock) > 0) {
                sample_tx(tx_sock, rx_len);
            }
			
        } else {
            // Since simple single threaded, this application is just Tx, RX or Echo really.
            if (test_mode != ModeRx) {
                // OK -- so this is definitely transmit or echo mode
                if (do_net2air_proxy) {//0
                    int rc;
                    int flags = 0;
                    int len = sizeof(buf);

                    rc = recv(listen_net_socket, buf, len, flags);

                    if (rc > 0) {
                        sample_tx(tx_sock, rc);
                    }
                } else {
                   
					if (1 == recv_flag ){//pbuf_len == stm32's data len,  that's  length of the pbuf
					
					    printf("pbuf_len === %d ,line: %d \n",pbuf_len,__LINE__);
                    	demo_payload_tx_setup(buf, g_tx_seq_num++, pbuf_len, timestamp, pbuf);
						printf("===line :%d...\n", __LINE__);  
					}
					
					
					
                   	//demo_payload_tx_setup(buf, g_tx_seq_num++, payload_len[paynum],timestamp, NULL);
                    		
                    		
                    /* Now pick which Tx Socket to use, based on packet size or other instructions */
                    if (SpsFlowCnt < 2) {// SpsFlowCnt 1
                        if ((flow_type[0] == EventOnly) || sps_event_opt || SpsFlowCnt < 1) {
							
                            tx_sock = event_sock[0];
                            dest_sockaddr.sin6_port = htons((uint16_t)evt_port[0]);
							
                            printf("tx_sock === %d ,line: %d \n",tx_sock,__LINE__);
                            if (gVerbosity) {
                                printf("Event Flow #0: ");
                            }
                        } else {
                        
                            tx_sock = sps_sock[0];//sps_sock[0] = 21
                            dest_sockaddr.sin6_port = htons((uint16_t)sps_port[0]);
							
                           // printf("tx_sock === %d ,line: %d \n",tx_sock,__LINE__);
                        }
                    } 
					#if 0
                    else {//no print
                        int j = 0;
                        int selected_sps = 0;
                        int goal = payload_len[paynum];
                        int best_fit_so_far = 10000;
                        // So its an SPS Flow,  If we only have one flow set-up, choice is easy
                        // if there are more, we'll do it based on a counting pattern or packet size
                        // depending on what commandline args were used
                        for (j = 0; j <= FlowCnt; j++) {
                            if (flow_type[j] != EventOnly) {
                                int extra = demo_res_req[j].tx_reservation_size_bytes - goal ;

                                // If the reservation fits, see if it wastes the least so far.
                                if (extra >= 0) {
                                    if (extra < best_fit_so_far) {
                                        best_fit_so_far = extra;
                                        selected_sps = j;
                                    }
                                }
                            }
                        }

                        if (best_fit_so_far == 10000) {
                            if (gVerbosity) {
                                printf("[OVERSIZE]");
                            }
                            selected_sps = LargestSpsIndex;
                        }

                        if (gVerbosity) {
                            printf("Flow #%d: ", selected_sps);
                        }
                        tx_sock = sps_sock[selected_sps];
                        dest_sockaddr.sin6_port = htons((uint16_t)sps_port[selected_sps]);
                    } // (selecting from multiple SPS )
                    #endif
                   //===============================================
                    //sample_tx(tx_sock, payload_len[paynum]);//print call sendmsg 
					if (1 == recv_flag ){//////////////////////// use this to send
					
					    printf("===line :%d...\n", __LINE__);  
						sample_tx(tx_sock, send_len);//print call sendmsg 
						recv_flag = 0;
						
						printf("===line :%d...\n", __LINE__);  
						//memset(pbuf, 0, send_len);
						#if 0
						for (int i = 0; i < send_len; i++)
							printf("%c ",buf[i]);
						printf("\n");
						#endif
						
						
						

						

					}
                    // Increase index to payload array, and wrap if this was last defined one
                    #if 0
                    paynum++;
                    if (paynum >= payload_qty) {
                        paynum = 0;
                    }
					#endif
                }
            } else {
                sample_rx(rx_sock);
            }

			if (using_tx_timer) {// 1
                sigwait(&alarm_wait, &signal_which_alarmed);

                g_timer_misses += timer_getoverrun(timerid);
                if (g_timer_misses && gVerbosity > 1) {
                    printf("timer overruns:  %d, perhaps due to large CPU load\n", g_timer_misses);
                }
            }
			
			
        }

        if (qty) {
            qty--;
        }
		
    }

exit:
    close_all_v2x_sockets();
    ShowResults(stdout);
    assert(v2x_radio_deinit(handle) ==  V2X_STATUS_SUCCESS);
    assert(v2x_radio_deinit(handle) !=  V2X_STATUS_SUCCESS); // second attempt should fail

    return 0;
}
