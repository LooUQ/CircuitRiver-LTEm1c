/******************************************************************************
 *  \file LTEmC-8-mqtt.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *  www.loouq.com
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
 * Test MQTT protocol client send/receive. 
 * Uses Azure IoTHub and the LooUQ Cloud as server side
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltemc.h>

#include "lqc_collections.h"            // borrow LQCloud collections for this test

#define DEFAULT_NETWORK_CONTEXT 1
#define XFRBUFFER_SZ 201
#define SOCKET_ALREADYOPEN 563

#define ASSERT(expected_true, failMsg)  if(!(expected_true))  appNotifyCB(255, failMsg)
#define ASSERT_NOTEMPTY(string, failMsg)  if(string[0] == '\0') appNotifyCB(255, failMsg)


/* Test is designed for use with Azure IoTHub. If you do not have a Azure subscription you can get a free test account with a preloaded credit.
 * Optionally you can request a trial account for LooUQ's LQ Cloud, which uses Azure IoTHub for device data ingress.
 * 
 * LQ Cloud or Azure IoTHub needs 3 provisioning elements: 
 *   -- The hub address, if you are using LQ Cloud for testing the MQTT_IOTHUB address below is valid. Otherwise supply your MQTT access point (no protocol prefix)
 *   -- A deviceId and 
 *   -- A SAS token (a time-limited password). For deviceId we recommend the modem's IMEI
 * 
 * The Device ID is up to 40 characters long and we suggest using the modem's IMEI. An example of the SAS token is shown on the next line.  
 * 
 * "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=XbjrqvX4kQXOTefJIaw86jRhfkv1UMJwK%2FFDiWmfqFU%3D&se=1759244275"
 *
 * Your values will be different, update MQTT_IOTHUB_DEVICEID and MQTT_IOTHUB_SASTOKEN with your values. If you obtain the SAS Token from Azure IoT Explorer it will
 * be prefixed with additional information: Hostname, DeviceId and the SharedAccessSignature property keys, these must be removed if using MQTT directly.
 * 
 * HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=867198053158865;SharedAccessSignature=SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F867198053158865&sig=YlW6PuH4MA93cINEhgstihwQxWt4Zr9iY9Tnkl%2BuQK4%3D&se=1608052225
 * ^^^                          REMOVE, NOT PART OF THE SAS TOKEN                              ^^^              KEEP ALL FOLLOWING '='
*/

#define MQTT_IOTHUB "iothub-dev-pelogical.azure-devices.net"
#define MQTT_PORT 8883

/* put your AZ IoTHub info here, SAS Token is available by using the "Azure IoT explorer" (https://github.com/Azure/azure-iot-explorer/releases/tag/v0.10.16)
 * Note that SAS tokens have an experiry from 5 minutes to years. Yes... the one below is expired. To cancel a published SAS Token with an extended expirery
 * you will need to change the device key (primary or secondary) it was based on.
 */
#define MQTT_IOTHUB_DEVICEID "864508030074113"
#define MQTT_IOTHUB_USERID "iothub-dev-pelogical.azure-devices.net/" MQTT_IOTHUB_DEVICEID "/?api-version=2018-06-30"
#define MQTT_IOTHUB_SASTOKEN "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F864508030074113&sig=GUTFgHfEGU4hoVmJpTHJwWimH2ItifUSxd4pI83r%2Boo%3D&se=1624939645"

// before commit "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F864508030074113&sig=dSmmZUimMuMu%2BNQjKtqLkrrzaeucvmqNM8BHKJbtIw8%3D&se=1621939493"

#define MQTT_IOTHUB_D2C_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/"
#define MQTT_IOTHUB_C2D_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/devicebound/#"
#define MQTT_MSG_PROPERTIES "mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97"
#define MQTT_MSG_BODY_TEMPLATE "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:%0.2f"


// test setup
#define CYCLE_INTERVAL 5000
uint16_t loopCnt = 1;
uint32_t lastCycle;


// ltem1 variables
socketResult_t result;
socketId_t mqttConnectionId = 1;
char mqttTopic[200];
char mqttMessage[200];


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(DBGCOLOR_white, "\rLTEm1c test8-MQTT\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem_create(ltem_pinConfig, appNotifyCB);
    mqtt_create();
    ltem_start(pdpProtocol_mqtt);

    PRINTF(DBGCOLOR_none, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(30000);
    if (strlen(networkOp.operName) == 0)
        appNotifyCB(255, "Timout (30s) waiting for cellular network.");
    PRINTF(DBGCOLOR_info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    uint8_t cntxtCnt = ntwk_getActivePdpCntxtCnt();
    if (cntxtCnt == 0)
    {
        ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT);
    }

    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
    */

    ASSERT(mqtt_open(MQTT_IOTHUB, MQTT_PORT, sslVersion_tls12, mqttVersion_311) == RESULT_CODE_SUCCESS, "MQTT open failed.");
    ASSERT(mqtt_connect(MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_SASTOKEN, mqttSession_cleanStart) == RESULT_CODE_SUCCESS,"MQTT connect failed.");
    ASSERT(mqtt_subscribe(MQTT_IOTHUB_C2D_TOPIC, mqttQos_1, mqttReceiver) == RESULT_CODE_SUCCESS, "MQTT subscribe to IoTHub C2D messages failed.");

    lastCycle = lMillis();
}



void loop() 
{
    if (lTimerExpired(lastCycle, CYCLE_INTERVAL))
    {
        lastCycle = lMillis();

        double windspeed = random(0, 4999) * 0.01;

        snprintf(mqttTopic, 200, MQTT_MSG_BODY_TEMPLATE, loopCnt, windspeed);
        snprintf(mqttMessage, 200, "MQTT message for loop=%d", loopCnt);
        mqtt_publish(mqttTopic, mqttQos_1, mqttMessage);

        loopCnt++;
        PRINTF(DBGCOLOR_magenta, "\rFreeMem=%u  <<Loop=%d>>\r", getFreeMemory(), loopCnt);
    }

    /* NOTE: ltem1_doWork() pipeline requires up to 3 invokes for each data receive. DoWork has no side effects 
     * other than taking time and should be invoked liberally. */
    ltem_doWork();
}


void mqttReceiver(char *topic, char *topicProps, char *message)
{
    PRINTF(DBGCOLOR_info, "\r**MQTT--MSG** @tick=%d\r", lMillis());
    PRINTF(DBGCOLOR_cyan, "\rt(%d): %s", strlen(topic), topic);
    PRINTF(DBGCOLOR_cyan, "\rp(%d): %s", strlen(topicProps), topicProps);
    PRINTF(DBGCOLOR_cyan, "\rm(%d): %s", strlen(message), message);

    // use local copy of LQ Cloud query string processor
    keyValueDict_t mqttProps = lqc_createDictFromQueryString(topicProps);
    PRINTF(DBGCOLOR_info, "\rProps(%d)\r", mqttProps.count);
    for (size_t i = 0; i < mqttProps.count; i++)
    {
        PRINTF(DBGCOLOR_cyan, "%s=%s\r", mqttProps.keys[i], mqttProps.values[i]);
    }
    PRINTF(0, "\r");
}



/* test helpers
========================================================================================================================= */

void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
	PRINTF(DBGCOLOR_error, "\r\n** %s \r\n", notifMsg);
    PRINTF(DBGCOLOR_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    while (halt) {}
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

