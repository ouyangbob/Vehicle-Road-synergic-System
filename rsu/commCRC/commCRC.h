#ifndef COMM_CRC_H
#define COMM_CRC_H

#ifdef COMM_CRC_C
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* COMM_CRC_C */


PUBLIC uint16_t MbRTUCRC(uint8_t *puchMsg , uint16_t usDataLen);


#undef PUBLIC

#endif /* COMM_CRC_H */

/* end of file */
