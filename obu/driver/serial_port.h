    /******************** (C) COPYRIGHT 2016 VINY **********************************
     * File Name          : serial_port.h
     * Author             : Hongyuan
     * Date First Issued  : 2016/06/06
     * Description        : This file contains the software implementation for the
     *                      serial port unit
     *******************************************************************************
    * History:
    * DATE       | VER   | AUTOR      | Description
    * 2016/06/06 | v1.0  | Hongyuan   | initial released

    *******************************************************************************/
    #ifndef SERIAL_PORT_H_
    #define SERIAL_PORT_H_

    #include<stdlib.h>
    #include<stdio.h>
    #include<fcntl.h>          /*File control*/
    #include<unistd.h>
    #include<errno.h>
    #include<string.h>
    #include<termio.h>         /*provide the common interface*/
    #include<sys/types.h>
    #include<sys/stat.h>


    /* 防止重复变量定义 */
    #ifndef _SERIAL_PORT_H_
    #define _SERIAL_PORT_H_

    #define FALSE -1
    #define TRUE  0

    typedef unsigned char  U8; 	    //1字节（ARM,X86）
    typedef unsigned short U16;     //2字节（ARM,X86）
    typedef unsigned long  U32;     //4字节（ARM,X86）
    typedef signed   char  INT8;    //1字节（ARM,X86）
    typedef unsigned int   UINT16;  //4字节（ARM,X86）
    typedef signed   int   INT16;   //4字节（ARM,X86）

    #endif/*SERIAL_PORT_H_*/

    /*定义串行端口宏定义*/
    #define SERIAL_PORT_DEBUG 		"/dev/ttymxc0" //UART1为调试串口
    #define SERIAL_PORT_1     		"/dev/ttymxc1" //UART2发送 9，接收10
    #define SERIAL_PORT_2	  	    "/dev/ttymxc2" //UART3发送13，接收14
    #define SERIAL_PORT_3	  	    "/dev/ttymxc3" //UART4发送15，接收17
    #define SERIAL_PORT_4	  	    "/dev/ttymxc4" //UART5发送16，接收18

    INT8 open_serial_port(const char *path);
    INT8 serial_port_init(INT8 port_fd, U32 baud_rate, U8 data_bits, U8 parity, U8 stop_bit);
    INT8 close_port(INT8 port_fd);


    #endif/*SERIAL_PORT_H_*/
