//	Copyright (c) 2020 LooUQ Incorporated.
//	Licensed under The MIT License. See LICENSE in the root directory.

#include "ltem1c.h"
// #include "components/nxp_sc16is741a.h"
// #include "components/quectel_bg96.h"
// #include "platform/platform_stdio.h"
// #include "platform/platform_gpio.h"
// #include "platform/platform_timing.h"
//#include "iop.h"


ltem1_pinConfig_t FEATHER_BREAKOUT =
{
	.spiCsPin = 13,
	.irqPin = 12,
	.statusPin = 06,
	.powerkeyPin = 11,
	.resetPin = 19,
	.ringUrcPin = 0,
    .wakePin = 0
};

ltem1_pinConfig_t RPI_BREAKOUT = 
{
	.spiCsPin = 0, 			//< J8_24
	.irqPin = 22U,		//< J8_15
	.statusPin = 13U,		//< J8_22
	.powerkeyPin = 24U,		//< J8_18
	.resetPin = 23U,		//< J8_16
	.ringUrcPin = 0,
    .wakePin = 0
};


ltem1_device_t *g_ltem1;


#pragma region private functions

/**
 *	\brief Initialize the modems IO.
 *
 *	\param[in] modem The LTE modem.
 */
static void initIO()
{
	// on Arduino, ensure pin is in default "logical" state prior to opening
	gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_low);
	gpio_writePin(g_ltem1->gpio->resetPin, gpioValue_low);
	gpio_writePin(g_ltem1->gpio->spiCsPin, gpioValue_high);

	gpio_openPin(g_ltem1->gpio->powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(g_ltem1->gpio->resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(g_ltem1->gpio->spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	gpio_openPin(g_ltem1->gpio->statusPin, gpioMode_input);
	gpio_openPin(g_ltem1->gpio->irqPin, gpioMode_inputPullUp);
}


#pragma endregion

#pragma region public functions


/**
 *	\brief Initialize the LTEm1 modem.
 *
 *	\param[in] ltem1_config The LTE modem gpio pin configuration.
 *  \param[in] funcLevel Determines the LTEm1 functionality to create and start.
 */
void ltem1_create(const ltem1_pinConfig_t* ltem1_config, ltem1_functionality_t funcLevel)
{
	g_ltem1 = calloc(1, sizeof(ltem1_device_t));
	if (g_ltem1 == NULL)
	{
        ltem1_faultHandler("ltem1-could not alloc ltem1 object");
	}

	g_ltem1->gpio = ltem1_config;
    g_ltem1->spi = spi_create(g_ltem1->gpio->spiCsPin);
    //g_ltem1->spi = createSpiConfig(g_ltem1->gpio->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT);

    g_ltem1->modemInfo = calloc(1, sizeof(ltem1_modemInfo_t));
	if (g_ltem1->modemInfo == NULL)
	{
        ltem1_faultHandler("ltem1-could not alloc ltem1 provisions object");
	}

    if (funcLevel >= ltem1_functionality_iop)
        g_ltem1->iop = iop_create();

    if (funcLevel >= ltem1_functionality_atcmd)
        g_ltem1->atcmd = atcmd_create(0);

    ltem1_start(funcLevel);
}



/**
 *	\brief Power on and start the modem (perform component init).
 */
void ltem1_start(ltem1_functionality_t funcLevel)
{
    initIO();   // make sure GPIO pins in correct state

	if (!gpio_readPin(g_ltem1->gpio->statusPin))
	{
        bg96_powerOn();
        g_ltem1->bg96ReadyState = bg96_readyState_powerOn;
        g_ltem1->funcLevel = funcLevel;
	}
	else
	{
		PRINTF_INFO("LTEm1 found powered on.\r\n");
        g_ltem1->bg96ReadyState = bg96_readyState_appReady;

        // power off/on if IRQ latched: previously fired and not serviced
        gpio_pinValue_t irqState = gpio_readPin(g_ltem1->gpio->irqPin);
        if (irqState == gpioValue_low)
        {
    		PRINTF_WARN("Invalid LTEm1 IRQ state, restarting.\r\n");
            bg96_powerOff();
            timing_delay(1000);
            bg96_powerOn();
        }

        /* future HW rev, reset bridge v. BG */
        // // reset IRQ if latched: previous fired and not serviced
        // gpio_pinValue_t irqState = gpio_readPin(g_ltem1->gpio->irqPin);
        // if (irqState == gpioValue_low)
        //     ltem1_reset(false);
	}

    // start NXP SPI-UART bridge
    spi_start(g_ltem1->spi);
    sc16is741a_start();

    if (funcLevel >= ltem1_functionality_iop)
        iop_start();

    // atcmd doesn't have a start/stop 

    if (funcLevel >= ltem1_functionality_full)
        bg96_start();
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
void ltem1_reset(bool restart)
{
	// gpio_writePin(g_ltem1->gpio->resetPin, gpioValue_low);
	// timing_delay(BG96_RESET_DELAY);
	// gpio_writePin(g_ltem1->gpio->resetPin, gpioValue_high);

    // if (restart)
    // {
    //     sc16is741a_start();
    //     if (g_ltem1->funcLevel == ltem1_functionality_iop)
    //     {
    //         iop_start();
    //     }
    //     bg96_start();
    // }
}



/**
 *	\brief Powers off the modem without destroying memory objects. Modem device will require ltem1_start() to reinit HW.
 */
void ltem1_stop()
{
    g_ltem1->bg96ReadyState = bg96_readyState_powerOff;
    bg96_powerOff();
}



/**
 *	\brief Uninitialize the LTE modem.
 *
 *	\param[in] modem The LTE modem.
 */
void ltem1_destroy()
{
	ltem1_stop();

	gpio_pinClose(g_ltem1->gpio->irqPin);
	gpio_pinClose(g_ltem1->gpio->powerkeyPin);
	gpio_pinClose(g_ltem1->gpio->resetPin);
	gpio_pinClose(g_ltem1->gpio->statusPin);

    atcmd_destroy(g_ltem1->atcmd);
    iop_destroy();
    spi_destroy(g_ltem1->spi);
	free(g_ltem1);
}



/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem1_dowork()
{
	// iop_classifyReadBuffers()
}


/**
 *	\brief Function of last resort, catestrophic failure Background work task runner. To be called in application Loop() periodically.
 */
void ltem1_faultHandler(const char * fault)
{
    PRINTF_ERR(fault);
    // NotImplemented if custom fault handler go there
    while(1) {};
}



/**
 *	\brief Registers the address (void*) of your application custom fault handler.
 */
void ltem1_registerApplicationFaultHandler(void * customFaultHandler)
{
}

#pragma endregion