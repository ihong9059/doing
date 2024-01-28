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
	initAdc();
	initNvm();
	initPort();

#ifdef I2C_ENABLE
	initSx1509();
#endif

	whybizConnect.first = true;
    // whybiz_t* pFactor = getWhybizFactor();

	for (;;) {
		static uint32_t count = 0;
		static bool toggle = false;
		delay(500);

#ifdef I2C_ENABLE
		if(!(count % 2)){
			// sendStatus2Server();
		}
		if(!(count % 3)){
			printk("main: %d\n", count);
			dispChannel();
		}

		if((count % 10) == 5){
		    whybiz_t* pFactor = getWhybizFactor();
			if(pFactor->channel == LORA_CHANNEL){
				printk("rssi read\n");
				printf("at=i");
			}
		}	
#endif
		toggle = !toggle;
		count++;
	}
}

#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

void parsing(void){
	printk("now start parsing task\r\n");
	uint8_t beforeSw = 0;
    whybiz_t* pFactor = getWhybizFactor();
    whybizFrame_t* pFrame = getWhybizFrame();
    connectFlag_t* pFlags = getConnectFlag();

	while(1){
		delay(200);
		jsonFrame_t* pJason = getJsonFrame();
		if(pJason->flag){
			newParse(pJason->frame, pJason->end+1);// json test
			dispJsonFrame();
			k_sleep(K_MSEC(20));
			clearJsonData();
		}

		uint8_t nowSw = readSxSw();
		if(beforeSw != nowSw){
			beforeSw = nowSw;
			printk("changed switch, send data to server------->\r\n");
			pFrame->category = SWITCH_DEVICE; 
			pFrame->sensor = pFactor->sw; pFrame->value = pFactor->sw; 
			ack_t ack = {pFrame->category, pFrame->sensor, pFrame->sensor, 200};
			sendAck(ack);
			procSwitchTxBle(pFlags->ble);
			delay(100);
			ack.ca = pFrame->category; ack.se = pFrame->sensor;
			ack.va = pFrame->sensor; ack.crc = 200;
			sendAck(ack);
			procSwitchTxBle(pFlags->ble);
		}
		// sendStatus2Server();
	}
}

K_THREAD_DEFINE(parsing_thread_id, STACKSIZE, parsing, NULL, NULL,
		NULL, PRIORITY, 0, 0);

void test(void){
	uint32_t testCount = 0;
    whybiz_t* pFactor = getWhybizFactor();
	while(true){
		if(pFactor->channel == RS485_CHANNEL)
			delay(20);
		else
			delay(500);
		sendStatus2Server();
		// printk("test: %d\n", testCount++);
		// printk("-");
	}
}

K_THREAD_DEFINE(test_id, STACKSIZE, test, NULL, NULL,
		NULL, PRIORITY, 0, 0);


