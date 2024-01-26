#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/data/json.h>

#include "uttec.h"
#include <zephyr/logging/log.h>
#include "adc.h"
#include "myBle.h"
#include "nvm.h"

whybiz_t myWhybiz = {0, };

sx1509_t beforeSx1509 = {0, };

struct whybiz{
    uint8_t no;
    uint8_t ca;
    uint8_t se;
    uint8_t va;
    uint8_t crc;
};

static const struct json_obj_descr whybiz_descr[] = {
  JSON_OBJ_DESCR_PRIM(struct whybiz, no, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM(struct whybiz, ca, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM(struct whybiz, se, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM(struct whybiz, va, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM(struct whybiz, crc, JSON_TOK_NUMBER),
};

#define SELECT1_NODE DT_NODELABEL(select1)
#define SELECT2_NODE DT_NODELABEL(select2)
#define LORARST_NODE DT_NODELABEL(lorarst)
#define PWR_CRL_NODE DT_NODELABEL(pwrctl)
#define RS485TX_NODE DT_NODELABEL(rs485tx)


static const struct gpio_dt_spec select1 = GPIO_DT_SPEC_GET(SELECT1_NODE, gpios);
static const struct gpio_dt_spec select2 = GPIO_DT_SPEC_GET(SELECT2_NODE, gpios);
static const struct gpio_dt_spec loraRst = GPIO_DT_SPEC_GET(LORARST_NODE, gpios);
static const struct gpio_dt_spec pwrCtl = GPIO_DT_SPEC_GET(PWR_CRL_NODE, gpios);
static const struct gpio_dt_spec rs485_en = GPIO_DT_SPEC_GET(RS485TX_NODE, gpios);

whybiz_t* getWhybizFactor(void){
    return &myWhybiz;    
}

void initPort(void){
	whybiz_t* pWhybiz = getWhybizFactor();
	gpio_pin_configure_dt(&select1, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&select2, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&loraRst, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&pwrCtl, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&rs485_en, GPIO_OUTPUT_ACTIVE);

    gpio_pin_set_dt(&select1, 1);//set low
    gpio_pin_set_dt(&select2, 1);//set low
    gpio_pin_set_dt(&loraRst, 0);//set high
    gpio_pin_set_dt(&pwrCtl, 1);//set low
    gpio_pin_set_dt(&rs485_en, 1);//set low

    setUartChannel(pWhybiz->channel);
    // setUartChannel(1); // for server test. 2023.12.12. 15:50
}

void setUartChannel(uint8_t channel){
    switch(channel){
        case 0: 
            gpio_pin_set_dt(&select2, 1); gpio_pin_set_dt(&select1, 1); 
            printk("Ethernet channel\r\n");
        break;
        case 1:
            gpio_pin_set_dt(&select2, 1); gpio_pin_set_dt(&select1, 0); 
            printk("rs485 channel\r\n");
        break;
        case 2:
            gpio_pin_set_dt(&select2, 0); gpio_pin_set_dt(&select1, 1); 
            printk("lora channel\r\n");
        break;
        case 3:
            gpio_pin_set_dt(&select2, 0); gpio_pin_set_dt(&select1, 0); 
            printk("tbd channel\r\n");
        break;
        default: printk("error channel: %d\r\n", channel);
    }
}

sx1509_t* getBeforeSxReg(void){
    return &beforeSx1509;
}

void sendAck(ack_t ack){
    whybiz_t* pFactor = getWhybizFactor();

    if(pFactor->channel == LORA_CHANNEL) printf("at=u");
    if(pFactor->channel == RS485_CHANNEL){
        gpio_pin_set_dt(&rs485_en, 0);//set high, for tx
        printf("%d, %d, %d, %d", ack.ca, ack.se, ack.va, ack.crc);
        delay(1);
        delay(5);

        gpio_pin_set_dt(&rs485_en, 1);//set low, for rx
    }else{
        printf("%d, %d, %d, %d", ack.ca, ack.se, ack.va, ack.crc);
    }
}

void newParse(uint8_t* pData, uint8_t len){
    printk("newParse\r\n");
    
    struct whybiz ctr;

    ctr.no = pData[1];
    ctr.ca = pData[2];
    ctr.se = pData[3];
    ctr.va = pData[4];
    ctr.crc = pData[5];
    printk("no: %d, ca: %d, se: %d, va: %d, crc: %d\r\n",
    ctr.no, ctr.ca, ctr.se, ctr.va, ctr.crc);

    whybiz_t* pFactor = getWhybizFactor();
    ack_t ack;
    ack.ca = 3; ack.se = ctr.se; ack.va= ctr.va; ack.crc = ctr.crc;

    switch(ctr.ca){
        case CTR_RELAY:
            printk("no:%d, ca:%d, se:%d, va:%d, crc:%d\r\n",
            ctr.no, ctr.ca, ctr.se, ctr.va, ctr.crc);
            writeOutSx(ctr.se, ctr.va);
            procRelayTxBle(false);
            ack.ca = 3; ack.se = pFactor->relay; pFactor->relay; ack.crc = ctr.crc;
            sendAck(ack);

            // printf("at=i");
        break;
        case CTR_LORA:
        case SET_LORA:
            pFactor->power = ctr.se;
            printk("set lora power-> se: %d, va: %d\r\n", 
                ctr.se, ctr.va);
            printk("save factor ###########\r\n");
            // saveFactorToFlash();
        break;
        case CTR_CHANNEL:
        case SET_CHANNEL:
            pFactor->channel = ctr.se; pFactor->lora_ch = ctr.va;
            printk("set channel-> uart: %d, lora: %d\r\n", 
                ctr.se, ctr.va);
            printk("save factor ###########\r\n");
            // setUartChannel(pFactor->channel);
            // saveFactorToFlash();
        break;
        case SET_VERSION:
            pFactor->version = ctr.se; pFactor->ble = ctr.va;
            pFactor->node = ctr.va;
            printk("set number-> node: %d, ble: %d\r\n", 
                ctr.se, ctr.va);
            printk("save factor ###########\r\n");
            // saveFactorToFlash();
        break;
        case STATUS_RSSI:
            pFactor->rssi = ctr.se;
            printk("-----> rssi: %d\r\n", ctr.se);
        break;
        default: printk("error category: %d\r\n", ctr.ca); break;
    }
}

#include "sx1509.h"
void uttecJsonTest(uint8_t* pData, uint8_t len){
}

whybizFrame_t myWhybizFrame = {0, };

void sendWhybizFrame(void){
    whybizFrame_t* pJson = getWhybizFrame();
    pJson->crc = pJson->node + pJson->category + pJson->sensor + pJson->sensor;
    printk("{\"no\":%d,\"ca\":%d,\"se\":%d,\"va\":%d,\"crc\":%d}\r\n",
    pJson->node, pJson->category, pJson->sensor, pJson->value, pJson->crc);
}

whybizFrame_t* getWhybizFrame(void){
    return &myWhybizFrame;
}

void testJsonOut(void){
}


void sendFactorAtConnection(void){
    whybiz_t* pFactor = getWhybizFactor();
    printk("------> sendFactorAtConnection\r\n");
    for(int i = 0; i < MAX_CATEGORY; i++){
        switch(i){
            case CHANNEL_DEVICE:
                procChannel();
                printk("channel: %d, lora ch: %d\r\n", 
                    pFactor->channel, pFactor->lora_ch);
            break;
            case ADC_DEVICE:
                procAdcTxBle();
                printk("sw: %d\r\n", pFactor->sw);
            break;
            case SWITCH_DEVICE: 
                procSwitchTxBle(true);
                printk("relay: %d\r\n", pFactor->relay);
            break;
            case RELAY_DEVICE: 
                procRelayTxBle(true);
                printk("adc1: %d, adc2: %d\r\n", pFactor->adc1, pFactor->adc2);
            break;
            case LORA_DEVICE: 
                procLora();
                printk("power: %d, rssi: %d\r\n", 
                    pFactor->power, pFactor->rssi);
            break;
            case VERSION_DEVICE:
                procVersion();
                printk("version: %d, ble num: %d\r\n", 
                    pFactor->version, pFactor->ble);
            break;
        }
        k_sleep(K_MSEC(20));
    }
}

void sendStatus2Server(void){
    static uint32_t count = 0;
    whybiz_t* pFactor = getWhybizFactor();
    whybizFrame_t* pFrame = getWhybizFrame();
    connectFlag_t* pFlags = getConnectFlag();

    if(!(count++ % 2)){
        static uint32_t sendCount = 0;
        // uint8_t who = (sendCount++ % MAX_CATEGORY);
        uint8_t who = (sendCount++ % 2) + SWITCH_DEVICE;
        pFrame->node = pFactor->node;
        switch(who){
        // switch(RELAY_DEVICE){
            case ADC_DEVICE: 
                // printk("ADC_DEVICE: %d\r\n", who); 
                // pFrame->category = ADC_DEVICE; 
                // pFrame->sensor = pFactor->adc1; pFrame->value = pFactor->adc2; 
                // procAdcTxBle();
            break;
            case SWITCH_DEVICE: printk("SWITCH_DEVICE: %d\r\n", who); 
                pFrame->category = SWITCH_DEVICE; 
                pFrame->sensor = pFactor->sw; pFrame->value = pFactor->sw; 
                procSwitchTxBle(pFlags->ble);
            break;
            case RELAY_DEVICE: printk("RELAY_DEVICE: %d\r\n", who); 
                pFrame->category = RELAY_DEVICE; 
                pFrame->sensor = pFactor->relay; pFrame->value = pFactor->relay; 
                procRelayTxBle(pFlags->ble);
            break;
            case LORA_DEVICE: printk("LORA_DEVICE: %d\r\n", who); 
                // pFrame->category = LORA_DEVICE; 
                // pFrame->sensor = pFactor->power; pFrame->value = pFactor->rssi; 
                // procLora();
            break;
            case VERSION_DEVICE: printk("VERSION_DEVICE: %d\r\n", who); 
                // pFrame->category = VERSION_DEVICE; 
                // pFrame->sensor = pFactor->version; pFrame->value = pFactor->ble; 
                // procVersion();
            break;
            case CHANNEL_DEVICE: printk("CHANNEL_DEVICE: %d\r\n", who); 
                // pFrame->category = CHANNEL_DEVICE; 
                // pFrame->sensor = pFactor->channel; pFrame->value = pFactor->lora_ch; 
                // procChannel();
            break;
        }
    ack_t ack = {pFrame->category, pFrame->sensor, pFrame->sensor, 200};

    sendAck(ack);
    }
}

void sendStatus2Ble(void){
    static uint32_t count = 0;
    whybiz_t* pFactor = getWhybizFactor();
    whybizFrame_t* pFrame = getWhybizFrame();

    connectFlag_t* pFlags = getConnectFlag();

    if(!(count++ % 2)){
        static uint32_t sendCount = 0;
        uint8_t who = (sendCount++ % MAX_CATEGORY);
        pFrame->node = pFactor->node;
        // switch(who){
        switch(RELAY_DEVICE){
            case ADC_DEVICE: 
                // printk("ADC_DEVICE: %d\r\n", who); 
                // pFrame->category = ADC_DEVICE; 
                // pFrame->sensor = pFactor->adc1; pFrame->value = pFactor->adc2; 
                // procAdcTxBle();
            break;
            case SWITCH_DEVICE: printk("SWITCH_DEVICE: %d\r\n", who); 
                pFrame->category = SWITCH_DEVICE; 
                pFrame->sensor = pFactor->sw; pFrame->value = pFactor->sw; 
                procSwitchTxBle(true);
            break;
            case RELAY_DEVICE: printk("RELAY_DEVICE: %d\r\n", who); 
                pFrame->category = RELAY_DEVICE; 
                pFrame->sensor = pFactor->relay; pFrame->value = pFactor->relay; 
                procRelayTxBle(true);
            break;
            case LORA_DEVICE: printk("LORA_DEVICE: %d\r\n", who); 
                // pFrame->category = LORA_DEVICE; 
                // pFrame->sensor = pFactor->power; pFrame->value = pFactor->rssi; 
                // procLora();
            break;
            case VERSION_DEVICE: printk("VERSION_DEVICE: %d\r\n", who); 
                // pFrame->category = VERSION_DEVICE; 
                // pFrame->sensor = pFactor->version; pFrame->value = pFactor->ble; 
                // procVersion();
            break;
            case CHANNEL_DEVICE: printk("CHANNEL_DEVICE: %d\r\n", who); 
                // pFrame->category = CHANNEL_DEVICE; 
                // pFrame->sensor = pFactor->channel; pFrame->value = pFactor->lora_ch; 
                // procChannel();
            break;
        }
    ack_t ack = {pFrame->category, pFrame->sensor, pFrame->sensor, 200};

    sendAck(ack);
    }
}

void sendJsonForStatus(void){
    // static uint32_t count = 0;
    // whybiz_t* pFactor = getWhybizFactor();
    // whybizFrame_t* pFrame = getWhybizFrame();

    // connectFlag_t* pFlags = getConnectFlag();

    // if(!(count++ % 2)){
    //     static uint32_t sendCount = 0;
    //     uint8_t who = (sendCount++ % MAX_CATEGORY);
    //     pFrame->node = pFactor->node;
    //     // printk("who: %d\r\n", who);
    //     switch(who){
    //         case ADC_DEVICE: 
    //             printk("ADC_DEVICE: %d\r\n", who); 
    //             pFrame->category = ADC_DEVICE; 
    //             pFrame->sensor = pFactor->adc1; pFrame->value = pFactor->adc2; 
    //             procAdcTxBle();
    //         break;
    //         case SWITCH_DEVICE: printk("SWITCH_DEVICE: %d\r\n", who); 
    //             pFrame->category = SWITCH_DEVICE; 
    //             pFrame->sensor = pFactor->sw; pFrame->value = pFactor->sw; 
    //             procSwitchTxBle();
    //         break;
    //         case RELAY_DEVICE: printk("RELAY_DEVICE: %d\r\n", who); 
    //             pFrame->category = RELAY_DEVICE; 
    //             pFrame->sensor = pFactor->relay; pFrame->value = pFactor->relay; 
    //             procRelayTxBle();
    //         break;
    //         case LORA_DEVICE: printk("LORA_DEVICE: %d\r\n", who); 
    //             pFrame->category = LORA_DEVICE; 
    //             pFrame->sensor = pFactor->power; pFrame->value = pFactor->rssi; 
    //             procLora();
    //         break;
    //         case VERSION_DEVICE: printk("VERSION_DEVICE: %d\r\n", who); 
    //             pFrame->category = VERSION_DEVICE; 
    //             pFrame->sensor = pFactor->version; pFrame->value = pFactor->ble; 
    //             procVersion();
    //         break;
    //         case CHANNEL_DEVICE: printk("CHANNEL_DEVICE: %d\r\n", who); 
    //             pFrame->category = CHANNEL_DEVICE; 
    //             pFrame->sensor = pFactor->channel; pFrame->value = pFactor->lora_ch; 
    //             procChannel();
    //         break;
    //     }
    //     pFrame->crc = pFrame->node + pFrame->category + pFrame->sensor + pFrame->value;  
        
    //     if(pFactor->channel == LORA_CHANNEL) printf("at=u");
    //     if(pFactor->channel == RS485_CHANNEL){
    //         gpio_pin_set_dt(&rs485_en, 0);//set high, for tx
    //         printf("{\"no\":%d,\"ca\":%d,\"se\":%d,\"va\":%d,\"crc\":%d}\r\n",
    //         pFrame->node, pFrame->category, pFrame->sensor, pFrame->value, pFrame->crc);
    //         delay(1);
    //         delay(5);

    //         gpio_pin_set_dt(&rs485_en, 1);//set low, for rx
    //     }else{
    //         printf("{\"no\":%d,\"ca\":%d,\"se\":%d,\"va\":%d,\"crc\":%d}\r\n",
    //         pFrame->node, pFrame->category, pFrame->sensor, pFrame->value, pFrame->crc);
    //     }
    // }
}

void dispChannel(void){
    whybiz_t* pFactor = getWhybizFactor();
    switch(pFactor->channel){
        case 0: printk("uart: Ethernet--> %d\r\n", pFactor->channel); break;
        case 1: printk("uart: rs485--> %d\r\n", pFactor->channel); break;
        case 2: printk("uart: lora--> %d\r\n", pFactor->channel); break;
        case 3: printk("uart: T.B.D--> %d\r\n", pFactor->channel); break;
    }
}

void delay(uint16_t msec){
    k_sleep(K_MSEC(msec));
}


