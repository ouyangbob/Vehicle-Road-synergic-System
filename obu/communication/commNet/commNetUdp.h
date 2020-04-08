#ifndef COMM_NET_UDP_H
#define COMM_NET_UDP_H

#ifdef COMM_NET_UDP_C
#define PUBLIC
#else
#define PUBLIC extern
#endif /* COMM_NET_UDP_C */
#include "../../TargetSettings.h"

PUBLIC int UdpServerProcess(void);
PUBLIC int UdpRecvProcess(void);
PUBLIC pthread_mutex_t mutex1;
PUBLIC pthread_mutex_t mutex2;

#undef PUBLIC

#endif /* COMM_NET_UDP_H */

/* end of file */
