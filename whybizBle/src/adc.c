#include "uart_async_adapter.h"

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>

#include <stdio.h>

#include <zephyr/logging/log.h>

#include "uttec.h"
#include "myBle.h"
#include "sx1509.h"

#include <zephyr/drivers/adc.h>

#define ADC DT_NODELABEL(adc)
static const struct device *adc_dev = DEVICE_DT_GET(ADC);

#define ADC_RESOLUTION 	10
#define ADC0_CHANNEL	0
#define ADC1_CHANNEL	2
#define ADC0_PORT		SAADC_CH_PSELN_PSELN_AnalogInput0
#define ADC1_PORT		SAADC_CH_PSELN_PSELN_AnalogInput2
#define ADC_REFERENCE	ADC_REF_INTERNAL
#define ADC_GAIN		ADC_GAIN_1_6 

struct adc_channel_cfg ch0_cfg = {
	.gain = ADC_GAIN,
	.reference = ADC_REFERENCE,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = ADC0_CHANNEL,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = ADC0_PORT
#endif 	
}; 
struct adc_channel_cfg ch1_cfg = {
	.gain = ADC_GAIN,
	.reference = ADC_REFERENCE,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = ADC1_CHANNEL,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = ADC1_PORT
#endif 	
}; 

int16_t sample_buffer[2];
struct adc_sequence sequence = {
	.channels = BIT(ADC0_CHANNEL) | BIT(ADC1_CHANNEL),
	// .channels = BIT(ADC0_CHANNEL),
	.buffer = sample_buffer,
	.buffer_size = sizeof(sample_buffer),
	.resolution = ADC_RESOLUTION
};

void initAdc(void){
	if(!device_is_ready(adc_dev)){
		printk("adc_dev is not ready.");
	}
	int err = adc_channel_setup(adc_dev, &ch0_cfg);
	err = adc_channel_setup(adc_dev, &ch1_cfg);
	
}

void testAdc(void){
    int err;
    err = adc_read(adc_dev, &sequence);
    if(err != 0){
        printk("ADC reading failed with error %d\r\n", err);
        return;
    }
    int32_t mv_value = sample_buffer[0];
    int32_t adc_vref = adc_ref_internal(adc_dev);
    adc_raw_to_millivolts(adc_vref, ADC_GAIN, ADC_RESOLUTION, &mv_value);
    printk("ADC-spanning: %d mV, %d\r\n", mv_value, sample_buffer[0]/10);

    int32_t mv_value1 = sample_buffer[1];
    adc_raw_to_millivolts(adc_vref, ADC_GAIN, ADC_RESOLUTION, &mv_value1);
    printk("ADC-spanning: %d mV, %d\r\n", mv_value1, sample_buffer[1]);    
}

void readAdcValue(void){
    int err;
    whybiz_t* pWhybiz = getWhybizFactor();
    err = adc_read(adc_dev, &sequence);
    if(err != 0){
        printk("ADC reading failed with error %d\r\n", err);
        return;
    }
    pWhybiz->adc1 = sample_buffer[0]/10;
    pWhybiz->adc2 = sample_buffer[1]/10;
    printk("Adc1: %d, Adc2: %d\r\n", 
        pWhybiz->adc1, pWhybiz->adc2);
}