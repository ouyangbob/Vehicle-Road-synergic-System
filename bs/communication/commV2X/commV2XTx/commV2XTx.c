/*-----------------------------------------------------------------------------*/
/*

	FileName	commV2XTx

	EditHistory
	2019-04-09	creat file 为了方便抽象出函数，把acme的功能分解掉，所以分为TX RX两个模块
    
    Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* general include                                                             */
/*-----------------------------------------------------------------------------*/
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

#define COMM_V2X_TX_C
#include "commV2XTx.h" /* self head file */

/*-----------------------------------------------------------------------------*/
/* module include                                                              */
/*-----------------------------------------------------------------------------*/
#include "../../../TargetSettings.h"
/*-----------------------------------------------------------------------------*/
/* private macro define                                                        */
/*-----------------------------------------------------------------------------*/
#define TEST_CLEAR_V2X_TX

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

#define errExit(msg)     do{perror(msg); return 1;}while (0)
                  

/*-----------------------------------------------------------------------------*/
/* private typedef define                                                      */
/*-----------------------------------------------------------------------------*/

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

// Acme message contents
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

static char buf[MAX_DUMMY_PACKET_LEN];
static int rx_len = 0;

static char server_ip_str[20] = "127.0.0.1";

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
#elif CURR_MODULE == RB_MODULE
static unsigned char g_testverno_magic = 'B'; // Magic test message proto version number
#elif CURR_MODULE == SL_MODULE
static unsigned char g_testverno_magic = 'S'; // Magic test message proto version number
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

		LOGD("Radio event listener callback: status=%d (%s)  ctx=%08lx\n",
			 event,
			 event_name(event),
			 *(unsigned long *)ctx);

		if (event != g_status)
		{
			g_status = event;
			// last_status_change_time = timestamp_now();
			LOGW("TX/RX Status Changed to <%s> **** \n", event_name(event));
			// switch (event) {
			// case V2X_TXRX_SUSPENDED:
			//     lost_tx_cnt++;
			//     lost_rx_cnt++;
			//     break;

			// case V2X_TX_SUSPENDED:
			//     lost_tx_cnt++;
			//     break;

			// case V2X_RX_SUSPENDED:
			//     lost_rx_cnt++;
			//     break;
			// }
		} // Status actually changed.
	}
	else
	{
		LOGE("NULL Context on radio_listener\n");
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

	// if (rx_sock >= 0) {
	//     v2x_radio_sock_close(&rx_sock);
	// }

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
	FuncName	termination_handlerTx

	EditHistory
	2019-04-11	 from acme
*/
static void termination_handlerTx(int signum)
{
	int i;
	printf("Got signal %d, tearing down Tx all services\n", signum);

	close_all_v2x_sockets();

	if (handle != V2X_RADIO_HANDLE_BAD)
	{
		v2x_radio_deinit(handle);
	}

	// ShowResults(stdout);

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
			//printf("\n");
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

	sig_action.sa_handler = termination_handlerTx;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;

	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGHUP, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
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
#ifndef TEST_CLEAR_V2X_TX
	if (1 || (1 && bytes_sent > 0))
	{
		printf("TX count: %d, len = %d\n", tx_count, bytes_sent);

		if (0)
		{
			print_buffer(buf, bytes_sent);
		}
	}
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
	buf_ptr = buf;
	uint32_t msg_len = 0;
	memset(buf, 0, sizeof(buf));
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

	if (msg_len < sizeof(buf))
	{
		buf[msg_len + 1] = '\0';
#ifndef TEST_CLEAR_V2X_TX
		printf("\n===========\nsendMsg=%s\n========\n", buf);
#endif
	}
#ifndef TEST_CLEAR_V2X_TX
	for (i = 0; i < msg_len; i++)
	{
		printf("%x ", buf[i]);
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

/*-----------------------------------------------------------------------------
	FuncName	v2xTxProcess
	function	v2x通信数据发送OBU——>OBU struct msghdr结构体信息
	return value	返回void
	EditHistory		2019-04-10 creat function buy wpz
-----------------------------------------------------------------------------*/
void v2xTxProcess(uint8_t *(*v2xTxBufGet)(uint16_t *bufLen, uint64_t time))
{
	int i; // packet loop counter
	v2x_concurrency_sel_t mode = V2X_WWAN_NONCONCURRENT;
	unsigned long test_ctx = 0xfeedbeef; // Just a dummy test context to make sure is maintained properly
	uint64_t timestamp = 0;
	unsigned long interval_btw_pkts_ns = -1;
	int timer_signo = SIGRTMIN;
	struct sockaddr_in6 dest_sockaddr;
	int traffic_class = -1;
	int f;
	int tx_sock;

	int paynum = 0;
	unsigned int FlowCnt = 0;

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
		LOGE("Error initializing the V2X radio, bail\n");
		return ;
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

	// for (f = 0; f <= FlowCnt; f++) {
	{
		f = 0;

		if (v2x_radio_tx_sps_sock_create_and_bind(handle, &demo_res_req[f],
												  &sps_function_calls, sps_port[f], evt_port[f],
												  &socknum,
												  &sockaddr,
												  &esocknum,
												  &esockaddr) != V2X_STATUS_SUCCESS)
		{

			fprintf(stderr, "Error creating SPS socket\n");
			return ;
		}
		sps_sock[f] = socknum;
		sps_sockaddr[f] = sockaddr;
		event_sock[f] = esocknum;
		event_sockaddr[f] = esockaddr;
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
	}

	tx_sock = sps_sock[0];
	dest_sockaddr.sin6_port = htons((uint16_t)sps_port[0]);

	init_per_rv_stats();

	printf("loop_start\n");
	while (1)
	{
		timestamp = timestamp_now();
#ifndef TEST_CLEAR_V2X_TX
		 printf("===================Tx time = %"PRIu64" -- \n",timestamp);
#endif
		bufPtr = v2xTxBufGet(&bufLen, timestamp);
//#ifndef TEST_CLEAR_V2X_TX
		printf("Tx bufLen = %d, encode is : ", bufLen);
		print_buffer(bufPtr, bufLen);
//#endif
		if (bufPtr == NULL)
		{
			sleep(1);
			continue;
		}
		bufLen = v2xPayloadTxSetup(buf, g_tx_seq_num++, bufLen,
								   timestamp, bufPtr);

#ifndef TEST_CLEAR_V2X_TX
		printf("\n Tx msgLen = %d\n", bufLen);
#endif
		tx_sock = sps_sock[0];
		dest_sockaddr.sin6_port = htons((uint16_t)sps_port[0]);
		sample_tx(tx_sock, bufLen);
		// Increase index to payload array, and wrap if this was last defined one
		paynum++;
		if (paynum >= 5)
		{
			paynum = 0;
		}
		usleep(500000);
	}
}

/* end of file */
