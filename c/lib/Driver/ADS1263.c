/*****************************************************************************
* | File        :   ADS1263.c
* | Author      :   Waveshare team
* | Function    :   ADS1263 driver
* | Info        :
*----------------
* | This version:   V1.0
* | Date        :   2020-10-28
* | Info        :
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
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
#include <stdio.h>
#include <stdlib.h>
#include "/home/peterson/High-Pricision_AD_HAT_1/c/lib/Driver/ADS1263.h"
#include <time.h>

UBYTE ScanMode = 0;


/******************************************************************************
function:   Get DRDY pin
parameter:
Info:
******************************************************************************/
int get_DRDYPIN(UWORD DEV_CS_PIN){
    UWORD drdyPin = 0;
    if (DEV_CS_PIN == 12){
        drdyPin = 16;
    }
    if (DEV_CS_PIN == 22){
        drdyPin = 17;
    }
    if (DEV_CS_PIN == 23){
        drdyPin = 25;
    }
    return drdyPin;
}

/******************************************************************************
function:   Get value of state and return as a buffer
parameter:
Info:
******************************************************************************/

char *READ_CALIBRATION_STATE(void)
{
    char *buf = malloc(64);
    if (!buf) return NULL;
    
    int bit1 = DEV_Digital_Read(21);
    int bit2 = DEV_Digital_Read(20);
    int bit3 = DEV_Digital_Read(16);
    int bit4 = DEV_Digital_Read(12);
    
    //below is the ugly boolean code lol
    if (bit4) {
        if (bit1 && bit2 && bit3){
            buf[0] = '9';
            buf[1] = '\0';
            return buf;
        } else {
            printf("Error: invalid state.");
        }
    } else {
        if (bit1) {
            if (bit2 && bit3) {
                buf[0] = '7';
            } else if (bit3) {
                buf[0] = '5';
            } else if (bit2) {
                buf[0] = '3';
            } else {
                buf[0] = '1';
            }
        } else if (bit2) {
            if (bit3) {
                buf[0] = '6';
            } else {
                buf[0] = '2';
            }
        } else if (bit3) {
            buf[0] = '4';
        } else {
            buf[0] = '0';
        }
    }
    
    buf[1] = '\0';
    return buf;
}

/******************************************************************************
function:   Module reset
parameter:
Info:
******************************************************************************/
void ADS1263_reset(UWORD DEV_RST_PIN)
{
    DEV_Digital_Write(DEV_RST_PIN, 1);
    DEV_Digital_Write(DEV_RST_PIN, 0);
    DEV_Digital_Write(DEV_RST_PIN, 1);
}

/******************************************************************************
function:   send command
parameter: 
        Cmd: command
Info:
******************************************************************************/
static void ADS1263_WriteCmd(UBYTE Cmd, UWORD DEV_CS_PIN)
{
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_SPI_WriteByte(Cmd);
    DEV_Digital_Write(DEV_CS_PIN, 1);
}

/******************************************************************************
function:   Write a data to the destination register
parameter: 
        Reg : Target register
        data: Written data
Info:
******************************************************************************/
static void ADS1263_WriteReg(UBYTE Reg, UBYTE data, UWORD DEV_CS_PIN)
{
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_SPI_WriteByte(CMD_WREG | Reg);
    DEV_SPI_WriteByte(0x00);
    DEV_SPI_WriteByte(data);
    DEV_Digital_Write(DEV_CS_PIN, 1);
}

/******************************************************************************
function:   Read a data from the destination register
parameter: 
        Reg : Target register
Info:
    Return the read data
******************************************************************************/
static UBYTE ADS1263_Read_data(UBYTE Reg, UWORD DEV_CS_PIN)
{
    UBYTE temp = 0;
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_SPI_WriteByte(CMD_RREG | Reg);
    DEV_SPI_WriteByte(0x00);
    temp = DEV_SPI_ReadByte();
    DEV_Digital_Write(DEV_CS_PIN, 1);
    return temp;
}

/******************************************************************************
function:   Check data
parameter: 
        val : 4 bytes(ADC2 is 3 bytes) data
        byt : CRC byte
Info:
        Check success, return 0
******************************************************************************/
static UBYTE ADS1263_Checksum(UDOUBLE val, UBYTE byt)
{
    UBYTE sum = 0;
    UBYTE mask = -1;        // 8 bits mask, 0xff
    while(val) {
        sum += val & mask;  // only add the lower values
        val >>= 8;          // shift down
    }
    sum += 0x9b;
    // printf("--- %x %x--- \r\n", sum, byt);
    //printf("CRC return: %u\n", sum ^ byt);
    return sum ^ byt;       // if equal, this will be 0
}

/******************************************************************************
function:   Waiting for a busy end
parameter: 
Info:
    Timeout indicates that the operation is not working properly.
******************************************************************************/
static void ADS1263_WaitDRDY(UWORD DEV_DRDY_PIN)
{   
    UDOUBLE i = 0;
    while (DEV_Digital_Read(DEV_DRDY_PIN) == 1 && i < 4000000) { i++; }
    
    if (i > 4000000) {
        printf("TIMED OUT!");
    }

    /*
    // printf("ADS1263_WaitDRDY \r\n");
    UDOUBLE i = 0;
    while(1) {
        if(DEV_Digital_Read(DEV_DRDY_PIN) == 0)
            break;
        if(i >= 4000000) {
            printf("Time Out ...\r\n"); 
            break;
        }
        i++;
    }   
    // printf("ADS1263_WaitDRDY Release \r\n");
    */
}

/******************************************************************************
function:  Read device ID
parameter: 
Info:
******************************************************************************/
UBYTE ADS1263_ReadChipID(UWORD DEV_CS_PIN)
{
    UBYTE id;
    id = ADS1263_Read_data(0, DEV_CS_PIN);
    printf("ID: %u\n", id);
    return id>>5;
}

/******************************************************************************
function:  Setting mode
parameter: 
    Mode : 0 Single-ended input
           1 channel1 Differential input
Info:
******************************************************************************/
void ADS1263_SetMode(UBYTE Mode)
{
    if(Mode == 0) {
        ScanMode = 0;
    }else {
        ScanMode = 1;
    }
}

/******************************************************************************
function:  Configure ADC gain and sampling speed
parameter: 
    gain : Enumeration type gain
    drate: Enumeration type sampling speed
Info:
******************************************************************************/
void ADS1263_ConfigADC1(ADS1263_GAIN gain, ADS1263_DRATE drate, ADS1263_DELAY delay, UWORD DEV_CS_PIN)
{
    UBYTE MODE2 = 0x8A;             //0x80:PGA bypassed, 0x00:PGA enabled
    //MODE2 |= (gain << 4) drate;
    ADS1263_WriteReg(REG_MODE2, MODE2, DEV_CS_PIN);
    DEV_Delay_ms(5);
    if(ADS1263_Read_data(REG_MODE2, DEV_CS_PIN) == MODE2)
        printf("REG_MODE2 success \r\n");
    else
        printf("REG_MODE2 unsuccess \r\n");
    
    UBYTE REFMUX = 0x24;        //0x00:+-2.5V as REF, 0x24:VDD,VSS as REF
    ADS1263_WriteReg(REG_REFMUX, REFMUX, DEV_CS_PIN);
    DEV_Delay_ms(5);
    if(ADS1263_Read_data(REG_REFMUX, DEV_CS_PIN) == REFMUX)
        printf("REG_REFMUX success \r\n");
    else
        printf("REG_REFMUX unsuccess \r\n");
    
    UBYTE MODE0 = delay;
    ADS1263_WriteReg(REG_MODE0, MODE0, DEV_CS_PIN); 
    DEV_Delay_ms(5);
    if(ADS1263_Read_data(REG_MODE0, DEV_CS_PIN) == MODE0)
        printf("REG_MODE0 success \r\n");
    else
        printf("REG_MODE0 unsuccess \r\n");
    
    UBYTE MODE1 = 0x00 | drate; // Digital Filter; 0x84:FIR, 0x64:Sinc4, 0x44:Sinc3, 0x24:Sinc2, 0x04:Sinc1
    ADS1263_WriteReg(REG_MODE1, MODE1, DEV_CS_PIN); 
    DEV_Delay_ms(5);
    if(ADS1263_Read_data(REG_MODE1, DEV_CS_PIN) == MODE1)
        printf("REG_MODE1 success \r\n");
    else
        printf("REG_MODE1 unsuccess \r\n");
}

/******************************************************************************
function:  Device initialization
parameter: 
Info:
******************************************************************************/
UBYTE ADS1263_init_ADC1(ADS1263_DRATE rate, UWORD DEV_CS_PIN)
{
    if(ADS1263_ReadChipID(DEV_CS_PIN) == 1) {
        printf("ID Read success \r\n");
    }
    else {
        printf("ID Read failed \r\n");
    }
    
    ADS1263_WriteCmd(CMD_STOP1, DEV_CS_PIN);
    ADS1263_ConfigADC1(ADS1263_GAIN_1, rate, ADS1263_DELAY_35us, DEV_CS_PIN);
    return 0;
}

/******************************************************************************
function:  Set the channel to be read
parameter: 
    Channal : Set channel number
Info:
******************************************************************************/
static void ADS1263_SetChannal(UBYTE Channal, UWORD DEV_CS_PIN)
{
    if(Channal > 10) {
        return ;
    }
    UBYTE INPMUX = (Channal << 4) | 0x0a;       //0x0a:VCOM as Negative Input
    //printf("CHANNEL: %u\n", Channal);
    ADS1263_WriteReg(REG_INPMUX, INPMUX, DEV_CS_PIN);
    if(ADS1263_Read_data(REG_INPMUX, DEV_CS_PIN) == INPMUX) {
        //printf("ADS1263_ADC1_SetChannal success \r\n");
    } else {
        printf("ADS1263_ADC1_SetChannal unsuccess \r\n");
    }
}

/******************************************************************************
function:  Read ADC data
parameter: 
Info:
******************************************************************************/
static UDOUBLE ADS1263_Read_ADC1_Data(UWORD DEV_CS_PIN, UWORD DEV_DRDY_PIN)
{
    UDOUBLE read = 0;
    UBYTE buf[4] = {0, 0, 0, 0};
    UBYTE Status, CRC;
    int retry_count = 0;

    DEV_Digital_Write(DEV_CS_PIN, 0);

    Status = DEV_SPI_ReadByte();      // 1 byte: status
    buf[0] = DEV_SPI_ReadByte();      // 4 bytes: ADC result
    buf[1] = DEV_SPI_ReadByte();
    buf[2] = DEV_SPI_ReadByte();
    buf[3] = DEV_SPI_ReadByte();
    CRC    = DEV_SPI_ReadByte();      // 1 byte: CRC

    DEV_Digital_Write(DEV_CS_PIN, 1);

    read |= ((UDOUBLE)buf[0] << 24);
    read |= ((UDOUBLE)buf[1] << 16);
    read |= ((UDOUBLE)buf[2] << 8);
    read |=  (UDOUBLE)buf[3];

     if (ADS1263_Checksum(read, CRC) != 0) {
        printf("⚠️  CRC error on ADC read. Retrying...\n");

        retry_count++;
        if (retry_count < 50) {
            ADS1263_WaitDRDY(DEV_DRDY_PIN);      // Wait again before retry
        } else {
            printf("❌ CRC failed 3 times — returning 0\n");
            return 0xFFFFFFFF;       // Special value to indicate failure
        }
    }
    
    return read;
    /*
    UDOUBLE read = 0;
    UBYTE buf[4] = {0, 0, 0, 0};
    UBYTE Status, CRC;
    DEV_Digital_Write(DEV_CS_PIN, 0);
    do {
        DEV_SPI_WriteByte(CMD_RDATA1);
        //DEV_delay_ms(10);
        Status = DEV_SPI_ReadByte();
    }while((Status & 0x40) == 0);
    
    buf[0] = DEV_SPI_ReadByte();
    buf[1] = DEV_SPI_ReadByte();
    buf[2] = DEV_SPI_ReadByte();
    buf[3] = DEV_SPI_ReadByte();
    CRC = DEV_SPI_ReadByte();
    DEV_Digital_Write(DEV_CS_PIN, 1);
    read |= ((UDOUBLE)buf[0] << 24);
    read |= ((UDOUBLE)buf[1] << 16);
    read |= ((UDOUBLE)buf[2] << 8);
    read |= (UDOUBLE)buf[3];
    // printf("%x %x %x %x %x %x\r\n", Status, buf[0], buf[1], buf[2], buf[3], CRC);
    if(ADS1263_Checksum(read, CRC) != 0)
        printf("ADC1 Data read error! \r\n");
    return read;
    */
}

/******************************************************************************
 * Read ADC specified channel data
 *
 * Parameters:
 *   Channel     : Channel number to read (0–10)
 *   DEV_CS_PIN  : GPIO pin used for SPI chip select (CS). Set LOW to select the ADC chip during SPI communication, HIGH to deselect.
 *   DEV_DRDY_PIN: GPIO pin used for Data Ready (DRDY) signal. Goes LOW when new ADC data is available; polled to ensure conversion is complete before reading.
 *
 * Returns:
 *   UDOUBLE     : Raw ADC value from the specified channel
 *
 * Info:
 *   - Uses SPI to communicate with the ADC chip.
 *   - Waits for DRDY signal before reading data to ensure conversion is complete.
 ******************************************************************************/
UDOUBLE ADS1263_GetChannalValue(UBYTE Channel, UWORD DEV_CS_PIN, UWORD DEV_DRDY_PIN)
{
    ADS1263_WriteCmd(CMD_STOP1, DEV_CS_PIN);
    UDOUBLE Value = 0;
    if(ScanMode == 0) {// 0  Single-ended input  10 channel1 Differential input  5 channe 
        if(Channel>10) {
            return 0;
        }
        ADS1263_SetChannal(Channel, DEV_CS_PIN);
        ADS1263_WriteCmd(CMD_START1, DEV_CS_PIN);
        ADS1263_WaitDRDY(DEV_DRDY_PIN);
        Value = ADS1263_Read_ADC1_Data(DEV_CS_PIN, DEV_DRDY_PIN);
    }
    return Value;

}

/******************************************************************************
function:  Read data from all channels
parameter: 
    ADC_Value : ADC Value
Info:
******************************************************************************/
void ADS1263_GetAll(UBYTE *List, UDOUBLE *Value, int Number, UWORD DEV_CS_PIN, UWORD DEV_DRDY_PIN)
{
    
    clock_t start_time, end_time;
    double cpu_time_used;
    UWORD REF = 3.27;
    
    start_time = clock();
    for(int i = 0; i<Number; i++) {
        Value[i] = ADS1263_GetChannalValue(List[i], DEV_CS_PIN, DEV_DRDY_PIN);
        //printf("VALUE: %u\n", Value[i]);
        /*
        if ((Value[i] >> 31) == 1){
            Value[i] = 3.27 * 2 - Value[i]/2147483648.0 * 3.27;
        } else {
            
            Value[i] = Value[i]/2147483647.8 * REF;
        }
        */
    }
    end_time = clock();
        
    cpu_time_used = ((double) (end_time-start_time)) / CLOCKS_PER_SEC;
    //printf("Execution time: %f seconds\n", cpu_time_used);
}
