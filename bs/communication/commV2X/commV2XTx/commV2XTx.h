#ifndef COMM_V2X_TX_H
#define COMM_V2X_TX_H

#ifdef COMM_V2X_TX_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* COMM_V2X_TX_C */


PUBLIC void v2xTxProcess(uint8_t* (*v2xTxBufGet)(uint16_t *bufLen, uint64_t time));

#undef PUBLIC

#endif /* COMM_V2X_TX_H */

/* end of file */
