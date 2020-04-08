
#ifndef COMM_USART_H
#define COMM_USART_H

#ifdef COMM_USART_H
  #define PUBLIC
#else
  #define PUBLIC extern
#endif	/* COMM_USART_H */


PUBLIC uint8_t *get_usart_info();
PUBLIC int UsartUpdateInfo(void);

#undef PUBLIC

#endif /* COMM_CAN_H */

/* end of file */
