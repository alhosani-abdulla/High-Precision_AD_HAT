#include <stdlib.h>     //exit()
#include <signal.h>     //signal()
#include <time.h>
#include "/home/peterson/High-Pricision_AD_HAT/c/lib/Driver/ADS1263.h"
#include "stdio.h"
#include <string.h>

#define RPI
#define USE_DEV_LIB
#define REF         5.08        //Modify according to actual voltage
                                //external AVDD and AVSS(Default), or internal 2.5V

void  Handler(int signo)
{
    //System Exit
    printf("\r\n END \r\n");
    DEV_Module_Exit();
    exit(0);
}

int main(void)
{
    UDOUBLE ADC[10];
    UWORD i;
    double RES, TEMP;
    
    // Exception handling:ctrl + c
    signal(SIGINT, Handler);
    
    printf("ADS1263 Demo \r\n");
    DEV_Module_Init(18, 12, get_DRDYPIN(12));
    DEV_Module_Init(18, 22, get_DRDYPIN(22));
    DEV_Module_Init(18, 23, get_DRDYPIN(23));

    // 0 is singleChannel, 1 is diffChannel
    ADS1263_SetMode(0);
    
    // The faster the rate, the worse the stability
    // and the need to choose a suitable digital filter(REG_MODE1)
    //doing 3 times to set up the 3 ADCs
    if(ADS1263_init_ADC1(ADS1263_38400SPS, 12) == 1) {
        printf("\r\n END \r\n");
        DEV_Module_Exit();
        exit(0);
    }
    if(ADS1263_init_ADC1(ADS1263_38400SPS, 22) == 1) {
        printf("\r\n END \r\n");
        DEV_Module_Exit();
        exit(0);
    }
    if(ADS1263_init_ADC1(ADS1263_38400SPS, 23) == 1) {
        printf("\r\n END \r\n");
        DEV_Module_Exit();
        exit(0);
    }
    
    printf("TEST_ADC1\r\n");
    
    #define ChannelNumber 10
    UBYTE ChannelList[ChannelNumber] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};    // The channel must be less than 10
        
    UDOUBLE Value[ChannelNumber] = {0};
    while(1) {
        ADS1263_GetAll(ChannelList, Value, ChannelNumber, 12, get_DRDYPIN(12));  // Get ADC1 value
        ADS1263_GetAll(ChannelList, Value, ChannelNumber, 22, get_DRDYPIN(22));  // Get ADC1 value
        ADS1263_GetAll(ChannelList, Value, ChannelNumber, 23, get_DRDYPIN(23));  // Get ADC1 value
        for(i=0; i<ChannelNumber; i++) {
            if((Value[i]>>31) == 1)
                printf("IN%d is -%lf \r\n", ChannelList[i], REF*2 - Value[i]/2147483648.0 * REF);      //7fffffff + 1
            else
                printf("IN%d is %lf \r\n", ChannelList[i], Value[i]/2147483647.0 * REF);       //7fffffff
        }
        for(i=0; i<ChannelNumber; i++) {
            printf("\33[1A");   // Move the cursor up
        }
    }
    printf("TEST SUCCESSFUL!");
    return 0;
}
