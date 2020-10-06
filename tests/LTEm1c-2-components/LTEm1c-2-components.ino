/******************************************************************************
 *  \file LTEm1-test2-components.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 * The test2-ltem1Components.ino tests the LTEm1 NXP serial bridge chip and
 * BG96 module for basic serial operations. 
 *****************************************************************************/

#define HOST_FEATHER_UXPLOR
#include <platform_pins.h>
#include <ltem1c.h>

#define _DEBUG
//#define USE_SERIAL 0
#include "dbgprint.h"


// spiConfig_t ltem1_spiConfig = 
// {
//   dataRate : 2000000U,
//   dataMode : spiDataMode_0,
//   bitOrder : spiBitOrder_msbFirst,
//   csPin : ltem1_pinConfig.spiCsPin
// };


// test setup
const int APIN_RANDOMSEED = 0;

void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(0, "LTEm1c test2-components\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    // create ltem1 and start it, wait for it to ready itself
    ltem1_create(ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_base);
}


int loopCnt = 0;
uint8_t testPattern;

void loop() {
    testPattern = random(256);
    uint8_t txBuffer_reg;
    uint8_t rxBuffer_reg;

    txBuffer_reg = testPattern;
    // rxBuffer doesn't matter prior to read
    
    sc16is741a_writeReg(SC16IS741A_SPR_ADDR, txBuffer_reg);
    rxBuffer_reg = sc16is741a_readReg(SC16IS741A_SPR_ADDR);

    if (testPattern != rxBuffer_reg)
        indicateFailure("Scratchpad write/read failed (write/read register)."); 


    /* BG96 test pattern: get IMEI
    *
    *  AT+GSN
    *         
    *  <IMEI value (20 char)>
    *
    *  OK
    */

    uint8_t regValue = 0;
    char cmd[] = "AT+GSN\r\0";
    //char cmd[] = "AT+QPOWD\r\0";
    PRINTF(0, "Invoking cmd: %s \r\n", cmd);

    sendCommand(cmd);

    // wait for BG96 response in FIFO buffer
    char response[65] = {0};

    recvResponse(response);

    // test response v. expected 
    char* validResponse = "AT+GSN\r\r\n86450";
    uint8_t imeiPrefixTest = strncmp(validResponse, response, strlen(validResponse)); 

    PRINTF(0, "Expecting 32 chars response, got %d \r\n", strlen(response));
    PRINTF(0, "Got response: %s", response);  

    if (loopCnt < 3 && strlen(response) == 43)
        PRINTF(dbgColor_warn, "Received APP RDY from LTEm1.\r\n");
    else if (imeiPrefixTest != 0 || strlen(response) != 32)
        indicateFailure("Unexpected IMEI value returned on cmd test... failed."); 

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */

#define ASCII_CR 13U

void sendCommand(const char* cmd)
{
    //sc16is741a_write(cmd, strlen(cmd));

    size_t sendSz = strlen(cmd);
    for (size_t i = 0; i < sendSz; i++)
    {
        sc16is741a_writeReg(SC16IS741A_FIFO_ADDR, cmd[i]);
    }
    delay(300);                                        // max response time per-Quectel
}



void recvResponse(char *response)
{
    uint8_t lsrValue = 0;
    uint8_t recvSz = 0;

    while (!(lsrValue & NXP_LSR_DATA_IN_RECVR))
    {
        lsrValue = sc16is741a_readReg(SC16IS741A_LSR_ADDR);
    }
    recvSz = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    sc16is741a_read(response, recvSz);
}



/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *  \return bool If the response string ends in a valid OK sequence
 */
bool validOkResponse(const char *response)
{
    #define BUFF_SZ 64
    #define EXPECTED_TERMINATOR_STR "OK\r\n"
    #define EXPECTED_TERMINATOR_LEN 4

    const char * end = (const char *)memchr(response, '\0', BUFF_SZ);
    if (end == NULL)
        end = response + BUFF_SZ;

    return strncmp(EXPECTED_TERMINATOR_STR, end - EXPECTED_TERMINATOR_LEN, EXPECTED_TERMINATOR_LEN) == 0;
}


/* test helpers
========================================================================================================================= */

void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor_info, "Loop: %i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        delay(50);
    }

    PRINTF(dbgColor_magenta, "Free memory: %u \r\n", getFreeMemory());
    PRINTF(0, "Next test in (millis): %i\r\n\r\n", waitNext);
    delay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    PRINTF(dbgColor_error, "** Halting Execution \r\n");
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        delay(100);
    }
}


/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}
