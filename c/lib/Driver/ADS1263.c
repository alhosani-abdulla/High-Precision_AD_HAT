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
#include "ADS1263.h"
#include <time.h>

/******************************************************************************
Global Variables
Info:
    ScanMode: 0 = Single-ended input (10 channels), 1 = Differential input (5 channels)
    For Highz: Always use mode 0 (single-ended) to maximize channel count
******************************************************************************/
UBYTE ScanMode = 0;


/******************************************************************************
function:   Get DRDY pin for a given CS pin
parameter:
    DEV_CS_PIN: Chip select pin number
Info:
    Pin Mapping for 3-ADC Stack:
    CS Pin 12 -> DRDY Pin 16 (ADC #1 - Top)
    CS Pin 22 -> DRDY Pin 17 (ADC #2 - Middle)
    CS Pin 23 -> DRDY Pin 25 (ADC #3 - Bottom)
    
    This mapping is specific to the Highz hardware modification where
    each stacked ADC has unique CS and DRDY pins for individual addressing.
******************************************************************************/
int get_DRDYPIN(UWORD DEV_CS_PIN){
    UWORD drdyPin = 0;
    if (DEV_CS_PIN == 12){
        drdyPin = 16;        // ADC #1 (Top)
    }
    if (DEV_CS_PIN == 22){
        drdyPin = 17;        // ADC #2 (Middle)
    }
    if (DEV_CS_PIN == 23){
        drdyPin = 25;        // ADC #3 (Bottom)
    }
    return drdyPin;
}

/******************************************************************************
function:   Read calibration/device state from digital input signals
parameter:
Info:
    Hardware Configuration:
    - 3 device state signals are read into ADC #1 (top ADC)
    - These are 2-bit encoded signals giving 8 possible states (0-7)
    - Plus one special state (9) when all bits including bit4 are high
    
    Pin Mapping:
    - bit1: GPIO 21  \
    - bit2: GPIO 20   } 3x 2-bit state signals
    - bit3: GPIO 16  /
    - bit4: GPIO 12  - Special state indicator
    
    State Encoding:
    bit4=0: States 0-7 based on bit1, bit2, bit3 combinations
    bit4=1: State 9 if all bits are high, Error if any other combination
    
    Returns: String buffer containing state number ('0'-'9'), or NULL on error
    Note: Caller must free() the returned buffer
******************************************************************************/
char *READ_CALIBRATION_STATE(void)
{
    char *buf = malloc(64);
    if (!buf) return NULL;
    
    // Read the 4 state bits from GPIO pins
    int bit1 = DEV_Digital_Read(21);
    int bit2 = DEV_Digital_Read(20);
    int bit3 = DEV_Digital_Read(16);
    int bit4 = DEV_Digital_Read(12);
    
    // Decode the state based on bit combinations
    // bit4 indicates special state or normal operation
    if (bit4) {
        // Special state: All bits high = State 9
        if (bit1 && bit2 && bit3){
            buf[0] = '9';
            buf[1] = '\0';
            return buf;
        } else {
            // Error: bit4 high but other bits don't match expected pattern
            printf("Error: invalid state.\n");
        }
    } else {
        // Normal states (0-7): Decode based on bit1, bit2, bit3
        // Truth table for 3-bit encoding:
        // bit1 | bit2 | bit3 | State
        // -----|------|------|------
        //   0  |   0  |   0  |   0
        //   0  |   0  |   1  |   4
        //   0  |   1  |   0  |   2
        //   0  |   1  |   1  |   6
        //   1  |   0  |   0  |   1
        //   1  |   0  |   1  |   5
        //   1  |   1  |   0  |   3
        //   1  |   1  |   1  |   7
        
        if (bit1) {
            if (bit2 && bit3) {
                buf[0] = '7';    // 111 -> State 7
            } else if (bit3) {
                buf[0] = '5';    // 101 -> State 5
            } else if (bit2) {
                buf[0] = '3';    // 110 -> State 3
            } else {
                buf[0] = '1';    // 100 -> State 1
            }
        } else if (bit2) {
            if (bit3) {
                buf[0] = '6';    // 011 -> State 6
            } else {
                buf[0] = '2';    // 010 -> State 2
            }
        } else if (bit3) {
            buf[0] = '4';        // 001 -> State 4
        } else {
            buf[0] = '0';        // 000 -> State 0
        }
    }
    
    buf[1] = '\0';
    return buf;
}

/******************************************************************************
function:   Hardware reset of a specific ADC chip
parameter:
    DEV_RST_PIN: Reset pin for the target ADC
Info:
    Reset Sequence:
    1. Set reset pin HIGH
    2. Pulse LOW (resets the chip)
    3. Return HIGH (normal operation)
    
    This performs a hardware reset. All ADC registers return to defaults.
******************************************************************************/
void ADS1263_reset(UWORD DEV_RST_PIN)
{
    DEV_Digital_Write(DEV_RST_PIN, 1);
    DEV_Digital_Write(DEV_RST_PIN, 0);    // Reset pulse
    DEV_Digital_Write(DEV_RST_PIN, 1);
}

/******************************************************************************
function:   Send command to ADC via SPI
parameter: 
    Cmd: Command byte (see ADS1263_CMD enum)
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    SPI Transaction:
    1. Assert CS (LOW) to select ADC
    2. Write command byte
    3. De-assert CS (HIGH) to complete transaction
******************************************************************************/
static void ADS1263_WriteCmd(UBYTE Cmd, UWORD DEV_CS_PIN)
{
    DEV_Digital_Write(DEV_CS_PIN, 0);    // Select ADC
    DEV_SPI_WriteByte(Cmd);
    DEV_Digital_Write(DEV_CS_PIN, 1);
}

/******************************************************************************
function:   Write a data to the destination register
parameter: 
    Reg : Target register address (see ADS1263_REG enum)
    data: Data byte to write
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    SPI Write Register Protocol:
    1. Assert CS
    2. Send WREG command OR'd with register address
    3. Send 0x00 (number of registers to write - 1)
    4. Send data byte
    5. De-assert CS
******************************************************************************/
static void ADS1263_WriteReg(UBYTE Reg, UBYTE data, UWORD DEV_CS_PIN)
{
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_SPI_WriteByte(CMD_WREG | Reg);
    DEV_SPI_WriteByte(0x00);              // Writing 1 register (n-1 = 0)
    DEV_SPI_WriteByte(data);
    DEV_Digital_Write(DEV_CS_PIN, 1);
}

/******************************************************************************
function:   Read a data from the destination register
parameter: 
    Reg : Target register address (see ADS1263_REG enum)
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    Return the read data
    
    SPI Read Register Protocol:
    1. Assert CS
    2. Send RREG command OR'd with register address
    3. Send 0x00 (number of registers to read - 1)
    4. Read data byte
    5. De-assert CS
******************************************************************************/
static UBYTE ADS1263_Read_data(UBYTE Reg, UWORD DEV_CS_PIN)
{
    UBYTE temp = 0;
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_SPI_WriteByte(CMD_RREG | Reg);
    DEV_SPI_WriteByte(0x00);              // Reading 1 register (n-1 = 0)
    temp = DEV_SPI_ReadByte();
    DEV_Digital_Write(DEV_CS_PIN, 1);
    return temp;
}

/******************************************************************************
function:   Check data CRC checksum
parameter: 
    val : 32-bit ADC data value
    byt : CRC byte received from ADC
Info:
    Check success, return 0
    
    CRC Algorithm:
    1. Sum all bytes of the ADC value
    2. Add constant 0x9b
    3. XOR with received CRC byte
    4. Result should be 0 for valid data
    
    ADS1263 uses this checksum to detect SPI communication errors
******************************************************************************/
static UBYTE ADS1263_Checksum(UDOUBLE val, UBYTE byt)
{
    UBYTE sum = 0;
    UBYTE mask = -1;        // 8-bit mask (0xFF)
    
    // Sum each byte of the 32-bit value
    while(val) {
        sum += val & mask;  // Add lowest byte
        val >>= 8;          // Shift to next byte
    }
    sum += 0x9b;            // Add ADS1263 checksum constant
    
    return sum ^ byt;       // if equal, this will be 0
}

/******************************************************************************
function:   Waiting for a busy end
parameter: 
    DEV_DRDY_PIN: Data Ready pin for the ADC being polled
Info:
    Timeout indicates that the operation is not working properly.
    
    DRDY Signal Behavior:
    - Goes LOW when new ADC data is available
    - Remains HIGH during conversion
    
    This function polls the DRDY pin until it goes LOW, indicating
    that a conversion is complete and data is ready to be read.
    
    Timeout: 4,000,000 iterations maximum
    Prevents infinite loop if ADC fails or is misconfigured
******************************************************************************/
static void ADS1263_WaitDRDY(UWORD DEV_DRDY_PIN)
{   
    UDOUBLE i = 0;
    
    // Poll DRDY pin until LOW (data ready) or timeout
    while (DEV_Digital_Read(DEV_DRDY_PIN) == 1 && i < 4000000) { 
        i++; 
    }
    
    if (i >= 4000000) {
        printf("TIMED OUT! DRDY never went LOW for pin %d\n", DEV_DRDY_PIN);
    }
}

/******************************************************************************
function:  Read device ID
parameter: 
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    ID Register Format:
    - Bits [7:5]: Device ID (001 for ADS1263)
    - Bits [4:0]: Revision ID
    
    This function reads register 0 (REG_ID) and extracts the device ID
    by right-shifting 5 bits.
    
    Expected ID: 1 (binary 001) for ADS1263
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
    Mode : 0 Single-ended input (10 channels)
           1 Differential input (5 channels)
Info:
    Mode 0 (Single-ended): All 10 inputs referenced to VCOM
    Used in Highz to maximize channel count (7 detectors + 3 state signals)
    
    Mode 1 (Differential): 5 differential pairs
    Not used in Highz configuration
    
    This sets the global ScanMode variable used by read functions
******************************************************************************/
void ADS1263_SetMode(UBYTE Mode)
{
    if(Mode == 0) {
        ScanMode = 0;    // Single-ended (Highz default)
    }else {
        ScanMode = 1;    // Differential (not used)
    }
}

/******************************************************************************
function:  Configure ADC1 operating parameters
parameter: 
    gain : PGA gain setting (not used - PGA bypassed in Highz config)
    drate: Data rate (sampling speed) - see ADS1263_DRATE enum
    delay: Conversion delay - see ADS1263_DELAY enum
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    Register Configuration:
    
    MODE2 (0x8A):
    - Bit 7=1: PGA bypassed (no gain amplification)
    - For Highz: Direct voltage measurement, no gain needed
    
    REFMUX (0x24):
    - 0x00: Internal ±2.5V reference
    - 0x24: VDD/VSS as reference (used in Highz)
    - Allows full-scale measurement up to supply voltage
    
    MODE0:
    - Sets conversion delay (settling time)
    - 35µs default for Highz (balances speed vs. accuracy)
    
    MODE1:
    - Sets digital filter and data rate
    - Filter options: Sinc1, Sinc2, Sinc3, Sinc4, FIR
    - 0x00 = Sinc1 filter (fastest, used in Highz)
    
    Each register write is verified by reading back
******************************************************************************/
void ADS1263_ConfigADC1(ADS1263_GAIN gain, ADS1263_DRATE drate, ADS1263_DELAY delay, UWORD DEV_CS_PIN)
{
    // MODE2: PGA Configuration
    UBYTE MODE2 = 0x8A;    // 0x80=PGA bypassed, 0x00=PGA enabled
                           // Highz uses bypassed mode for direct voltage reading
    ADS1263_WriteReg(REG_MODE2, MODE2, DEV_CS_PIN);
    DEV_Delay_ms(5);    // Set to 5 ms to ensure write completes instead of 1 ms
    if(ADS1263_Read_data(REG_MODE2, DEV_CS_PIN) == MODE2)
        printf("REG_MODE2 success \r\n");
    else
        printf("REG_MODE2 unsuccess \r\n");
    
    // REFMUX: Reference Voltage Selection
    UBYTE REFMUX = 0x24;   // 0x00=Internal ±2.5V, 0x24=VDD/VSS
                           // Highz uses VDD/VSS for full-scale measurement
    ADS1263_WriteReg(REG_REFMUX, REFMUX, DEV_CS_PIN);
    DEV_Delay_ms(5);    // Set to 5 milliseconds to ensure write completes instead of 1 ms
    if(ADS1263_Read_data(REG_REFMUX, DEV_CS_PIN) == REFMUX)
        printf("REG_REFMUX success \r\n");
    else
        printf("REG_REFMUX unsuccess \r\n");
    
    // MODE0: Conversion Delay (settling time)
    UBYTE MODE0 = delay;
    ADS1263_WriteReg(REG_MODE0, MODE0, DEV_CS_PIN); 
    DEV_Delay_ms(5);
    if(ADS1263_Read_data(REG_MODE0, DEV_CS_PIN) == MODE0)
        printf("REG_MODE0 success \r\n");
    else
        printf("REG_MODE0 unsuccess \r\n");
    
    // MODE1: Digital Filter and Data Rate
    UBYTE MODE1 = 0x00 | drate;  // Filter: 0x84=FIR, 0x64=Sinc4, 0x44=Sinc3, 
                                  //         0x24=Sinc2, 0x04=Sinc1, 0x00=Sinc1
                                  // Highz uses 0x00 (Sinc1) for fastest response
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
    rate : Sampling rate (data rate) - see ADS1263_DRATE enum
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    Initialization Sequence:
    1. Read and verify chip ID (should be 1 for ADS1263)
    2. Stop any ongoing conversions
    3. Configure ADC parameters:
       - Gain: 1x (PGA bypassed)
       - Data rate: User-specified
       - Delay: 35µs (settling time)
       - Reference: VDD/VSS
       - Filter: Sinc1
    
    Highz Configuration:
    - GAIN_1: No amplification (direct voltage measurement)
    - DELAY_35us: Fast settling for rapid channel switching
    - VDD/VSS reference: Full-scale to supply voltage
    
    Must be called once for each ADC before reading data
******************************************************************************/
UBYTE ADS1263_init_ADC1(ADS1263_DRATE rate, UWORD DEV_CS_PIN)
{
    // Verify chip ID
    if(ADS1263_ReadChipID(DEV_CS_PIN) == 1) {
        printf("ID Read success \r\n");
    }
    else {
        printf("ID Read failed \r\n");
    }
    
    // Stop any running conversions
    ADS1263_WriteCmd(CMD_STOP1, DEV_CS_PIN);
    
    // Configure ADC with Highz-specific parameters
    ADS1263_ConfigADC1(ADS1263_GAIN_1, rate, ADS1263_DELAY_35us, DEV_CS_PIN);
    
    return 0;
}

/******************************************************************************
function:  Set the channel to be read
parameter: 
    Channal : Channel number to select (0-10)
    DEV_CS_PIN: Chip select pin for target ADC
Info:
    INPMUX Register Format:
    - Bits [7:4]: Positive input selection (AINP)
    - Bits [3:0]: Negative input selection (AINN)
    
    Configuration:
    - AINP = Channel number (0-10 for AIN0-AIN10)
    - AINN = 0x0A (VCOM - common reference)
    
    Single-Ended Mode:
    All channels measured relative to VCOM (common voltage reference).
    This allows 10 independent single-ended measurements.
    
    Register write is verified by reading back
******************************************************************************/
static void ADS1263_SetChannal(UBYTE Channal, UWORD DEV_CS_PIN)
{
    if(Channal > 10) {
        return;    // Invalid channel number
    }
    
    // INPMUX: [AINP (4-bit) | AINN (4-bit)]
    // AINP = Channel, AINN = 0x0A (VCOM)
    UBYTE INPMUX = (Channal << 4) | 0x0a;
    
    ADS1263_WriteReg(REG_INPMUX, INPMUX, DEV_CS_PIN);
    
    // Verify register write
    if(ADS1263_Read_data(REG_INPMUX, DEV_CS_PIN) == INPMUX) {
        // Success (commented out to reduce console output)
        //printf("ADS1263_ADC1_SetChannal success \r\n");
    } else {
        printf("ADS1263_ADC1_SetChannal unsuccess \r\n");
    }
}

/******************************************************************************
function:  Read ADC data
parameter: 
    DEV_CS_PIN: Chip select pin for target ADC
    DEV_DRDY_PIN: Data ready pin for target ADC (used for retry)
Info:
    Data Format (6 bytes total):
    - Byte 0: Status register
    - Bytes 1-4: 32-bit ADC result (MSB first)
    - Byte 5: CRC checksum
    
    Reading Sequence:
    1. Assert CS to begin SPI transaction
    2. Read status byte (not currently used)
    3. Read 4 data bytes (32-bit result)
    4. Read CRC byte
    5. De-assert CS
    6. Verify CRC checksum
    
    Error Handling:
    - If CRC fails, can retry up to 50 times
    - Returns 0xFFFFFFFF if all retries fail
    - Indicates possible SPI communication issue
    
    Must wait for DRDY LOW before calling this function
******************************************************************************/
static UDOUBLE ADS1263_Read_ADC1_Data(UWORD DEV_CS_PIN, UWORD DEV_DRDY_PIN)
{
    UDOUBLE read = 0;
    UBYTE buf[4] = {0, 0, 0, 0};
    UBYTE Status, CRC;
    int retry_count = 0;

    // Begin SPI transaction
    DEV_Digital_Write(DEV_CS_PIN, 0);

    // Read 6-byte data packet
    Status = DEV_SPI_ReadByte();      // Byte 0: Status register
    buf[0] = DEV_SPI_ReadByte();      // Byte 1: ADC data MSB
    buf[1] = DEV_SPI_ReadByte();      // Byte 2: ADC data
    buf[2] = DEV_SPI_ReadByte();      // Byte 3: ADC data
    buf[3] = DEV_SPI_ReadByte();      // Byte 4: ADC data LSB
    CRC    = DEV_SPI_ReadByte();      // Byte 5: CRC checksum

    // End SPI transaction
    DEV_Digital_Write(DEV_CS_PIN, 1);

    // Assemble 32-bit value from 4 bytes (big-endian)
    read |= ((UDOUBLE)buf[0] << 24);
    read |= ((UDOUBLE)buf[1] << 16);
    read |= ((UDOUBLE)buf[2] << 8);
    read |=  (UDOUBLE)buf[3];

    // Verify data integrity with CRC
    if (ADS1263_Checksum(read, CRC) != 0) {
        printf("⚠️  CRC error on ADC read. Retrying...\n");

        retry_count++;
        if (retry_count < 50) {
            ADS1263_WaitDRDY(DEV_DRDY_PIN);      // Wait for next conversion
            // Could recursively retry here, but current code just reports error
        } else {
            printf("❌ CRC failed %d times — returning error value\n", retry_count);
            return 0xFFFFFFFF;       // Error indicator
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
function:  Read ADC specified channel data
parameter: 
    Channel : Channel number to read (0-10)
    DEV_CS_PIN : GPIO pin used for SPI chip select (CS)
    DEV_DRDY_PIN: GPIO pin used for Data Ready (DRDY) signal
Info:
    Returns raw ADC value from the specified channel
    
    - Uses SPI to communicate with the ADC chip
    - CS pin set LOW to select the ADC chip during SPI communication, HIGH to deselect
    - DRDY goes LOW when new ADC data is available
    - Waits for DRDY signal before reading data to ensure conversion is complete
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
    List : Array of channel numbers to read
    Value : Array to store ADC readings (must be pre-allocated)
    Number : Number of channels to read
    DEV_CS_PIN : Chip select pin for target ADC
    DEV_DRDY_PIN: Data ready pin for target ADC
Info:
    Reads multiple channels sequentially from the specified ADC
    Used for reading log detectors on each ADC in Highz spectrometer
    
    Optional voltage conversion code is commented out but available
    Execution time measurement code is commented out
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
