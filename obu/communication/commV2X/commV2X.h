#ifndef COMM_V2X_H
#define COMM_V2X_H

// Acme message contents for rx
typedef struct _acme_message_r {
    char v2x_family_id;     /* 头码：'O'代表OBU,'R'代表RSU,'B'代表路障，'S',代表限速牌 */
    char equipment_id;      /* 设备ID */
    bool has_seq_num;       /* 接收序号使能(移远提供代码里自己操作，计算丢包率用) */
    uint16_t seq_num;       /* 接收序号(移远提供代码里自己操作，计算丢包率用) */
    bool has_timestamp;     /* 时间戳使能(移远提供代码里自己操作，计算丢包率用) */
    uint64_t timestamp;     /* 时间戳(移远提供代码里自己操作，计算丢包率用) */
    uint8_t *buf_ptr;       /* 有效数据指针 */
    uint8_t buf_len;        /* 有效数据长度 */
} acme_message_r;

extern void v2xTxProcess(uint8_t* (*v2xTxBufGet)(uint16_t *bufLen, uint64_t time));
extern void v2xRxProcess(void);
extern void v2xRxInit(void (*v2xMsgGotFuncPtr)(acme_message_r msg));
extern void v2xProcessInit(void);

#endif /* COMM_V2X_H */

/* end of file */
