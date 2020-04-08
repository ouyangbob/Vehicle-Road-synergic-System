#ifndef ITS_MANAGER_H
#define ITS_MANAGER_H

#ifdef ITS_MANAGER_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* ITS_MANAGER_C */
#include "../../communication/proto/test.pb.h"

PUBLIC void itsProcessCreate(void);
PUBLIC proto_RsuInfoRequest *rsuInfoGet(void);

#undef PUBLIC

#endif /* ITS_MANAGER_H */

/* end of file */
