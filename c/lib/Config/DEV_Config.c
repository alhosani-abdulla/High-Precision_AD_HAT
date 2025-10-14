/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-12-21
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of theex Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "/home/peterson/High-Pricision_AD_HAT_1/c/lib/Config/DEV_Config.h"
#include <fcntl.h>

#define RPI
#define USE_DEV_LIB

/**
 * GPIO read and write
**/
void DEV_Digital_Write(UWORD Pin, UBYTE Value)
{
#ifdef RPI
#ifdef USE_DEV_LIB
	SYSFS_GPIO_Write(Pin, Value);
#endif
#endif
}

UBYTE DEV_Digital_Read(UWORD Pin)
{
	UBYTE Read_value = 0;
#ifdef RPI
#ifdef USE_DEV_LIB
	Read_value = SYSFS_GPIO_Read(Pin);
	//printf("DRDY Output: %u\n", Read_value);
#endif
#endif
	//printf("PIN: %d\n", Pin);
	//printf("Read value: %u", Read_value);
	return Read_value;
}

/**
 * SPI
**/
UBYTE DEV_SPI_WriteByte(uint8_t Value)
{
	UBYTE temp = 0;
	//printf("I'M IN DEV_SPI_WRITEBYTE\n");
	// printf("write %x \r\n", Value);
#ifdef RPI
#ifdef USE_DEV_LIB
	//printf("I'M IN USE_DEV_LIB IN DEV_SPI_WRITEBYTE");
	temp = DEV_HARDWARE_SPI_TransferByte(Value);
#endif
#endif
	// printf("Read %x \r\n", temp);
	//printf("I'M OUT OF DEV_SPI_WRITEBYTE\n");
	return temp;
}

UBYTE DEV_SPI_ReadByte(void)
{
	return DEV_SPI_WriteByte(0x00);
}

/**
 * GPIO Mode
**/
void DEV_GPIO_Mode(UWORD Pin, UWORD Mode)
{
#ifdef RPI
#ifdef USE_DEV_LIB
	printf("About to export in DEV_GPIO_Mode...\n");
	//SYSFS_GPIO_Export(Pin);
	if(Mode == 0 || Mode == SYSFS_GPIO_IN) {
		SYSFS_GPIO_Direction(Pin, SYSFS_GPIO_IN);
		// Debug("IN Pin = %d\r\n",Pin);
	} else {
		SYSFS_GPIO_Direction(Pin, SYSFS_GPIO_OUT);
		// Debug("OUT Pin = %d\r\n",Pin);
	}
#endif
#endif
}

/**
 * delay x ms
**/
void DEV_Delay_ms(UDOUBLE xms)
{
#ifdef RPI
#ifdef USE_DEV_LIB
	UDOUBLE i;
	for(i=0; i < xms; i++) {
		usleep(1000);
	}
#endif
#endif
}

static int DEV_Equipment_Testing(void)
{
	int i;
	int fd;
	char value_str[20];
	fd = open("/etc/issue", O_RDONLY);
	printf("%d\n", fd);
    printf("Current environment: ");
	while(1) {
		if (fd < 0) {
			Debug( "Read failed Pin\n");
			return -1;
		}
		for(i=0;; i++) {
			if (read(fd, &value_str[i], 1) < 0) {
				Debug( "failed to read value!\n");
				return -1;
			}
			if(value_str[i] ==32) {
				printf("\r\n");
				break;
			}
			printf("%c",value_str[i]);
		}
		break;
	}
#ifdef RPI
	if(i<5) {
		printf("Unrecognizable\r\n");
	} else {
		char RPI_System[10]   = {"Debian"};
		for(i=0; i<6; i++) {
			if(RPI_System[i]!= value_str[i]) {
				printf("Please make JETSON !!!!!!!!!!\r\n");
				return -1;
			}
		}
	}
#endif
	return 0;
}

void DEV_GPIO_Init(UWORD DEV_RST_PIN, UWORD DEV_CS_PIN, UWORD DEV_DRDY_PIN)
{
	DEV_GPIO_Mode(DEV_RST_PIN, 1);
	DEV_GPIO_Mode(DEV_CS_PIN, 1);
	
	DEV_GPIO_Mode(DEV_DRDY_PIN, 0);
	
	DEV_Digital_Write(DEV_CS_PIN, 1);
}

/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(UWORD DEV_RST_PIN, UWORD DEV_CS_PIN, UWORD DEV_DRDY_PIN)
{
    printf("/***********************************/ \r\n");
	if(DEV_Equipment_Testing() < 0) {
		return 1;
	}
#ifdef RPI
#ifdef USE_DEV_LIB
	printf("Write and read /dev/spidev0.0 \r\n");
	DEV_GPIO_Init(DEV_RST_PIN, DEV_CS_PIN, DEV_DRDY_PIN);
	DEV_HARDWARE_SPI_begin("/dev/spidev0.0");
    DEV_HARDWARE_SPI_setSpeed(2000000);
	DEV_HARDWARE_SPI_Mode(SPI_MODE_1);
#endif
#endif
    printf("/***********************************/ \r\n");
	return 0;
}

/******************************************************************************
function:	Module exits, closes SPI and BCM2835 library
parameter:
Info:
******************************************************************************/
void DEV_Module_Exit(UWORD DEV_RST_PIN, UWORD DEV_CS_PIN)
{
#ifdef RPI
#ifdef USE_DEV_LIB
	DEV_HARDWARE_SPI_end();
	DEV_Digital_Write(DEV_RST_PIN, 0);
	DEV_Digital_Write(DEV_CS_PIN, 0);
#endif
#endif
}
