#include "uart_async_adapter.h"

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <stdio.h>
#include <zephyr/logging/log.h>

#include "uttec.h"
#include "myBle.h"
#include "sx1509.h"
#include "adc.h"
#include "nvm.h"

#define RUN_LED_BLINK_INTERVAL 500
#define RUN_STATUS_LED DK_LED1

connectFlag_t whybizConnect = {0, };
connectFlag_t* getConnectFlag(void){
	return &whybizConnect;
}

#define I2C_ENABLE 1

int main(void)
{
	printk("nordic for whybiz. 2024.01.26. 09:20\r\n");
	initBle();
#ifdef I2C_ENABLE
	initSx1509();
#endif
	initAdc();
	initNvm();
	initPort();
	whybizConnect.first = true;
    // whybiz_t* pFactor = getWhybizFactor();

	for (;;) {
		static uint32_t count = 0;
		static bool toggle = false;
		delay(500);

#ifdef I2C_ENABLE
	sendStatus2Server();
	// if(whybizConnect.ble){
	// 	sendStatus2Ble();
	// }else{
	// 	sendStatus2Server();
	// }
		// sendJsonForStatus();
#endif
		printk("main: %d\r\n", count);
		dispChannel();
		toggle = !toggle;
		count++;
	}
}

#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

void parsing(void){
	printk("now start parsing task\r\n");
	uint32_t parsingCount = 0;
	while(1){
		delay(100);
		printk(".");
		jsonFrame_t* pFrame = getJsonFrame();
		if(pFrame->flag){
			newParse(pFrame->frame, pFrame->end+1);// json test
			dispJsonFrame();
			k_sleep(K_MSEC(20));
			clearJsonData();
		}
		// sendStatus2Server();
	}
}

K_THREAD_DEFINE(parsing_thread_id, STACKSIZE, parsing, NULL, NULL,
		NULL, PRIORITY, 0, 0);




