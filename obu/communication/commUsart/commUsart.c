

#include <stdarg.h>
#include <termio.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include "../commNet/commNetUdp.h"

static uint8_t r_buf[76] = {0};

#define DEVICE_TTYS "/dev/ttymxc2"

uint8_t *get_usart_info()
{
	//char *ptr = r_buf;
	//memcpy(ptr, "$GPGGA,190717.00,24.4842,N,118.1970,E,1,05,2.0,45.9,M,-5.7,M,0.0,0000*78", 76);
	//printf("GSP date = %s \n", r_buf);
	if(strlen(r_buf) == 0) {
		printf("no uwb!!!\n");
		return NULL;
	}
	return r_buf;
}

void init_ttyS(int fd)
{
	struct termios options;
	bzero(&options, sizeof(options));
	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);
	options.c_cflag |= (CS8 | CREAD);
	options.c_iflag = IGNPAR;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &options);
}

int send_cmd(int fd, char *send_buf)
{
	ssize_t ret;
	ret = write(fd, send_buf, strlen(send_buf));
	if (ret == -1)
	{
		printf("write device %s error\n", DEVICE_TTYS);
		return -1;
	}

	return 1;
}
/* 
int recv_cmd(int fd)  
{  
    ssize_t ret = 0;
    char recv_buf[100]="";  
    int iocount = sizeof(recv_buf);
    int iores = 0;
	
    iores = read(fd,recv_buf,iocount);
    if ( iores < 0)
	printf("==iores < 0==\n");
    printf("=== %s \n",recv_buf);  
    return 1;  
}  
*/

int recv_cmd(int fd)
{
	ssize_t ret = 0;
	// memset(r_buf,0, sizeof(r_buf));
	char *ptr = r_buf;
	int iocount = sizeof(r_buf);
	int iores = 0;
	pthread_mutex_lock(&mutex1);
	while (iocount > 0)
	{
		iores = read(fd, ptr, iocount);
		if (iores < 0)
			printf("==iores < 0==\n");
		// printf("recv count = %d\n", iores);
		if(iores == 0)
		{
			continue;
		}
		iocount -= iores;
		ptr += iores;
	}
	pthread_mutex_unlock(&mutex1);
	printf("GSP date = %s \n", r_buf);
	return 1;
}

int UsartUpdateInfo(void)
{
	int fd;
	char *send_buf = "hello imx6";
	fd = open(DEVICE_TTYS, O_RDWR);
	if (fd == -1)
	{
		printf("open device %s error\n", DEVICE_TTYS);
	}
	else
	{
		init_ttyS(fd);
	}
	while (1)
	{
		send_cmd(fd, send_buf);
		recv_cmd(fd);
	}
	return 0;
}
