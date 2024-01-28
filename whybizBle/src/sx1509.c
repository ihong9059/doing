#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h> 
#include <zephyr/drivers/i2c.h>

#include "uttec.h"
#include "myBle.h"
#include "sx1509.h"

#define SX1509_ADDRESS		0x3e
#define I2C_NODE DT_NODELABEL(arduino_i2c)

static const struct device *i2c0_dev = DEVICE_DT_GET(I2C_NODE);
// static uint8_t sx1509_in = 0;

void initSxRelay(void){
	for(int i = 0; i < 8; i++){
		writeOutSx(i, 0);
		k_sleep(K_MSEC(100));
	}
}

void initSx1509(void){
	int err;
	uint8_t buf[2];

	if(!device_is_ready(i2c0_dev)){
		printk("i2c_dev is not ready ----------->\r\n");
		while(1);
	}

	buf[0] = 0x00; buf[1] = 0x00; //port b, input enable	
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	buf[0] = 0x01; buf[1] = 0x00; //port a, input enable
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);

	buf[0] = 0x06; buf[1] = 0xff; //port b, pull up
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	buf[0] = 0x07; buf[1] = 0xff; //port a, pull up
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	buf[0] = 0x0e; buf[1] = 0x00; //port b, output
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	buf[0] = 0x0f; buf[1] = 0xff; //port a, input
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");		// return 0;
		while(1);
	}
	// initSxRelay();
	writeInitRelay();
	delay(100);
	
	printk("relay status: %x, sw status: %x\r\n", 
		readSxRelay(), readSxSw());

	// printk("------> end of initSx1509\r\n");
}

uint8_t readSxRelay(void){
	int err;
	uint8_t buf = 0x10;
	whybiz_t* pWhybiz = getWhybizFactor();

	err = i2c_write(i2c0_dev, &buf, 1, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	err = i2c_read(i2c0_dev, &buf, 1, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	pWhybiz->relay = buf;
	return pWhybiz->relay;
}

uint8_t readSxSw(void){
	int err;
	uint8_t buf = 0x11;
	whybiz_t* pWhybiz = getWhybizFactor();

	err = i2c_write(i2c0_dev, &buf, 1, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	err = i2c_read(i2c0_dev, &buf, 1, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	pWhybiz->sw = buf;
	return pWhybiz->sw;
}

bool checkSx1509In(bool ble){
	static uint16_t testCount = 0;
	uint8_t swNow = readSxSw();
	sx1509_t* pBefore = getBeforeSxReg();

	if(pBefore->sw == swNow){
		printk("now and before same: %d\r\n", testCount++);
		return 0;	
	} 		
	printk("now: %x, reg: %x\r\n", swNow, pBefore->sw);
	whybizFrame_t* pJson = getWhybizFrame();
	pJson->node = 1;
	pJson->category = SWITCH_DEVICE;
	pJson->sensor = swNow;
	pJson->value = swNow;
	procSwitchTxBle(true);

	pBefore->sw = swNow;
	return true;
}

uint8_t readOutSxRelay(void){
	int err;
	uint8_t buf = 0x10;

	err = i2c_write(i2c0_dev, &buf, 1, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	err = i2c_read(i2c0_dev, &buf, 1, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	printk("read buf: %x\r\n", buf);
	return buf;
}

void writeInitRelay(void){
	whybiz_t* pWhybiz = getWhybizFactor();
	int err;
	
	uint8_t buf[2];
	buf[0] = 0x10; buf[1] = pWhybiz->relay; //regB, all high
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
}

void writeOutSx(uint8_t bitNum, bool output){
	uint8_t regB = readOutSxRelay();
	int err;
	printk("~BIT_0: %x\r\n", (uint8_t)~BIT_0);

	switch(bitNum){
		case 0:
			if(output) regB = regB | (uint8_t)~BIT_0;
			else regB = regB & BIT_0;
		break;
		case 1:
			if(output) regB = regB | (uint8_t)~BIT_1;
			else regB = regB & BIT_1;
		break;
		case 2:
			if(output) regB = regB | (uint8_t)~BIT_2;
			else regB = regB & BIT_2;
		break;
		case 3:
			if(output) regB = regB | (uint8_t)~BIT_3;
			else regB = regB & BIT_3;
		break;
		case 4:
			if(output) regB = regB | (uint8_t)~BIT_4;
			else regB = regB & BIT_4;
		break;
		case 5:
			if(output) regB = regB | (uint8_t)~BIT_5;
			else regB = regB & BIT_5;
		break;
		case 6:
			if(output) regB = regB | (uint8_t)~BIT_6;
			else regB = regB & BIT_6;
		break;
		case 7:
			if(output) regB = regB | (uint8_t)~BIT_7;
			else regB = regB & BIT_7;
		break;
	}
	uint8_t buf[2];
	buf[0] = 0x10; buf[1] = regB; //regB, all high
	err = i2c_write(i2c0_dev, buf, 2, SX1509_ADDRESS);
	if(err < 0){
		printk("i2c write fail------------\r\n");
	}
	printk("regB: %x\r\n", regB);
}

// void setRelay()
void testRelay(void){
	static uint16_t count = 0;
	switch(count%16){
		case 0: writeOutSx(count%8, true); break;
		case 1: writeOutSx(count%8, true); break;
		case 2: writeOutSx(count%8, true); break;
		case 3: writeOutSx(count%8, true); break;
		case 4: writeOutSx(count%8, true); break;
		case 5: writeOutSx(count%8, true); break;
		case 6: writeOutSx(count%8, true); break;
		case 7: writeOutSx(count%8, true); break;
		case 8: writeOutSx(count%8, false); break;
		case 9: writeOutSx(count%8, false); break;
		case 10: writeOutSx(count%8, false); break;
		case 11: writeOutSx(count%8, false); break;
		case 12: writeOutSx(count%8, false); break;
		case 13: writeOutSx(count%8, false); break;
		case 14: writeOutSx(count%8, false); break;
		case 15: writeOutSx(count%8, false); break;
	}
	count++;
}

void testSw(void){
}
