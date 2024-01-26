#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include "nvm.h"

static struct nvs_fs fs;

#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define UTTEC_ID 2 

// static uttec_t myUttec = {0, };
#include "uttec.h"

void dispFactors(void){
	whybiz_t* pWhybiz = getWhybizFactor();
	printk("============= whybiz factors ==========\r\n");
	printk("version: %d\r\n", pWhybiz->version);
	printk("node: %d\r\n", pWhybiz->node);
	printk("uart: %d\r\n", pWhybiz->channel);
	printk("ble: %d\r\n", pWhybiz->ble);
	printk("adc1: %d\r\n", pWhybiz->adc1);
	printk("adc2: %d\r\n", pWhybiz->adc2);
	printk("sw: %d\r\n", pWhybiz->sw);
	printk("relay: %d\r\n", pWhybiz->relay);
	printk("lora_ch: %d\r\n", pWhybiz->lora_ch);
	printk("power: %d\r\n", pWhybiz->power);
	printk("rssi: %d\r\n", pWhybiz->rssi);
	printk("============= whybiz factors ==========\r\n");
	k_sleep(K_MSEC(1000));
}

void initNvm(void){
    int rc = 0;
	struct flash_pages_info info;
	whybiz_t* pWhybiz = getWhybizFactor();

	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		printk("Flash device %s is not ready\n", fs.flash_device->name);
	}
	fs.offset = NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
	}
	fs.sector_size = info.size;
	fs.sector_count = 3U;

	rc = nvs_mount(&fs);
	if (rc) {
		printk("Flash Init failed\n");
	}                    

	rc = nvs_read(&fs, UTTEC_ID, pWhybiz, sizeof(whybiz_t));

	if(pWhybiz->flashFlag == FLASH_FLAG){
		printk("Already written to flash factors\r\n");
		printk("version: %d\r\n", pWhybiz->version);			
	}
	else{
		printk("New Program. now set first factors\r\n");
		pWhybiz->flashFlag = FLASH_FLAG;
		pWhybiz->version = 1;
		pWhybiz->node = 111;
		pWhybiz->channel = 1;//0: ethernet, 1: rs485, 2: lora
		pWhybiz->ble = 123;
		pWhybiz->adc1 = 0;
		pWhybiz->adc2 = 0;
		pWhybiz->sw = 0;
		pWhybiz->relay = 0xff;
		pWhybiz->lora_ch = 20;
		pWhybiz->power = 16;
		pWhybiz->rssi = 0;
		(void)nvs_write(&fs, UTTEC_ID, pWhybiz, sizeof(whybiz_t)+1);
	}
	pWhybiz->channel = 1;//0: ethernet, 1: rs485, 2: lora , for test. 2024.01.26
	dispFactors();
}

void testNvm(void){
    int rc = 0;
	whybiz_t* pWhybiz = getWhybizFactor();

	rc = nvs_read(&fs, UTTEC_ID, pWhybiz, sizeof(whybiz_t));
	if (rc > 0) { /* item was found, show it */
		printk("version: %d, node: %d\n", pWhybiz->version, pWhybiz->node);
		pWhybiz->version++;
		(void)nvs_write(&fs, UTTEC_ID, pWhybiz, sizeof(whybiz_t)+1);
	} else   {/* item was not found, add it */
		printk("flash write read error\r\n");
	}
}

void saveFactorToFlash(void){
	whybiz_t* pWhybiz = getWhybizFactor();
	(void)nvs_write(&fs, UTTEC_ID, pWhybiz, sizeof(whybiz_t)+1);
}
