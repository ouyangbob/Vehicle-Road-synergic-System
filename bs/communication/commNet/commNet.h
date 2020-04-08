#ifndef COMM_NET_H
#define COMM_NET_H

#ifdef COMM_NET_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* COMM_NET_C */

PUBLIC void uploadCanUwbProcess();
PUBLIC void initModuleCommNet(void);
PUBLIC void uploadProcess(proto_ObuTrafficLightsRequest* (obuTrafficInfoGet)(void)); 
PUBLIC void uploadRoadBlockProcess(proto_RoadBlockRequest *(RoadBlockInfoGet)(void));

#undef PUBLIC

#endif /* COMM_NET_H */

/* end of file */
