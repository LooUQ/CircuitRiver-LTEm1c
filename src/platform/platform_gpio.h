/******************************************************************************
 *  \file platform_gpio.h
 *  \author Jensen Miller, Greg Terrell
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
 *****************************************************************************/
#ifndef __PLATFORM_GPIO_H__
#define __PLATFORM_GPIO_H__

#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#else
#include <stdint.h>
#endif // __cplusplus


typedef enum {
    gpioValue_low = 0,
    gpioValue_high = 1
} gpio_pinValue_t;


typedef enum {
    gpioMode_input = 0x0,
    gpioMode_output = 0x1,
    gpioMode_inputPullUp,
    gpioMode_inputPullDown
} gpio_pinMode_t;


typedef enum {
    gpioIrqTriggerOn_low = 0,
    gpioIrqTriggerOn_high = 1,
    gpioIrqTriggerOn_change =2,
    gpioIrqTriggerOn_falling = 3,
    gpioIrqTriggerOn_rising = 4
} gpio_irqTrigger_t;


typedef uint8_t platformGpioPin;
// typedef struct platformGpioPin_tag* platformGpioPin;
// typedef struct platformGpioPin_tag { uint8_t pinNum; };
typedef void(*platformGpioPinIrqCallback)(void);



void gpio_openPin(uint8_t pinNum, gpio_pinMode_t pinMode);
void gpio_closePin(uint8_t pinNum);

gpio_pinValue_t gpio_readPin(uint8_t pinNum);
void gpio_writePin(uint8_t pinNum, gpio_pinValue_t val);

void gpio_attachIsr(uint8_t pinNum, bool enabled, gpio_irqTrigger_t triggerOn, platformGpioPinIrqCallback isrCallback);
void gpio_detachIsr(uint8_t pinNum);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__PLATFORM_GPIO_H__ */