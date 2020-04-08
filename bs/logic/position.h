
#ifndef POSITION_H
#define POSITION_H

#include "../communication/proto/test.pb.h"
#ifdef POSITION_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* POSITION_C */

// PUBLIC proto_ObuTrafficLightsRequest* chooseTrafficInfo(proto_RsuInfoRequest *stRsuInfoRequest);
PUBLIC void gpsInfoUpdateSetup(void);
PUBLIC proto_position_data_t *carLocationGet(void);
PUBLIC void uwbDataTest(void);
proto_position_data_t *carLocationGet(void);
#undef PUBLIC

#endif /* POSITION_H */

/* end of file */
