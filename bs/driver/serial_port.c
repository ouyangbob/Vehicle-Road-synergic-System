/******************** (C) COPYRIGHT 2016 VINY **********************************
 * File Name          : serial_port.c
 * Author             : Hongyuan
 * Date First Issued  : 2016/06/06
 * Description        : This file contains the software implementation for the
 *                      serial port unit
 *******************************************************************************
 * History:
 * DATE       | VER   | AUTOR      | Description
 * 2016/06/06 | v1.0  | Hongyuan   | initial released

 *******************************************************************************/
#include "serial_port.h"


/*************************************************
 * Function: open_port()
 * Description: 打开串口
 * Calls: none
 * Called By: none
 * Input: serial port device inode path
 * Output: print the error info
 * Return: sucessful return port_fd, failed Return -1
 * Author: River
 * History: <author> <date>  <desc>
 * Others: none
 *************************************************/
INT8 open_serial_port(const char *path)
{
	INT8 port_fd;

	/*打开串口*/
	//port_fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	port_fd = open(path, O_RDWR | O_NOCTTY);
	if (-1 == port_fd)
	{
		perror("open the serial port failed!!!\n");
		return FALSE;
	}
#if 0
	/*设置串口为fei阻塞状态*/
	if (fcntl(port_fd, F_SETFL, FNDELAY) < 0)
	{
		printf("fcntl failed!\n");
	}
	else
	{
		printf("fcntl = %d \n", fcntl(port_fd, F_SETFL, 0));
	}/*end if*/
#endif
	return port_fd;
}


/*************************************************
 * Function: serialport_init()
 * Description: 初始化串口
 * Calls: none
 * Called By: main
 * Input: port_fd baud_rate data_bits parity stop_bit
 * Output: print the error info
 * Return: 0 is sucessful, Other values is failed
 * Author: River
 * History: <author> <date>  <desc>
 *
 * Others: none
 *************************************************/
INT8 serial_port_init(INT8 port_fd, U32 baud_rate, U8 data_bits, U8 parity, U8 stop_bit)
{
	struct termios newtio, oldtio;

	/*保存并测试现有串口参数设置*/
	if (tcgetattr(port_fd, &oldtio )!= 0)
	{
		perror("setup serial failed!!!\n");
		return -6;
	}

	bzero(&newtio, sizeof(newtio));

	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;

	/*设置数据位*/
	switch (data_bits)
	{
		case 7:
				newtio.c_cflag |= CS7;
				break;
		case 8:
				newtio.c_cflag |= CS8;
				break;
		default:
				fprintf(stderr, "Unsupported data size\n");
				return -5;
	}

	/*设置奇偶校验位*/
	switch (parity)
	{
		case 'o':
		case 'O':/*odd number*/
				newtio.c_cflag |= PARENB;
				newtio.c_cflag |= PARODD;
				newtio.c_iflag |= (INPCK | ISTRIP);
				break;
		case 'e':
		case 'E':/*even number*/
				newtio.c_iflag |= (INPCK | ISTRIP);
				newtio.c_cflag |= PARENB;
				newtio.c_cflag &= ~PARODD;
				break;
		case 'n':
		case 'N':
				newtio.c_cflag &= ~PARENB;
				break;

		default:
				fprintf(stderr,"Unsupported parity\n");
				return -4;
	}

	/*设置波特率*/
	switch (baud_rate)
	{
		case 9600:
			cfsetispeed(&newtio, B9600);
			cfsetospeed(&newtio, B9600);
			break;

		case 115200:
			cfsetispeed(&newtio, B115200);
			cfsetospeed(&newtio, B115200);
			break;

		default:
			fprintf(stderr,"Unsupported baud rate\n");
			return -3;
	}

	/*设置停止位*/
	if (1 == stop_bit)
	{
		newtio.c_cflag &= ~CSTOPB;
	}
	else if (2 == stop_bit)
	{
		newtio.c_cflag |= CSTOPB;
	}
	else
	{
		fprintf(stderr,"Unsupported stop bits\n");
		return -2;
	}

	/*设置等待时间和最小接收字符*/
	//newtio.c_cc[VTIME] = 10; // 1 seconds
	//newtio.c_cc[VMIN] = 0;
	newtio.c_cc[VTIME] = 2;
	newtio.c_cc[VMIN] = 14;
	/*处理未接收字符*/
	tcflush(port_fd, TCIFLUSH);

	/*激活新配置*/
	if ((tcsetattr(port_fd, TCSANOW, &newtio)) != 0)
	{
		perror("serial port set error!!!\n");
		return -1;
	}

	printf("serial port set done!!!\n");
	return 0;
}

/*************************************************
 * Function: close_port(INT8 port_fd)
 * Description:  close the serial port
 * Calls: none
 * Called By: main
 * Input: port_fd
 * Output: prompt information
 * Return: TURE/FALSE
 * Author: River
 * History: <author> <date>  <desc>
 * Others: none
 ************************************************/
INT8 close_port(INT8 port_fd)
{
	if (close(port_fd) < 0)
	{
		printf("close the serial port failed!\n");
		return FALSE;
	}
	else
	{
		printf("close the serial port success\n");
		return TRUE;
	}/*end if*/
}
