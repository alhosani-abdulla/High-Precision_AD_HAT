/*****************************************************************************
* | File        :   SYSFS_GPIO.c
* | Author      :   Waveshare team
* | Function    :   Drive SYSFS_ GPIO
* | Info        :   Read and write /sys/class/gpio
*----------------
* |	This version:   V1.0
* | Date        :   2019-06-04
* | Info        :   Basic version
*
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# SYSFS_GPIO_IN the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the folSYSFS_GPIO_LOWing conditions:
#
# The above copyright notice and this permission notice shall be included SYSFS_GPIO_IN
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. SYSFS_GPIO_IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER SYSFS_GPIO_IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# SYSFS_GPIO_OUT OF OR SYSFS_GPIO_IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS SYSFS_GPIO_IN
# THE SOFTWARE.
#
******************************************************************************/
/*
#include "/home/peterson/High-Pricision_AD_HAT_1/c/lib/Config/RPI_sysfs_gpio.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
*/

#include "/home/peterson/High-Pricision_AD_HAT_1/c/lib/Config/RPI_sysfs_gpio.h"
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONSUMER "gpiod_gpio"
#define CHIP_NAME "gpiochip0"

//keep track of line handles in a global array
static struct gpiod_chip *chip = NULL;
static struct gpiod_line *lines[64] = {NULL};

//REWRITE USING LIBGPIOD

int SYSFS_GPIO_Init()
{
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
    
    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip) {
        perror("Open chip failed");
        return -1;
    }
    return 0;
    
    /* OLD CODE 
    char buffer[NUM_MAXBUF];
    int len;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    printf("fd_export: %d\n", fd);
    if (fd < 0) {
        printf( "Export Failed: Pin%d\n", Pin);
        return -1;
    }

    len = snprintf(buffer, NUM_MAXBUF, "%d", Pin);
    write(fd, buffer, len);
    
    printf( "Export: Pin%d\r\n", Pin);

    close(fd);
    return 0;
    */
}

int SYSFS_GPIO_Release()
{   
    for (int i = 0; i < 64; ++i) {
        //printf("Line: %d", lines[i]);
        if (lines[i]) {
            gpiod_line_release(lines[i]);
            lines[i] = NULL;
        }
    }
    
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
    
    return 0;
    
    /* OLD CODE
    char buffer[NUM_MAXBUF];
    int len;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        printf( "unexport Failed: Pin%d\n", Pin);
        return -1;
    }

    len = snprintf(buffer, NUM_MAXBUF, "%d", Pin);
    write(fd, buffer, len);
    
    printf( "Unexport: Pin%d\r\n", Pin);
    
    close(fd);
    return 0;
    */
}

int SYSFS_GPIO_Direction(int Pin, int Dir)
{
    if (!chip) return -1;
    
    lines[Pin] = gpiod_chip_get_line(chip, Pin);
    if (!lines[Pin]) {
        printf("Get line failed for pin %d\n", Pin);
        return -1;
    }
    
    int ret;
    if (Dir == 0) {
        ret = gpiod_line_request_input(lines[Pin], CONSUMER);
        if (ret < 0) {
            perror("gpiod_line_request_input");
        }
    } else {
        ret = gpiod_line_request_output(lines[Pin], CONSUMER, 0);
        if (ret < 0) {
            perror("gpiod_line_request_output");
        }
    }
    
    if (ret < 0) {
        printf("Request %s failed for pin %d\n", Dir == 0 ? "input" : "output", Pin);
        return -1;
    }
    printf("SUCCESSFULLY SET DIRECTION\n");
    
    return 0;
    
    /* OLD CODE
    const char dir_str[]  = "in\0out";
    char path[DIR_MAXSIZ];
    int fd;
    
    snprintf(path, DIR_MAXSIZ, "/sys/class/gpio/gpio%d/direction", Pin);
    fd = open(path, O_WRONLY);
    printf("fd_direction: %d\n", fd);
    if (fd < 0) {
       printf( "Set Direction failed: Pin%d\n", Pin);
        return -1;
    }

    if (write(fd, &dir_str[Dir == SYSFS_GPIO_IN ? 0 : 3], Dir == SYSFS_GPIO_IN ? 2 : 3) < 0) {
        printf("failed to set direction!\r\n");
        return -1;
    }

    if(Dir == SYSFS_GPIO_IN){
        printf("Pin%d:intput\r\n", Pin);
    }else{
        printf("Pin%d:Output\r\n", Pin);
    }
    
    close(fd);
    return 0;
    */
}

int SYSFS_GPIO_Read(int Pin)
{
    if (!lines[Pin]) {
        printf("Pin %d not initialized\n", Pin);
        return -1;
    }
    
    //printf("WE ARE NOT FAILING OUR READ\n");
    
    return gpiod_line_get_value(lines[Pin]);
    
    /* OLD CODE
    char path[DIR_MAXSIZ];
    char value_str[3];
    int fd;
    
    snprintf(path, DIR_MAXSIZ, "/sys/class/gpio/gpio%d/value", Pin); //isolated it to this line
    fd = open(path, O_RDONLY); //this guy is reading -1
    printf("fd_read: %d\n", fd);
    if (fd < 0) {
        printf("Read failed Pin%d\n", Pin);
        printf( "Read failed Pin%d\n", Pin);
        return -1;
    }

    if (read(fd, value_str, 3) < 0) {
        printf( "failed to read value!\n");
        return -1;
    }

    close(fd);
    printf("SYSFS_GPIO_Read Sucess\n");
    return(atoi(value_str));
    */
}

int SYSFS_GPIO_Write(int Pin, int value)
{
    if (!lines[Pin]) {
        printf("Pin %d not initialized\n", Pin);
        return -1;
    }
    
    //printf("WE ARE NOT FAILING OUR WRITE\n");
    
    return gpiod_line_set_value(lines[Pin], value);
    
    /* OLD CODE
    const char s_values_str[] = "01";
    char path[DIR_MAXSIZ];
    int fd;
    
    snprintf(path, DIR_MAXSIZ, "/sys/class/gpio/gpio%d/value", Pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        printf( "Write failed : Pin%d,value = %d\n", Pin, value);
        return -1;
    }

    if (write(fd, &s_values_str[value == SYSFS_GPIO_LOW ? 0 : 1], 1) < 0) {
        printf( "failed to write value!\n");
        return -1;
    }
    
    close(fd);
    return 0;
    */
}
