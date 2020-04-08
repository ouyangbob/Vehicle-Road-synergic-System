/*
 *  Copyright (c) 2017 Qualcomm Technologies, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/**
    @file v2x_radio_api.h

    @addtogroup v2x_api_radio
    Abstraction of the radio driver parameters for a V2X broadcast socket
    interface, including 3GPP CV2X QoS bandwidth contracts.
 */

#ifndef __V2X_RADIO_APIS_H__
#define __V2X_RADIO_APIS_H__ 1

#include <net/ethernet.h> /* the L2 protocols */
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "v2x_common.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup v2x_api_radio
@{ */

/** Radio data handle to the interface. */
typedef int v2x_radio_handle_t;

/** Invalid handle returned by v2x_radio_init() upon an error. */
#define V2X_RADIO_HANDLE_BAD (-1)

/** Limit on the number of simultaneous RmNet Radio interfaces this library can
    have open at once.

    Typically, there are only a few actual radios. On the same radio however, one
    interface can be for IP traffic, and another interface can be for non-IP
    traffic.
 */
#define V2X_MAX_RADIO_SESSIONS (10)

/** Wildcard value for a port number. When the wildcard is used, all V2X received
    traffic is routed.
 */
#define V2X_RX_WILDCARD_PORTNUM (9000)

/**
    Describes whether the radio chip modem should attempt or support concurrent
    3GPP CV2X operation with a WWAN 4G/5G data call.

    Some chips are only capable of operating on CV2X. In this case:
    - The callers can only request V2X_WWAN_NONCONCURRENT.
    - The interface parameters that are returned list v2x_concurrency_sel_t as
        the value for V2X_WAN_NONCONCURRENT.
 */
typedef enum {
    V2X_WWAN_NONCONCURRENT = 0, /**< No simultaneous WWAN + CV2X on this
                                     interface. */
    V2X_WWAN_CONCURRENT = 1     /**< Interface supports requests for concurrent
                                     support of WWAN + CV2X connections. */
} v2x_concurrency_sel_t;

/**
    Event indications sent asynchronously from the radio via callbacks that
    indicate the state of the radio. The state can change in response to the loss
    of timing precision or a geofencing change.
 */
typedef enum {
    V2X_INACTIVE = 0,    /**< V2X communication is disabled. */
    V2X_ACTIVE,          /**< V2X communication is enabled. Transmit and
                              receive are possible. */
    V2X_TX_SUSPENDED,    /**< Small loss of timing precision occurred.
                                                            Transmit is no longer supported. */
    V2X_RX_SUSPENDED,    /**< Radio can no longer receive any messages. */
    V2X_TXRX_SUSPENDED,  /**< Radio can no longer transmit or receive, for
                              some reason. @newpage */
} v2x_event_t;

/** Range of supported priority levels, where a lower number means a higher
    priority. For example, 8 is the current 3GPP standard. */
typedef enum {
    V2X_PRIO_MOST_URGENT = 0,
    V2X_PRIO_1 = 1,
    V2X_PRIO_2 = 2,
    V2X_PRIO_3 = 3,
    V2X_PRIO_4 = 4,
    V2X_PRIO_5 = 5,
    V2X_PRIO_6 = 6,
    V2X_PRIO_BACKGROUND = 7
} v2x_priority_et;

/**
    Contains information on the capabilities of a Radio interface.
 */
typedef struct {
    int link_ip_MTU_bytes;
    /**< Maximum data payload length (in bytes) of a packet supported by the
         IP Radio interface. */

    int link_non_ip_MTU_bytes;
    /**< Maximum data payload length (in bytes) of a packet supported by the
         non-IP Radio interface. */

    v2x_concurrency_sel_t max_supported_concurrency;
    /**< Indicates whether this interface supports concurrent WWAN with
         V2X (PC5). */

    uint16_t non_ip_tx_payload_offset_bytes;
    /**< Byte offset in a non-IP Tx packet before the actual payload begins.
         In 3GPP CV2X, the first byte after the offset is the 1-byte V2X
         Family ID.

         This offset is to the left for a per-packet Tx header that includes
         Tx information that might be inserted in front of the packet
         payload (in subsequent releases).
         An example of Tx information is MAC/Phy parameters (power, rate,
         retransmissions policy, and so on).

         Currently, this value is expected to be 0. But it is reserved to
         support possible per-packet Tx/Rx headers that might be added in
         future releases of this API. */

    uint16_t non_ip_rx_payload_offset_bytes;
    /**< Byte offset in a non-IP Rx packet before the actual payload begins.

         Initially, this value is zero. But it allows for later insertion of
         per-packet Rx information (sometimes called metadata) to be
         added to the front of the data payload. An example of Rx
         information is MAC/Phy measurements (receive signal strength,
         timestamps, and so on).

         The V2X Family ID is considered as part of the payload in the 3GPP
         CV2X. Higher layers (applications that are clients to this API) must
         remove or advance past that 1 byte to get to the more familiar
         actual WSMP/Geonetworking payload. */

    uint16_t int_min_periodicity_multiplier_ms;
    /**< Lowest number of milliseconds requested for a bandwidth.

         This value is also the basis for all possible bandwidth reservation
         periods. For example, if this multiplier=100 ms, applications can only
         reserve bandwidths of 100 ms, 200 ms, up to 1000 ms. */

    uint16_t int_maximum_periodicity_ms;
    /**< Least frequent bandwidth periodicity that is supported. Above this
         value, use event-driven periodic messages of a period larger than
         this value. */

    unsigned supports_10ms_periodicity : 1;
    /**< Indicates whether n*10 ms periodicities are supported (1) or not (0). */

    unsigned supports_20ms_periodicity : 1;
    /**< Indicates whether a n*20 ms bandwidth reservation is supported (1) or
         not (0). */

    unsigned supports_50ms_periodicity : 1;
    /**< Indicates whether 50 ms periodicity is supported (1) or not (0). */

    unsigned supports_100ms_periodicity : 1;
    /**< Indicates whether the basic minimum periodicity of 100 ms is
         supported (1) or not (0). */

    unsigned max_quantity_of_auto_retrans : 4;
    /**< Maximum number automatic retransmissions (0 to 15). */

    unsigned size_of_layer2_mac_address : 4;
    /**< Size of the L2 MAC address.

         Different Radio Access Technologies have different-sized L2 MAC
         addresses: 802.11 has 6 bytes, whereas 3GPP PC5 has only 3 bytes.

         Because a randomized MAC address comes from an HSM with good pseudo
         random entropy, higher layers must know how many bytes of the MAC
         address to generate. */

    uint16_t v2x_number_of_priority_levels;
    /**< Number of different priority levels supported. For example, 8 is
         the current 3GPP standard (where a lower number means a higher
         priority). */

    uint16_t highest_priority_value;
    /**< Least urgent priority number supported by this radio.

         Higher numbers are lower priority, so if the full range is supported,
         this value is #V2X_PRIO_BACKGROUND. */

    uint16_t lowest_priority_value;
    /**< Highest priority value (most urgent traffic).

         Lower numbers are higher priority, so if the highest level supported
         this value is #V2X_PRIO_MOST_URGENT. */

    uint16_t max_qty_SPS_flows;
    /**< Maximum number of supported SPS reservations. */

    uint16_t max_qty_non_SPS_flows;
    /**< Maximum number of supported event flows (non-SPS ports). @newpagetable */

} v2x_iface_capabilities_t;

/**
    Converts a traffic priority to one of the 255 IPv6 traffic class bytes that
    are used in the data plane to indicate per-packet priority on non-SPS
    (event driven) data ports. This function is symmetric and is a reverse
    operation.

    The traffic priority is one of the values between min_priority_value and
    max_priority_value returned in the v2x_iface_capabilities_t query.

    @datatypes
    #v2x_priority_et

    @param[in] priority  Packet priority that is to be converted to an IPv6
                         traffic class. This priority is between the lowest and
                         highest priority values returned in
                         #v2x_iface_capabilities_t.

    @return
    IPv6 traffic class for achieving the calling input parameter priority level.
    @newpage
 */
uint16_t v2x_convert_priority_to_traffic_class(v2x_priority_et priority);

/**
    Maps an IPv6 traffic class to a V2X priority value. This function is the
    inverse of the v2x_convert_priority_to_traffic_class() function. It is
    symmetric and is a reverse operation.

    @param[in] traffic_class  IPv6 traffic classification that came in a packet
                                                        from the radio.

    @return
    Priority level (between highest and lowest priority values) equivalent to
    the input IPv6 traffic class parameter.
 */
v2x_priority_et v2x_convert_traffic_class_to_priority(uint16_t traffic_class);

/**
    Contains parameters used to convey MAC/Phy settings (such as transmit power
    limits) channel/bandwidth from the application.

    Applications might need to set these parameters in response to a WSA/WRA or
    other application-level reconfiguration (such as power reduction).
    Currently, these parameters are all transmission-profile types of parameters.
 */
typedef struct {
    int channel_center_khz;
    /**< Channel center frequency in kHz. */

    int channel_bandwidth_mhz;
    /**< Channel bandwidth in MHz, such as 5 MHz, 10 MHz, and 20 MHz. */

    int tx_power_limit_decidbm;
    /**< Limit on transmit power in tenths of a dBm.

         Examples of how this field is used:
         - To reduce a range that is possible as the output of an
             application-layer congestion management scheme.
         - In cases when a small communication range is needed, such as
             indoors and electronic fare collection.
         - In ETSI use cases where the power might need to be temporarily
             restricted to accommodate a nearby mobile 5.8 CEN enforcement toll
             (EFC) reader. @tablebulletend */

    int qty_auto_retrans;
    /**< Used to request the number of automatic-retransmissions. The maximum
         supported number is defined in v2x_iface_capabilities_t. */

    uint8_t l2_source_addr_length_bytes;
    /**< Length of the L2 MAC address that is supplied to the radio. */

    uint8_t *l2_source_addr_p;
    /**< Pointer to l2_source_addr_length_bytes, which contains the L2 SRC
         address that the application layer selected for the radio.
         @newpagetable */

} v2x_radio_macphy_params_t;

/**
    Used to request a QoS bandwidth contract, which is implemented in
    PC5 3GPP V2-X radio as a <i>Semi Persistent Flow</i> (SPS).

    The underlying radio providing the interface might support periodicities of
    various granularity in 100 ms integer multiples (such as 200 ms, 300 ms, and
    400 ms).

    The reservation is also used internally as a handle.
 */
typedef struct {
    int v2xid;
    /**< Variable length 4-byte PSID or ITS_AID, or another application ID. */

    v2x_priority_et priority;
    /**< Specifies one of the 3GPP levels of priority for the traffic that is
         prereserved on the SPS flow.

         Use v2x_radio_query_parameters() to get the exact number of
         supported priority levels. */

    int period_interval_ms;
    /**< Bandwidth-reserved periodicity interval in milliseconds.

         There are limits on which intervals the underlying radio supports.
         Use the capabilities query method to discover the
         int_min_periodicity_multiplier_ms and int_maximum_periodicity_ms
         supported intervals. */

    int tx_reservation_size_bytes;
    /**< Number of bytes of Tx bandwidth that are sent every periodicity
         interval. @newpagetable */

} v2x_tx_bandwidth_reservation_t;

/**
    Contains the measurement parameters for configuring the MAC/Phy radio
    channel measurements (such as CBR utilization).

    The radio chip contains requests on radio measurement parameters that API
    clients can use to specify the following:
    - How their higher-level application requires the CBR/CBP to be measured
    - Over what time window
    - How often to send a report
 */
typedef struct {
    int channel_measurement_interval_us;
    /**< Duration in microseconds of the sliding window size. */

    int rs_threshold_decidbm;
    /**< Parameter to the radio CBR measurement that is used for determining
         how busy the channel is.

         Signals weaker than the specified receive strength (RSRP, or RSSI) are
         not considered to be in use (busy). */
} v2x_chan_meas_params_t;

/**
    Periodically returned by the radio with all measurements about the radio
    channel, such as the amount of noise and bandwidth saturation
    (channel_busy_percentage, or CBR).
 */
typedef struct {
    float channel_busy_percentage;
    /**< No measurement parameters are supplied. */

    float noise_floor;
    /**< Measurement of the background noise for a quiet channel.
         @newpagetable */
} v2x_chan_measurements_t;

/**
    Contains callback functions used in a v2x_radio_init() call.

    The radio interface uses these callback functions for events such as
    completion of initialization, a Layer-02 MAC address change, or a status
    event (loss of sufficient GPS time precision to transmit).

    These callbacks are related to a specific radio interface, and its
    MAC/Phy parameters, such as transmit power, bandwidth utilization, and
    changes in radio status.
 */
typedef struct {

    /**
    Callback that indicates initialization is complete.

    @param status   Updated current radio status that indicates whether
                    transmit and receive are ready.
    @param context  Pointer to the context that was supplied during initial
                    registration.
    */
    void (*v2x_radio_init_complete)(v2x_status_enum_type status,
                                    void *context);

    /**
    Callback made when the status in the radio changes. For example, in
    response to a fault when there is a loss of GPS timing accuracy.

    @param event    Delivery of the event that just occurred, such losing the
                    ability to transmit.
    @param context  Pointer to the context of the caller who originally
                    registered for this callback.

    @newpage
    */
    void (*v2x_radio_status_listener)(v2x_event_t event,
                                      void *context);

    /**
    Callback made periodically from lower layers when periodic radio
    measurements are prepared.

    @param measurements  Pointer to the periodic measurements.
    @param context       Pointer to the context of the caller who originally
                         registered for this callback.
    */
    void (*v2x_radio_chan_meas_listener)(v2x_chan_measurements_t *measurements, void *context);

    /**
    Callback made by the platform SDK when the MAC address (L2 SRC address)
    changes.

    @param new_l2_address  New L2 destination address, as an integer (because
                           the L2 address is 3 bytes).
    @param context         Pointer to the context of the caller who
                           originally registered for this callback.
    */
    void (*v2x_radio_l2_addr_changed_listener)(int new_l2_address, void *context);

    /**
    Callback made to indicate that the requested radio MAC/Phy change (such
    as channel/frequency and power) has completed.

    @param context  Pointer to the context of the caller who originally
                    registered for this callback.

    @newpage
    */
    void (*v2x_radio_macphy_change_complete_cb)(void *context);

} v2x_radio_calls_t;

/**
    Contains MAC information that is reported from the actual MAC SPS in the
    radio. The offsets can periodically change on any given transmission report.
 */
typedef struct {
    uint32_t periodicity_in_use_ns;
    /**< Actual transmission interval period (in nanoseconds) scheduled
         relative to 1PP 0:00.00 time. */

    uint16_t currently_reserved_periodic_bytes;
    /**< Actual number of bytes currently reserved at the MAC layer. This
         number can be slightly larger than original request. */

    uint32_t tx_reservation_offset_ns;
    /**< Actual offset, from a 1PPS pulse and Tx flow periodicity, that the MAC
         selected and is using for the transmit reservation.

         If data goes to the radio with enough time, it can be transmitted on
         the medium in the next immediately scheduled slot. @newpagetable */

} v2x_sps_mac_details_t;

/**
    Callback functions used in v2x_radio_tx_sps_sock_create_and_bind() function
    calls.
 */
typedef struct {

    /**
    Callback made upon completion of a reservation change a
    v2x_radio_tx_reservation_change() call initiated for a MAC/Phy contention.

    The current SPS offset and reservation parameter are passed in the details
    structure returned by the pointer.

    @param context  Pointer to the application context.
    @param details  Pointer to the MAC information.
    */
    void (*v2x_radio_l2_reservation_change_complete_cb)(void *context, v2x_sps_mac_details_t *details);

    /**
    Callback periodically made when the MAC SPS timeslot changes. The new
    reservation offset is in the details structure returned by pointer.

    This event can occur when a MAC contention triggers a new reservation time
    slot to be selected.

    This callback is relevant only to connections opened with
    v2x_radio_tx_sps_sock_create_and_bind().

    @param measurements  Pointer to the channel measurements.
    @param context       Pointer to the context.

    @newpage
    */
    void (*v2x_radio_sps_offset_changed)(void *context,
                                         v2x_sps_mac_details_t *details);

} v2x_per_sps_reservation_calls_t;

/**
    Method used to query the platform SDK for its version number, build
    information, and build date.

    @return
    v2x_api_ver_t -- Contains the build date and API version number. @newpage
 */
extern v2x_api_ver_t v2x_radio_api_version();

/**
    Gets the capabilities of a specific Radio interface attached to the system.

    @datatypes
    v2x_iface_capabilities_t

    @param[in] iface_name  Pointer to the Radio interface name. The Radio
                           interface is one of the following:
                           - An RmNet interface (HLOS)
                           - The interface supplied for IP communication
                           - The interface for non-IP communication (such as WSMP
                             and Geonetworking).
    @param[out] caps       Pointer to the v2x_iface_capabilities_t structure,
                           which contains the capabilities of this specific
                           interface.

    @return
    #V2X_STATUS_SUCCESS -- On success. The radio is ready for data-plane sockets
    to be created and bound.
    @par
    Error code -- On failure (see #v2x_status_enum_type).

    @dependencies
    An SPS flow must have been successfully initialized. @newpage
 */
extern v2x_status_enum_type v2x_radio_query_parameters(const char *iface_name, v2x_iface_capabilities_t *caps);

/**
    Initializes the Radio interface and sets the callback that will be used when
    events in the radio change (including when radio initialization is complete).

    @datatypes
    #v2x_concurrency_sel_t \n
    v2x_radio_calls_t

    @param[in] interface_name  Pointer to the NULL-terminated parameter that
                               specifies which Radio interface name caller is to
                               be initialized (the IP or non-IP interface of a
                               specific name). The Radio interface is one of the
                               following:
                               - An RmNet interface (HLOS)
                               - The interface supplied for IP communication
                               - The interface for non-IP communication (such as
                                 WSMP and Geonetworking).
    @param[in] mode            WAN concurrency mode, although the radio might not
                               support concurrency. Errors can be generated.
    @param[in] callbacks       Pointer to the v2x_radio_calls_t structure that is
                               prepopulated with function pointers used during
                               radio events (such as loss of time synchronization
                               or accuracy) for subscribers of interest. This
                               parameter also points to a callback for this
                               initialization function.
    @param[in] context         Voluntary pointer to the first parameter on the
                               callback.

    @detdesc
    This function call is a nonblocking, and it is a control plane action.
    @par
    Use v2x_radio_deinit() when radio operations are complete.
    @par
    @note1hang Currently, the channel and transmit power are not specified. They
    are specified with a subsequent call to v2x_radio_init_complete() when
    initialization is complete.

    @return
    Handle to the specified initialized radio. The handle is used for
    reconfiguring, opening or changing, and closing reservations.
    @par
    #V2X_RADIO_HANDLE_BAD -- Upon an error. No initialization callback is made.
    @newpage
 */
v2x_radio_handle_t v2x_radio_init(char *interface_name,
                                  v2x_concurrency_sel_t mode,
                                  v2x_radio_calls_t *callbacks,
                                  void *context);

/**
    Configures the MAC/Phy parameters on an initialized radio handle to
    either an IP or non-IP radio. Parameters include the source L2 address,
    channel, bandwidth, and transmit power.

    After the radio has been configured or changed, a callback to
    v2x_radio_macphy_change_complete_cb is made with the supplied context.

    @datatypes
    #v2x_radio_handle_t \n
    v2x_radio_macphy_params_t

    @param[in] handle   Identifies the initialized Radio interface.
    @param[in] macphy   Pointer to the MAC/Phy parameters to be configured.
    @param[in] context  Voluntary pointer to the context that will be supplied
                        as the first parameter in the callback.

    @return
    #V2X_STATUS_SUCCESS -- On success. The radio is now ready for data-plane
    sockets to be open and bound.
    @par
    Error code -- On failure (see #v2x_status_enum_type).

    @dependencies
    The interface must be pre-initialized with v2x_radio_init(). The handle from
    that function must be used as the parameter in this function. @newpage
 */
extern v2x_status_enum_type v2x_radio_set_macphy(v2x_radio_handle_t handle, v2x_radio_macphy_params_t *macphy,
        void *context);

/**
    De-initializes a specific Radio interface.

    @datatypes
    #v2x_radio_handle_t

    @param[in] handle  Handle to the Radio that was initialized.

    @returns
    Indication of success or failure from #v2x_status_enum_type.

    @dependencies
    The interface must be pre-initialized with v2x_radio_init(). The handle from
    that function must be used as the parameter in this function. @newpage
 */
extern v2x_status_enum_type v2x_radio_deinit(v2x_radio_handle_t handle);

/**
    Opens a new V2X radio receive socket, and initializes the given sockaddr
    buffer. The socket is also bound as an AF_INET6 UDP type socket.

    @datatypes
    #v2x_radio_handle_t

    @param[in] handle        Identifies the initialized Radio interface.
    @param[out] sock         Pointer to the socket that, on success, returns the
                             socket descriptor. The caller must release this
                             socket with close().
    @param[out] rx_sockaddr  Pointer to the IPv6 UDP socket. The sockaddr_in6
                             buffer is initialized with the IPv6 source address
                             and source port that are used for the bind.

    @detdesc
    You can execute any sockopts that are appropriate for this type of socket
    (AF_INET6).
    @par
    @note1hang The port number for the receive path is not exposed, but it is in
    the sockaddr_ll structure (if the caller is interested).

    @return
    0 -- On success.
    @par
    Otherwise:
     - EPERM -- Socket creation failed; for more details, check errno.h.
     - EAFNOSUPPORT -- On failure to find the interface.
     - EACCES -- On failure to get the MAC address of the device.

    @dependencies
    The interface must be pre-initialized with v2x_radio_init(). The handle from
    that function must be used as the parameter in this function. @newpage
 */
extern int v2x_radio_rx_sock_create_and_bind(v2x_radio_handle_t handle, int *sock, struct sockaddr_in6 *rx_sockaddr);

/**
    Creates and binds a socket with a bandwidth-reserved (SPS) Tx flow with the
    requested ID/priority/periodicity/size on a specified source port number.
    The socket is created as an IPv6 UDP socket.

    @datatypes
    #v2x_radio_handle_t \n
    v2x_tx_bandwidth_reservation_t \n
    v2x_per_sps_reservation_calls_t

    @param[in]  handle          Identifies the initialized Radio interface on
                                which this data connection is connected.
    @param[in]  res             Pointer to the parameter structure (how often it
                                is sent, how many bytes reserved, and so on).
    @param[in]  calls           Pointer to reservation callbacks/listeners. This
                                parameter is called when underlying radio MAC
                                parameters change related to the SPS bandwidth
                                contract. For example, the callback after a
                                reservation change, or if the timing offset of
                                the SPS adjusts itself in response to traffic.
                                This parameter passes NULL if no callbacks are
                                required.
    @param[in]  sps_portnum     Requested source port number for the bandwidth
                                reserved SPS transmissions.
    @param[in]  event_portnum   Requested source port number for the bandwidth
                                reserved event transmissions, or  -1 for no event
                                port.
    @param[out] sps_sock        Pointer to the socket that is bound to the
                                requested port for Tx with reserved bandwidth.
    @param[out] sps_sockaddr    Pointer to the IPv6 UDP socket. The sockaddr_in6
                                buffer is initialized with the IPv6 source
                                address and source port that are used for the
                                bind() function. The caller can then use the
                                buffer for subsequent sendto() function calls.
    @param[out] event_sock      Pointer to the socket that is bound to the
                                event-driven transmission port.
    @param[out] event_sockaddr  Pointer to the IPV6 UDP socket. The sockaddr_in6
                                buffer is initialized with the IPv6 source
                                address and source port that are used for the
                                bind() function. The caller can then use the
                                buffer for subsequent sendto() function calls.

    @detdesc
    The radio attempts to reserve the flow with the specified size and rate
    passed in the request parameters.
    @par
    This function is used only for Tx. It sets up two UDP sockets on the
    requested two HLOS port numbers.
    @par
    For only a single SPS flow, indicate the event port number by using a
    negative number or NULL for the event_sockaddr. For a single event-driven
    port, use v2x_radio_tx_event_sock_create_and_bind() instead.
    @par
    Because the modem endpoint requires a specific global address, all data sent
    on these sockets must have a configurable IPv6 destination address for the
    non-IP traffic.
    @par
    @note1hang The Priority parameter of the SPS reservation is used only for the
    reserved Tx bandwidth (SPS) flow. The non-SPS/event-driven data sent to the
    event_portnum parameter is prioritized on the air, based on the IPv67
    Traffic Class of the packet.
    @par
    The caller is expected to identify two unused local port numbers to use for
    binding: one for the event-driven flow and one for the SPS flow.
    @par
    This call is a blocking call. When it returns, the sockets are ready to used,
    assuming there is no error.

    @return
    0 -- On success.
    @par
    Otherwise:
     - EPERM -- Socket creation failed; for more details, check errno.h.
     - EAFNOSUPPORT -- On failure to find the interface.
     - EACCES -- On failure to get the MAC address of the device.

    @dependencies
    The interface must be pre-initialized with v2x_radio_init(). The handle from
    that function must be used as the parameter in this function. @newpage
*/
extern int v2x_radio_tx_sps_sock_create_and_bind(v2x_radio_handle_t handle,
        v2x_tx_bandwidth_reservation_t *res,
        v2x_per_sps_reservation_calls_t *calls,
        int sps_portnum,
        int event_portnum,
        int *sps_sock,
        struct sockaddr_in6 *sps_sockaddr,
        int *event_sock,
        struct sockaddr_in6 *event_sockaddr);

/**
    Adjusts the reservation for transmit bandwidth.

    @datatypes
    v2x_tx_bandwidth_reservation_t

    @param[out] sps_sock             Pointer to the socket bound to the requested
                                     port.
    @param[in]  updated_reservation  Pointer to a bandwidth reservation with
                                     new reservation information.

    @detdesc
    This function is used as follows:
    - When the bandwidth requirement changes in periodicity (for example, due to
        an application layer DCC algorithm)
    - Because the packet size is increasing (for example, due to a growing path
        history size in a BSM).
    @par
    When the reservation change is complete, a callback to the structure is
    passed at in a v2x_radio_init() call.

    @return
    #V2X_STATUS_SUCCESS -- On success.
    @par
    Error code -- If there is a problem (see #v2x_status_enum_type).

    @dependencies
    An SPS flow must have been successfully initialized with the
    v2x_radio_tx_sps_sock_create_and_bind() method.
 */
extern v2x_status_enum_type v2x_radio_tx_reservation_change(int *sps_sock,
        v2x_tx_bandwidth_reservation_t *updated_reservation);

/**
    Flushes the radio transmitter queue.

    This function is used for all packets on the specified interface that have
    not been sent yet. This action is necessary when a radio MAC address change
    is coordinated for anonymity.

    @param[in] interface  Name of the Radio interface operating system.

    @returns
    None. @newpage
 */
extern void v2x_radio_tx_flush(char *interface);

/**
    Opens and binds an event-driven socket (one with no bandwidth reservation).

    @param[in]  interface        Pointer to the operating system name to use.
                                 This interface is an RmNet interface (HLOS).
    @param[in]  v2x_id           Used for transmissions that are ultimately
                                 mapped to an L2 destination address.
    @param[in]  event_portnum    Local port number to which the socket is
                                 bound. Used for transmissions of this ID.
    @param[out] event_sock_addr  Pointer to the sockaddr_ll structure buffer
                                 to be initialized.
    @param[out] sock             Pointer to the file descriptor. Loaded when
                                 the function is successful.

    @detdesc
    This function is used only for Tx when no periodicity is available for the
    application type. If your transmit data periodicity is known, use
    v2x_radio_tx_sps_sock_create_and_bind() instead.
    @par
    These event-driven sockets pay attention to QoS parameters in the IP socket.

    @return
    0 -- On success
    @par
    Otherwise:
     - EPERM -- Socket creation failed; for more details, check errno.h.
     - EAFNOSUPPORT -- On failure to find the interface.
     - EACCES -- On failure to get the MAC address of the device. @newpage
 */
extern int v2x_radio_tx_event_sock_create_and_bind(const char *interface,
        int v2x_id,
        int event_portnum,
        struct sockaddr_in6 *event_sock_addr,
        int *sock);

/**
    Requests a channel utilization (CBP/CBR) measurement result on a
    channel. This function uses the callbacks passed in during initialization to
    deliver the measurements. Measurement callbacks continue until the Radio
    interface is closed.

    @datatypes
    #v2x_radio_handle_t \n
    v2x_chan_meas_params_t

    @param[in] handle            Handle to the port.
    @param[in] measure_this_way  Indicates how and what to measure, and how often
                                 to send results. Some higher-level standards
                                 (like J2945/1 and ETSI TS102687 DCC) have
                                 specific time windows and items to measure.

    @return
    #V2X_STATUS_SUCCESS -- Upon success. The radio is now ready for data-plane
    sockets to be created and bound.

    @dependencies
    The interface must be pre-initialized with v2x_radio_init(). The handle from
    that function must be used as the parameter in this function. @newpage
 */
extern v2x_status_enum_type v2x_radio_start_measurements(v2x_radio_handle_t handle,
        v2x_chan_meas_params_t *measure_this_way);

/**
    Discontinues any periodic MAC/Phy channel measurements and the reporting of
    them via listener calls.

    @datatypes
    #v2x_radio_handle_t

    @param[in] handle  Handle to the radio measurements to be stopped.

    @return
    #V2X_STATUS_SUCCESS -- On success.

    @dependencies
    The measurements must have been started with v2x_radoi_start_measurements().
    @newpage
 */
extern v2x_status_enum_type v2x_radio_stop_measurements(v2x_radio_handle_t handle);

/**
    Closes a specified socket file descriptor and deregisters any modem resources
    associated with it (such as reserved SPS bandwidth contracts). This
    function works on receive, SPS, or event driven sockets.

    The socket file descriptor must be closed when the client exits. It is recommended
    to use a trap to catch controlled shutdowns.

    @param[in] sock_fd  Socket file descriptor.

    @return
    Integer value of close(sock).
 */
extern int v2x_radio_sock_close(int *sock_fd);

/**
 *  Configure the V2X log level and destination for SDK and lower layers
 *
 *  @param[in] new_level  set the log level to one of the standard syslog levels (LOG_ERR, LOG_INFO, etC)
 *  @param[in] if zero, send to stdout.  if otherwise send to syslog
 *
 */
extern void v2x_radio_set_log_level(int new_level, int use_syslog);

/**
 *  poll for recent V2X status.  Does not generate any modem control traffic, but rather for efficiency, just returns 
 *  most recently cached value that was reported form the modem (often repoted at high/frequent rate from Modem
 *
 *  @param[out]  the age in microseconds of the last event (radio status) that is being reported
 *  @return,  the Status Active, suspended, etc
 */
extern v2x_event_t cv2x_status_poll(uint64_t *status_age_useconds);

/**
 * Testing functions mainly for sim environment
 * but also useful for IPV6 testing
 */
extern void v2x_set_dest_ipv6_addr(char *new_addr);

extern void v2x_set_dest_port(uint16_t portnum);

extern void v2x_set_rx_port(uint16_t portnum);

extern void v2x_disable_socket_connect();

/** @} *//* end_addtogroup v2x_api_radio */

#ifdef __cplusplus
}
#endif

#endif // __V2X_RADIO_APIS_H__
