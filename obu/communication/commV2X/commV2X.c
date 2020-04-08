#include <stdbool.h>
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

#define COMM_V2X_C
#include "commV2X.h" 

/*-----------------------------------------------------------------------------*/
/* module include                                                              */
/*-----------------------------------------------------------------------------*/
#include "../../TargetSettings.h"
/*-----------------------------------------------------------------------------*/
/* private macro define                                                        */
/*-----------------------------------------------------------------------------*/
#define TEST_CLEAR_V2X

// 255 unique RV's tracked for IPG measurement, one byte UE ID
#define MAX_UEID (255)
#define MAX_DUMMY_PACKET_LEN (1000)
#define MAX_QTY_PAYLOADS (1000)
#define MAX_SPS_FLOWS (2)
#define MAX_FLOWS (100)

#define DEFAULT_SERVICE_ID (1)

// /* Simulated secured BSM lengths with 5 path history points */
#define PAYLOAD_SIZE_CERT (287)
#define PAYLOAD_SIZE_DIGEST (3000)

// /* Regarding these sample port#: any unused ports could be selected, and additional ports
//  * could also be set-up for additional reservations and near unlimited number of event flows. */
#define SAMPLE_SPS_PORT (2500)
#define SAMPLE_EVENT_PORT (2600)

// /* Two possible reservation sizes, if only one made, it uses the latgest of the two */
#define DEFAULT_SPS_RES_BYTES_0 (PAYLOAD_SIZE_CERT)
#define DEFAULT_SPS_RES_BYTES_1 (PAYLOAD_SIZE_DIGEST)

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
	double heading;
	bool has_velocity;
	double velocity;
} location_data_t;

// Acme message contents for tx
typedef struct _acme_message_t
{
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

typedef enum
{
	NoReg = 0,
	SpsOnly = 1,
	EventOnly = 2,
	ComboReg = 3
} FlowType_t;

/*-----------------------------------------------------------------------------*/
/* private parameters                                                          */
/*-----------------------------------------------------------------------------*/

static char tx_buf[MAX_DUMMY_PACKET_LEN];
static char rx_buf[MAX_DUMMY_PACKET_LEN];

static int rx_len = 0;

static char server_ip_str[20] = "127.0.0.1";

static void (*v2xMsgGot)(acme_message_r msg);
static long long gReportInterval_ns = 1000000000;
static v2x_radio_calls_t radio_calls;
static v2x_radio_handle_t handle;
static char *iface;
static uint64_t rv_rx_timestamp[MAX_UEID];   // Keep track of last RX'ed timestamp of each RV for IPG
static uint32_t rv_rx_cnt[MAX_UEID];		 // Keep track of total packets received for each RV
static uint32_t rv_presumed_sent[MAX_UEID];  // Keep track  how many packets we think other side has
static uint16_t rv_seq_num[MAX_UEID];		 // Keep track of last RV seq_num received for each RV
static uint16_t rv_missed_packets[MAX_UEID]; // Keep track of last RV seq_num received for each RV
static uint16_t rv_seq_start_num[MAX_UEID];  // Keep track of sequence start for each RV, so we can calculate PER
static int rx_count = 0;					 // number of packets to RX or TX. -1 = indefinite.
static int tx_count = 0;					 // number of tx packets

static v2x_event_t g_status = V2X_INACTIVE; // recent Event, from listner: effectively the status (TX/RX suspended/Active/etc)
static v2x_priority_et event_tx_prio = V2X_PRIO_2;

static int rx_sock;
static struct sockaddr_in6 rx_sockaddr;

static unsigned int g_missed_packets = 0;

static FlowType_t flow_type[MAX_FLOWS];
static uint16_t sps_port[MAX_FLOWS] = {0};
static int sps_sock[MAX_FLOWS];
static uint16_t evt_port[MAX_FLOWS] = {0};
static int event_sock[MAX_FLOWS];

static struct sockaddr_in6 sps_sockaddr[MAX_FLOWS];
static struct sockaddr_in6 event_sockaddr[MAX_FLOWS];
static struct sockaddr_in6 dest_sockaddr;

static uint16_t g_tx_seq_num = 0;

static v2x_tx_bandwidth_reservation_t demo_res_req[MAX_FLOWS] = {0}; /* The reservation request includes the service id, priority, interval, byte size */

static v2x_per_sps_reservation_calls_t sps_function_calls;

static const char v2x_event_strings[V2X_TXRX_SUSPENDED + 1][20] = {
	"Inactive",
	"Active",
	"TX Suspended",
	"RX Suspended",
	"TX + RX Suspended"};

static const char *event_name(v2x_event_t event)
{
	return (event <= V2X_TXRX_SUSPENDED ? v2x_event_strings[event] : "ERR: Unkonwn event.");
}

static void init_per_rv_stats(void)
{
	int i;

	for (i = 0; i < MAX_UEID; i++)
	{
		rv_rx_timestamp[i] = 0;
		rv_rx_cnt[i] = 0;
		rv_seq_num[i] = 0;
		rv_seq_start_num[i] = 0;
		rv_missed_packets[i] = 0;
		rv_presumed_sent[0];
	}
}

// Interface names
static const char default_v2x_ip_iface_name[] = "rmnet_data0";
static const char default_v2x_non_ip_iface_name[] = "rmnet_data1";
static const char default_db820_ethernet_name[] = "enP2p1s0";
static const char default_proxy_if_name[] = "rndis0";

static int payload_len[MAX_QTY_PAYLOADS] = {PAYLOAD_SIZE_CERT, PAYLOAD_SIZE_DIGEST, PAYLOAD_SIZE_DIGEST, PAYLOAD_SIZE_DIGEST, PAYLOAD_SIZE_DIGEST};

#if CURR_MODULE == RSU_MODULE
static unsigned char g_testverno_magic = 'R'; // Magic test message proto version number
#elif CURR_MODULE == OBU_MODULE
static unsigned char g_testverno_magic = 'O'; // Magic test message proto version number
#else
static unsigned char g_testverno_magic = 'Q'; // Magic test message proto version number
#endif
static unsigned char g_ueid = 1;			  // UE

/*-----------------------------------------------------------------------------*/
/* private function                                                  		   */
/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/*
	FuncName	timestamp_now
    
    return current time stamp in milliseconds
    @return long long

	EditHistory
	2019-04-11	 from acme
*/
static __inline uint64_t timestamp_now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	radio_listener

	EditHistory
	2019-04-11	 from acme
*/
static void radio_listener(v2x_event_t event, void *ctx)
{
	if (ctx)
	{
		if (event != g_status)
		{
			g_status = event;
		} // Status actually changed.
	}
	else
	{
		printf("NULL Context on radio_listener\n");
	}
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	close_all_v2x_sockets

	EditHistory
	2019-04-11	 from acme
*/
static void close_all_v2x_sockets(void)
{
	int i;

	printf("closing all Tx V2X Sockets \n");

	if (rx_sock >= 0) {
		v2x_radio_sock_close(&rx_sock);
	}

	for (i = 0; i < MAX_FLOWS; i++)
	{
		if (flow_type[i])
		{
			printf("closing Flow#%d: type=%d %d %d sps_port=%d evt_port=%d, %d ms, %d bytes\n",
				   i,
				   flow_type[i],
				   sps_sock[i],
				   event_sock[i],
				   sps_port[i],
				   evt_port[i],
				   demo_res_req[i].period_interval_ms,
				   demo_res_req[i].tx_reservation_size_bytes);

			if (sps_sock[i] >= 0)
			{
				v2x_radio_sock_close(sps_sock + i);
			}
			if (event_sock[i] >= 0)
			{
				v2x_radio_sock_close(event_sock + i);
			}
		}
	}
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	termination_handler

	EditHistory
	2019-04-11	 from acme
*/
static void termination_handler(int signum)
{
	int i;
	printf("Got signal %d, tearing down Tx all services\n", signum);

	close_all_v2x_sockets();

	if (handle != V2X_RADIO_HANDLE_BAD)
	{
		v2x_radio_deinit(handle);
	}

	signal(signum, SIG_DFL);
	raise(signum);
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	print_buffer

	Print out a buffer in hex

	EditHistory
	2019-04-12	 from acme
*/

static void print_buffer(uint8_t *buffer, int buffer_len)
{
	uint8_t *pkt_buf_ptr;
	int items_printed = 0;

	pkt_buf_ptr = buffer;

	while (items_printed < buffer_len)
	{
		if (items_printed && items_printed % 16 == 0)
		{
			printf("\n");
		}
		printf("%02x ", *pkt_buf_ptr);
		pkt_buf_ptr++;
		items_printed++;
	}
	printf("\n");
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	install_signal_handler

	EditHistory
	2019-04-11	 from acme
*/
static void install_signal_handler()
{
	struct sigaction sig_action;

	sig_action.sa_handler = termination_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;

	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGHUP, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	deserialize_acme_message

    Deserialize acme message, copy data from buffer into msg struct.
    
    
	EditHistory
	2019-04-11	 from acme
    2019-04-16   remove return,and set msg->buf_ptr
*/
static void deserialize_acme_message(acme_message_r *msg, uint8_t *buf, uint32_t msg_len)
{
    uint8_t *buf_ptr = buf;

    // family id
    if (msg_len)
    {
        msg->v2x_family_id = *buf_ptr++;
        msg_len--;
    }

    // UE equipment id
    if (msg_len)
    {
        msg->equipment_id = *buf_ptr++;
        msg_len--;
    }

    // Sequence number
    if (msg_len >= 2)
    {
        msg->seq_num = (ntohs(*(uint16_t *)buf_ptr));
        msg->has_seq_num = true;
        buf_ptr += sizeof(uint16_t);
        msg_len -= sizeof(uint16_t);
    }

    // Attempt to desrialize timestamp
    msg->has_timestamp = false;

    if (msg_len && *buf_ptr == '<')
    {
        unsigned char *ts_end_p;
        buf_ptr++;
        msg_len--;
        ts_end_p = strstr(buf_ptr, ">");

        if (ts_end_p)
        {

#if SIM_BUILD
            sscanf(buf_ptr, "%ld", &msg->timestamp);
#else
            sscanf(buf_ptr, "%Ld", &msg->timestamp);
#endif
            msg->has_timestamp = true;

            msg_len -= (ts_end_p + 1) - buf_ptr;
            buf_ptr = ts_end_p + 1;
        }
    }

    /* added by WPz */
    msg->buf_ptr = buf_ptr;
    msg->buf_len = msg_len;
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	missPacketsProcess

	EditHistory
	2019-04-12  create by WPz
*/
void missPacketsProcess(acme_message_r msg)
{
    uint16_t seq_num = 0;

    unsigned char senders_magic = msg.v2x_family_id;
    unsigned char senders_ueid = msg.equipment_id;

    int missed_packets;
    int64_t latency = 0;
    uint64_t ipg_us = 0; // Interpacket gap in microseconds
    float per_ue_loss_pct = 0.f;
    uint64_t tx_timestamp = 0;
    uint64_t rx_timestamp = timestamp_now();

    rv_rx_cnt[senders_ueid]++;        // Per RV Count incremented.
    rv_presumed_sent[senders_ueid]++; // Per RV TX count increment.
    seq_num = msg.seq_num;

    // only tabulate lost/missed sequence after the first packet
    if (rv_rx_cnt[senders_ueid] > 1)
    {
        missed_packets = seq_num - rv_seq_num[senders_ueid] - 1;

        // 16 bit roll-over can be ignored
        if (missed_packets < 0)
        {
            if (missed_packets == -1)
            {
                LOGE("ERR: Duplicated seq_num packet from RV#%d\n", senders_ueid);
                missed_packets = 1;               //We'll count as a single error
                rv_presumed_sent[senders_ueid]--; // don't count duplicates as sent
            }
            else
            {
                // If Way, way large neg number, then seq# rolled over
                if (missed_packets < -32768)
                {
                    // these two additions will result in 0, if roll-over was 65536 -> 0
                    missed_packets = seq_num;
                    missed_packets += 65535 - rv_seq_num[senders_ueid];
                    rv_presumed_sent[senders_ueid] += missed_packets; //could be zero
                }
                else
                {
                    // Consider any sequence # within 100 an out-of order packet
                    if (missed_packets < -20)
                    {
                        rv_seq_start_num[senders_ueid] = seq_num;
                        fprintf(stderr, "# RV restarted, sequence jumped \n");
                        missed_packets = 1;
                    }
                    else
                    {
                        missed_packets++;     // compensate for -1 added initially
                        missed_packets *= -1; //change sign to positive.
                        fprintf(stderr, "# ERR: Out of order sequence by %d \n", missed_packets);
                        // g_out_of_order_packets++;
                    }
                }
            }
        }
        else
        {
            // Boring positive number of missed packets.
            rv_presumed_sent[senders_ueid] += missed_packets; //could be zero
        }

        fprintf(stdout, "#%d|seq#%d|", rv_rx_cnt[senders_ueid],
                seq_num);

        if (missed_packets > 0)
        {
            rv_missed_packets[senders_ueid] += missed_packets;
            g_missed_packets += missed_packets;
            per_ue_loss_pct = rv_missed_packets[senders_ueid] * 100.0;
            per_ue_loss_pct /= (float)rv_presumed_sent[senders_ueid];

            fprintf(stderr, "# Err: %d missed, per UE: %d lost(%8.1f%%)\n",
                    missed_packets,
                    rv_missed_packets[senders_ueid],
                    per_ue_loss_pct);
        }
    }
    else
    {
        printf("RV #%d seq# start initalized at %d\n", senders_ueid, seq_num);
        rv_seq_start_num[senders_ueid] = seq_num;
    }

    per_ue_loss_pct = rv_missed_packets[senders_ueid] * 100.0;
    per_ue_loss_pct /= (float)rv_presumed_sent[senders_ueid];

    /* For each RV, record the last received sequence number */
    rv_seq_num[senders_ueid] = seq_num;

    // If message contains a timestamp
    if (msg.has_timestamp)
    {

        printf("[has ts brackets]");

        tx_timestamp = msg.timestamp;

        printf(" senders stamp:<%" PRIu64 "> ", tx_timestamp);

        latency = rx_timestamp - tx_timestamp;

        printf("<latency=%6.2f ms> ", (double)latency / 1000.0);
        if (latency < 0)
        {
            printf("!!! WARN NEG LATENCY !!!  ");
        }

        if (rv_rx_cnt[senders_ueid] > 1)
        {
            ipg_us = rx_timestamp - rv_rx_timestamp[senders_ueid];
            printf("<ipg=%6.2f ms>", (double)ipg_us / 1000.0);
        }
        rv_rx_timestamp[senders_ueid] = rx_timestamp;
    }

    printf("|total missed=%d| per UE lost/sent=|%d|%d|%8.1f%%|",
           g_missed_packets,
           rv_missed_packets[senders_ueid],
           rv_presumed_sent[senders_ueid],
           per_ue_loss_pct);
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	rxStatusUpdate

	EditHistory
	2019-04-11	 from acme
*/
static void rxStatusUpdate(int rx_sock)
{
    int rc = 0;
    int flags = 0;
    int len = sizeof(rx_buf);
    struct ethhdr *eth;
    unsigned char *buf_p;
    unsigned char senders_magic;
    unsigned char senders_ueid;
    acme_message_r msg = {0};

    printf("V2X rady RX...\n");
    memset(rx_buf, 0, sizeof(rx_buf));

    //通过sock接收buf
    rc = recv(rx_sock, rx_buf, len, flags);
    if (rc < 0)
    {
        printf("Error receiving message!\n");
        return;
    }

    rx_count++;
    buf_p = rx_buf;
    // Total Rx packet count (not per RV)
    printf("|#%-4d|l=%3d|", rx_count, rc);

    //解包
    deserialize_acme_message(&msg, buf_p, rc);
    senders_magic = msg.v2x_family_id;
    senders_ueid = msg.equipment_id;

#ifndef CANCEL_OLD_ACME
    char magic_ver_str[4];
    magic_ver_str[0] = 0;

    if (senders_magic >= '1' && senders_magic <= 'z')
    {
        snprintf(magic_ver_str, sizeof(magic_ver_str), "(%c)", senders_magic);
    }

    fprintf(stdout, "Ver=%d%s|", senders_magic, magic_ver_str);

    fprintf(stdout, "UE#%d|\n", senders_ueid);
#endif
  
    //回调处理函数,对msg值进行分类打印
    v2xMsgGot(msg);

    rx_buf[rc + 1] = 0; // null terminate the string.

    // FIXME -- could add flag to force seq#/timestamp checking, even if verno mismatch
    if (senders_magic == 'R')
    {
        missPacketsProcess(msg);
#ifndef TEST_CLEAR_V2X
        printf("bufLen = %d\n", msg.buf_len);
        print_buffer(msg.buf_ptr, msg.buf_len);
#endif
    }

#ifndef TEST_CLEAR_V2X
    print_buffer(rx_buf, rc);
#endif
    rx_len = rc;
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2xRxInit

	EditHistory
	2019-04-16	func create
*/
void v2xRxInit(void (*v2xMsgGotFuncPtr)(acme_message_r msg))
{
    v2xMsgGot = v2xMsgGotFuncPtr;
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	sample_tx

	EditHistory
	2019-04-15  from acme
*/
static int sample_tx(int socket, int n_bytes)
{
	int bytes_sent = 0;
	struct msghdr message = {0};
	struct iovec iov[1] = {0};
	struct cmsghdr *cmsghp = NULL;
	char control[CMSG_SPACE(sizeof(int))];

	/* Send data using sendmsg to provide IPV6_TCLASS per packet */
	iov[0].iov_base = tx_buf;
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

	if (bytes_sent < 0)
	{
		fprintf(stderr, "Error sending message: %d\n", bytes_sent);
		bytes_sent = -1;
	}
	else
	{
		if (bytes_sent == n_bytes)
		{
			tx_count++;
		}
		else
		{
			printf("TX bytes sent were short\n");
		}
	}
#ifndef TEST_CLEAR_V2X
	printf("TX count: %d, len = %d\n", tx_count, bytes_sent);
	print_buffer(tx_buf, bytes_sent);
#endif
	return bytes_sent;
}
/*-----------------------------------------------------------------------------*/
/*
	FuncName	AddNewDefaultFlow

	EditHistory
	2019-04-15  from acme
*/
static void AddNewDefaultFlow(int idx)
{
	if ((idx >= 0) && (idx < MAX_FLOWS))
	{
		demo_res_req[idx].v2xid = DEFAULT_SERVICE_ID; // Must be 1, 2, 3, 4 for default config in most pre-configured units
		demo_res_req[idx].priority = V2X_PRIO_2;	  // 2 = lowest priority supported in TQ.
		demo_res_req[idx].period_interval_ms = -1;	// Invalid Periodicity signaling to use Interval if not specified
		demo_res_req[idx].tx_reservation_size_bytes = (idx ? DEFAULT_SPS_RES_BYTES_1 : DEFAULT_SPS_RES_BYTES_0);
		flow_type[idx] = ComboReg; // By default we register an event with each SPS
		sps_port[idx] = SAMPLE_SPS_PORT + idx;
		evt_port[idx] = SAMPLE_EVENT_PORT + idx;
	}
	else
	{
		LOGE("Max number of compile time supported flows exceeded.\n");
		return ;
	}
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	serialize_acme_message

	Serialize acme message and copy to buffer.
	Return the number of bytes copied.

	EditHistory
	2019-04-15  from acme
	2019-04-17	modify by WPz, to serialize a V2X message
*/
static uint32_t serialize_acme_message(acme_message_t *msg, uint8_t *buffer, uint16_t bufLen)
{
	uint8_t *buf_ptr;
	uint32_t i, temp_len;
	buf_ptr = tx_buf;
	uint32_t msg_len = 0;
	memset(tx_buf, 0, sizeof(tx_buf));
	//
	//modify by WPz.
	//
	*buf_ptr++ = msg->v2x_family_id;
	msg_len++;

	// Next-byte is the UE Equipment ID.
	*buf_ptr++ = msg->equipment_id;
	msg_len++;

	// Only add the timestamp and sequence number of requested length long enough
	uint16_t short_seq_num = htons(msg->seq_num);
	memcpy(buf_ptr, (unsigned char *)&short_seq_num, sizeof(uint16_t));
	buf_ptr += (sizeof(uint16_t));
	msg_len += (sizeof(uint16_t));

	// Attempt adding timestamp
	char timestamp_format[] = "<%" PRIu64 ">";
	temp_len = snprintf(NULL, 0, timestamp_format, msg->timestamp);
	buf_ptr += snprintf(buf_ptr, 100, timestamp_format, msg->timestamp);
	msg_len += temp_len;

	memcpy(buf_ptr, buffer, bufLen);
	msg_len += bufLen;

	if (msg_len < sizeof(tx_buf))
	{
		tx_buf[msg_len + 1] = '\0';
#ifndef TEST_CLEAR_V2X
		printf("\n===========\nsendMsg=%s\n========\n", tx_buf);
#endif
	}
#ifndef TEST_CLEAR_V2X
	for (i = 0; i < msg_len; i++)
	{
		printf("%x ", tx_buf[i]);
		if (i % 20 == 0)
		{
			printf("\n");
		}
	}
	printf("\n=========\n");
#endif
	return msg_len;
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2xPayloadTxSetup

	EditHistory
	2019-04-15  from acme
	2019-04-17	modify by WPz, to serialize a V2X message
*/
static uint16_t v2xPayloadTxSetup(uint8_t *buf,
								  uint16_t seq_num,
								  uint16_t bufLen,
								  uint64_t timestamp,
								  uint8_t *payload)
{
	uint8_t *buf_ptr, val;
	uint32_t i, temp_len;

	buf_ptr = buf;
	acme_message_t msg = {0};

	// Very first payload is test Magic number, this is  where V2X Family ID would normally be.
	msg.v2x_family_id = g_testverno_magic;
	// Next-byte is the UE Equipment ID.
	msg.equipment_id = g_ueid;
	msg.seq_num = seq_num;
	msg.has_seq_num = true;

	if (timestamp)
	{
		msg.has_timestamp = true;
		msg.timestamp = timestamp;
	}

	msg.has_kinematics = false;

	return serialize_acme_message(&msg, payload, bufLen);
}

/*-----------------------------------------------------------------------------*/
/*
	FuncName	v2xRxProcess

	EditHistory
	2019-04-11	func create,将所有acme相关的 V2X接收代码都加上注释 // from acme
*/
void v2xRxProcess(void)
{
    printf("rx loop_start\n");
    while (1)
    {
        rxStatusUpdate(rx_sock);
    }
}

void v2xProcessInit(void)
{
	int i; // packet loop counter
	v2x_concurrency_sel_t mode = V2X_WWAN_NONCONCURRENT;
	unsigned long test_ctx = 0xfeedbeef; // Just a dummy test context to make sure is maintained properly
	uint64_t timestamp = 0;
	unsigned long interval_btw_pkts_ns = -1;
	int timer_signo = SIGRTMIN;
	int traffic_class = -1;
	int tx_sock;

	unsigned int FlowCnt = 0;

	/* Block timer signal temporarily */
    printf("Blocking signal %d\n", timer_signo);

	//
	//added by WPz.
	//
	uint8_t *bufPtr = NULL;
	uint16_t bufLen = 0;

	/* counter FlowCnt keeps track of how many SPS resrvations have been set-up.
	* This is a natural counting number, 0 means no SPS, 2 means two actual SPS flows were requested*/
	unsigned int SpsFlowCnt = 0;

	/* Set destination information */
	int socknum = 0;
	struct sockaddr_in6 sockaddr;
	int esocknum = 0;
	struct sockaddr_in6 esockaddr = {0};
	char dest_ipv6_addr_str[50] = "ff02::1";
	iface = (char *)default_v2x_non_ip_iface_name;

	AddNewDefaultFlow(FlowCnt);
	install_signal_handler();

	radio_calls.v2x_radio_status_listener = radio_listener;

	// Init will fail if CV2X mode is "INACTIVE" but not if just suspended due to timing.
	handle = v2x_radio_init(iface, mode, &radio_calls, &test_ctx);
	if (handle == V2X_RADIO_HANDLE_BAD || handle >= V2X_MAX_RADIO_SESSIONS)
	{
		printf("Error initializing the V2X radio, fail\n");
		return;
	}

	// radio_query_and_print_param(iface);
	// v2x_test_param_check(&gCapabilities);

	/* Disable the socket connection */
	v2x_disable_socket_connect();

	/* Set destination information */
	inet_pton(AF_INET6, dest_ipv6_addr_str, (void *)&dest_sockaddr.sin6_addr);
	dest_sockaddr.sin6_family = AF_INET6;
	dest_sockaddr.sin6_scope_id = if_nametoindex(iface);

	// If this is not a RX client, configure the dest IPV6 addr & dest port (if requested)
	v2x_set_dest_ipv6_addr(dest_ipv6_addr_str);
	/* Set default TCLASS on event socket */
	traffic_class = v2x_convert_priority_to_traffic_class(event_tx_prio);
	fprintf(stdout, "# traffic_class=%d\n", traffic_class);

	if (v2x_radio_tx_sps_sock_create_and_bind(handle, &demo_res_req[FlowCnt],
												  &sps_function_calls, sps_port[FlowCnt], evt_port[FlowCnt],
												  &socknum,
												  &sockaddr,
												  &esocknum,
												  &esockaddr) != V2X_STATUS_SUCCESS)
	{

		fprintf(stderr, "Error creating SPS socket\n");
		return ;
	}
	sps_sock[FlowCnt] = socknum;
	sps_sockaddr[FlowCnt] = sockaddr;
	event_sock[FlowCnt] = esocknum;
	event_sockaddr[FlowCnt] = esockaddr;
	SpsFlowCnt++;

	if (esocknum)
	{
		if (setsockopt(esocknum, IPPROTO_IPV6, IPV6_TCLASS,
						   (void *)&traffic_class, sizeof(traffic_class)) < 0)
		{
			fprintf(stderr, "setsockopt(IPV6_TCLASS) on event socket failed err=%d\n", errno);
		}
		else
		{
			printf("Setup traffic class=%d on the event socket completed.\n", traffic_class);
		}
	}

	if (socknum)
	{
		if (setsockopt(socknum, IPPROTO_IPV6, IPV6_TCLASS,
						   (void *)&traffic_class, sizeof(traffic_class)) < 0)
		{
			fprintf(stderr, "setsockopt(IPV6_TCLASS) on SPS socket failed err=%d\n", errno);
		}
		else
		{
			printf("Setup traffic class =%d completed the SPS flow socket.\n", traffic_class);
		}
	}

	if (v2x_radio_rx_sock_create_and_bind(handle, &rx_sock, &rx_sockaddr))
	{
		fprintf(stderr, "Error creating RX socket");
		return;
	}

	init_per_rv_stats();

	pthread_t threadidV2xRx = 0;

    //新建V2X Rx线程
    pthread_create(&threadidV2xRx, NULL, (void *)&v2xRxProcess, NULL);
    pthread_detach(threadidV2xRx);
}

/*-----------------------------------------------------------------------------
	FuncName	v2xTxProcess
	function	v2x通信数据发送OBU——>OBU struct msghdr结构体信息
	return value	返回void
	EditHistory		2019-04-10 creat function buy wpz
-----------------------------------------------------------------------------*/
void v2xTxProcess(uint8_t *(*v2xTxBufGet)(uint16_t *bufLen, uint64_t time))
{
	uint64_t timestamp = 0;
	int tx_sock;

	//
	//added by WPz.
	//
	uint8_t *bufPtr = NULL;
	uint16_t bufLen = 0;

	printf("tx loop_start\n");
	while (1)
	{
		timestamp = timestamp_now();
		bufPtr = v2xTxBufGet(&bufLen, timestamp);

#ifndef TEST_CLEAR_V2X
		printf("Tx bufLen = %d\n", bufLen);
		print_buffer(bufPtr, bufLen);
#endif
		if (bufPtr == NULL)
		{
			sleep(1);
			continue;
		}
		bufLen = v2xPayloadTxSetup(tx_buf, g_tx_seq_num++, bufLen,
								   timestamp, bufPtr);

#ifndef TEST_CLEAR_V2X
		printf("\n Tx msgLen = %d\n", bufLen);
#endif
		tx_sock = sps_sock[0];
		dest_sockaddr.sin6_port = htons((uint16_t)sps_port[0]);
		sample_tx(tx_sock, bufLen);
		usleep(500000);
	}
}

/* end of file */
