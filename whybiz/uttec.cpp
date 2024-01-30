#include "uttec.h"
#include "EEPROM.h"
#include "myJson.h"
#include "sx1509Lib.h"

#define SIGNAL_LED 2     //On board LED

#define PIN_A0 34     // some analog input sensor
#define PIN_A1 35     // some analog input sensor

whybiz_t myWhybiz = {0, };
sx1509_t beforeSx1509 = {0, };

whybiz_t* getWhybizFactor(void){
    return &myWhybiz;    
}

void readAdc(void){
  myWhybiz.adc1 = analogRead(PIN_A0) / 40;
  myWhybiz.adc2 = analogRead(PIN_A1) / 40;
  // Serial.printf("Adc1: %d, Adc2: %d\r\n", 
  //   myWhybiz.adc1, myWhybiz.adc2);

}

void loop_uttec(void){
  static uint32_t count = 0;
  if(!(count++ % 10)) printf("loop_uttec: %d\r\n", count++);
  printf(".");
  sendJsonForStatus();
}
//test__0 
void initPort(void){
  pinMode(SEL1, OUTPUT);
  pinMode(SEL2, OUTPUT);
  pinMode(SX_RESET, OUTPUT);
  pinMode(RS485_EN, OUTPUT);
  pinMode(PWR_CTL, OUTPUT);
  pinMode(LORA_RST, OUTPUT);
  pinMode(SIGNAL_LED, OUTPUT);

  digitalWrite(LORA_RST, 0);
  delay(300);
  digitalWrite(LORA_RST, 1);
  setUartChannel(myWhybiz.channel); // basic channel
  // setUartChannel(LORA_CHANNEL); //for lora test. 2023.12.20
  digitalWrite(RS485_EN, 0); //for read 
}

void dispFactor(void){
    EEPROM.readBytes(0, &myWhybiz, sizeof(whybiz_t));
    Serial.printf("flag: %x\r\n", myWhybiz.flashFlag);
    Serial.printf("version: %d\r\n", myWhybiz.version);
    Serial.printf("node: %d\r\n", myWhybiz.node);
    Serial.printf("channel: %d\r\n", myWhybiz.channel);
    Serial.printf("ble: %d\r\n", myWhybiz.ble);
    Serial.printf("adc1: %d\r\n", myWhybiz.adc1);
    Serial.printf("adc2: %d\r\n", myWhybiz.adc2);
    Serial.printf("sw: %d\r\n", myWhybiz.sw);
    Serial.printf("relay: %d\r\n", myWhybiz.relay);
    Serial.printf("lora_ch: %d\r\n", myWhybiz.lora_ch);
    Serial.printf("power: %d\r\n", myWhybiz.power);
    Serial.printf("rssi: %d\r\n", myWhybiz.rssi);
    delay(2000);
}

void saveFactorToFlash(void){
  EEPROM.writeBytes(0, &myWhybiz, sizeof(whybiz_t));
  EEPROM.commit();
}

void initEeprom(void){
  EEPROM.begin(EEPROM_MAX);
  Serial.printf("EEPROM Size: %d\r\n", EEPROM.length());
  EEPROM.readBytes(0, &myWhybiz, sizeof(whybiz_t));
  
  if(myWhybiz.flashFlag != FLASH_FLAG){
    Serial.printf("New software version---> 2023.12.06\r\n");
    delay(1000);
    myWhybiz.flashFlag = FLASH_FLAG;
    myWhybiz.version = VERSION;
    myWhybiz.node = 0;
    myWhybiz.channel = 0; //uart channel
    myWhybiz.ble = 12;
    myWhybiz.adc1 = 0;
    myWhybiz.adc2 = 0;
    myWhybiz.sw = 0;
    myWhybiz.relay = 0;
    myWhybiz.lora_ch = 0; //lora channel
    myWhybiz.power = 0;
    myWhybiz.rssi = 0;
    saveFactorToFlash();
  }
  else{
    Serial.printf("Already factor Ok.\r\n");
    dispFactor();
  }
}

void initUttec(void){
  initEeprom();
  initPort();
  initSx1509();
}

void procCmdLine(uttecJson_t data){
  Serial.printf("procCmdLine in uttec.c\r\n");
}

void procAdc(uttecJson_t data){
  Serial.printf("procAdc in uttec.c\r\n");
}

void testPort(void){
  static bool toggle = false;

  // digitalWrite(SEL1, toggle);
  // digitalWrite(SEL2, toggle);
  // digitalWrite(SX_RESET, toggle);
  // digitalWrite(RS485_EN, toggle);
  // digitalWrite(PWR_CTL, toggle);
  digitalWrite(LORA_RST, toggle);
  toggle = !toggle;
}

void signal(void){
  static bool toggle = false;
  digitalWrite(SIGNAL_LED, toggle);
  toggle = !toggle;
}

void setUartChannel(uint8_t channel){
  whybiz_t* pFactor = getWhybizFactor();
  pFactor->channel = channel;
  switch(channel){
    case 0://Ethernet
      digitalWrite(SEL2, 0);  digitalWrite(SEL1, 0);
    break;
    case 1://rs485
      digitalWrite(SEL2, 0);  digitalWrite(SEL1, 1);
    break;
    case 2://lora
      digitalWrite(SEL2, 1);  digitalWrite(SEL1, 0);
    break;
    case 3://T.B.D
      digitalWrite(SEL2, 1);  digitalWrite(SEL1, 1);
    break;
    default:
    break;
  }
  Serial.printf("uart channel: %d\r\n", channel);
}

void dispUartChannel(void){
  whybiz_t* pFactor = getWhybizFactor();
  switch(pFactor->channel){
    case 0://Ethernet
      Serial.printf("Ethernet channel\r\n");
      digitalWrite(SEL2, 0);  digitalWrite(SEL1, 0);
    break;
    case 1://rs485
      Serial.printf("rs485 channel\r\n");
      digitalWrite(SEL2, 0);  digitalWrite(SEL1, 1);
    break;
    case 2://lora
      Serial.printf("lora channel\r\n");
      digitalWrite(SEL2, 1);  digitalWrite(SEL1, 0);
    break;
    case 3://T.B.D
      Serial.printf("T.B.D channel\r\n");
      digitalWrite(SEL2, 1);  digitalWrite(SEL1, 1);
    break;
    
  }
}

void testEeprom(void){
  EEPROM.readBytes(0, &myWhybiz, sizeof(whybiz_t));
  Serial.printf("read rssi: %d\r\n", myWhybiz.rssi);
  myWhybiz.rssi++;
  EEPROM.writeBytes(0, &myWhybiz, sizeof(whybiz_t));
  EEPROM.commit();
}

void sendAck(ack_t ack){
    whybiz_t* pFactor = getWhybizFactor();

    if(pFactor->channel == RS485_CHANNEL){ //for 4s485
        digitalWrite(RS485_EN, 1); //for write 
        // gpio_pin_set_dt(&rs485_en, 0);//set high, for tx
        delay(1);
        Serial2.printf("%d, %d, %d, %d", ack.ca, ack.se, ack.va, ack.crc);
        // printf("{%c, %c, %c, %c}", ack.ca, ack.se, ack.va, ack.crc);

        delay(5);
        digitalWrite(RS485_EN, 0); //for read 
        // gpio_pin_set_dt(&rs485_en, 1);//set low, for rx
        // printk("%d, %d, %d, %d", ack.ca, ack.se, ack.va, ack.crc);
    }else{
        if(pFactor->channel == LORA_CHANNEL){
            // printf("at=u%c%c%c%c\r\n", ack.ca, ack.se, ack.va, ack.crc);
            Serial2.printf("at=u%d, %d, %d, %d\r\n", ack.ca, ack.se, ack.va, ack.crc);
        }else if(pFactor->channel == ETHERNET_CHANNEL){ // for ethernet
            // printf("{%c, %c, %c, %c}", ack.ca, ack.se, ack.va, ack.crc);
            Serial2.printf("%d, %d, %d, %d", ack.ca, ack.se, ack.va, ack.crc);
        }
        else{
            Serial.printf("error ack==================>\r\n");
        }
    }
}

void sendJsonForStatus(void){
    static uint32_t count = 0;
    whybiz_t* pFactor = getWhybizFactor();
    whybizFrame_t* pFrame = getWhybizFrame();

    connectFlag_t* pFlags = getConnectFlag();

    if(!(count++ % 2)){
        static uint32_t sendCount = 0;
        uint8_t who = (sendCount++ % MAX_CATEGORY);
        pFrame->node = pFactor->node;
        // printf("who: %d\r\n", who);
        switch(who){
            case ADC_DEVICE: 
                // Serial.printf("ADC_DEVICE: %d\r\n", who); 
                pFrame->category = ADC_DEVICE; 
                pFrame->sensor = pFactor->adc1; pFrame->value = pFactor->adc2; 
            break;
            case SWITCH_DEVICE: //Serial.printf("SWITCH_DEVICE: %d\r\n", who); 
                pFrame->category = SWITCH_DEVICE; 
                pFrame->sensor = pFactor->sw; pFrame->value = pFactor->sw; 
            break;
            case RELAY_DEVICE: //Serial.printf("RELAY_DEVICE: %d\r\n", who); 
                pFrame->category = RELAY_DEVICE; 
                pFrame->sensor = pFactor->relay; pFrame->value = pFactor->relay; 
            break;
            case LORA_DEVICE: //Serial.printf("LORA_DEVICE: %d\r\n", who); 
                pFrame->category = LORA_DEVICE; 
                pFrame->sensor = pFactor->power; pFrame->value = pFactor->rssi; 
            break;
            case VERSION_DEVICE: //Serial.printf("VERSION_DEVICE: %d\r\n", who); 
                pFrame->category = VERSION_DEVICE; 
                pFrame->sensor = pFactor->version; pFrame->value = pFactor->ble; 
            break;
            case CHANNEL_DEVICE: //Serial.printf("CHANNEL_DEVICE: %d\r\n", who); 
                pFrame->category = CHANNEL_DEVICE; 
                pFrame->sensor = pFactor->channel; pFrame->value = pFactor->lora_ch; 
            break;
        }
        pFrame->crc = pFrame->node + pFrame->category + pFrame->value + pFrame->value;  
        
//for server through selected uart channel. printf. 2023.12.15
        dispUartChannel();
        ack_t ack = {pFrame->category, pFrame->sensor, pFrame->value, 200};
        sendAck(ack);
        // digitalWrite(RS485_EN, 1); //for write 
        // Serial.printf("sendJsonForStatus---->\r\n");
        // if(pFactor->channel == LORA_CHANNEL){
        //   Serial2.printf("at=u");
        // }
        // delay(1);
        // Serial2.printf("{\"no\":%d,\"ca\":%d,\"se\":%d,\"va\":%d,\"crc\":%d}\r\n",
        // pFrame->node, pFrame->category, pFrame->sensor, pFrame->value, pFrame->crc);
        // delay(4);
        // digitalWrite(RS485_EN, 0); //for read 
    }
}

connectFlag_t whybizConnect = {0, };
connectFlag_t* getConnectFlag(void){
	return &whybizConnect;
}

whybizFrame_t myWhybizFrame = {0, };

void sendWhybizFrame(void){
    whybizFrame_t* pJson = getWhybizFrame();
    pJson->crc = pJson->node + pJson->category + pJson->sensor + pJson->sensor;
    Serial.printf("{\"no\":%d,\"ca\":%d,\"se\":%d,\"va\":%d,\"crc\":%d}\r\n",
    pJson->node, pJson->category, pJson->sensor, pJson->value, pJson->crc);
}

whybizFrame_t* getWhybizFrame(void){
    return &myWhybizFrame;
}

#include <ArduinoJson.h>

#include "myJson.h"

void parseLoraInfo(uint8_t cmd, uint8_t value){
  static uint32_t testCount = 0;
  uint8_t count = 0;
  uint8_t flag0 = 0;
  uint8_t flag1 = 0;
  uttecJson_t ctr;
  char temp[MAX_DOC] = {0, };

  switch(cmd){
    case LORA_CHANNEL_INFO:
      // Serial.printf("LORA_CHANNEL_INFO\r\n");
      Serial.printf("at=7%-2d\r\n", value);
      Serial2.printf("at=7%-2d", value);
    break;
    case LORA_POWER_INFO:
      // Serial.printf("LORA_POWER_INFO\r\n");
      Serial.printf("at=p%-2d\r\n", value);
      Serial2.printf("at=p%-2d", value);
    break;
    case LORA_RSSI_INFO:
      // Serial.printf("LORA_RSSI_INFO\r\n");
      Serial.printf("at=m%-2d", value);
      Serial2.printf("AT=i");
    break;
    case LORA_TEST_UART:
      Serial.printf("-----------> LORA_TEST_UART\r\n");
      Serial.printf("at=u{\"ca\":%d,\"se\":%d,\"va\":%d}\r\n",
      testCount, testCount+1, testCount+2);
      Serial2.printf("at=u{\"ca\":%d,\"se\":%d,\"va\":%d}\r",
      testCount++, testCount+1, testCount+2);
      delay(500);
      // Serial.printf("-----------> after LORA_TEST_UART\r\n");
    break;
    case LORA_TEST_INFO:
      Serial.printf("-----------> LORA_TEST_INFO\r\n");
      // Serial2.printf("at=9");
    break;
    default:
      Serial.printf("Lora Error cmd: %d\r\n", cmd);
      return;
  }

  bool finish = true;
  while((Serial2.available() > 0)&&finish){
    char test = Serial2.read();
    Serial.printf("%c", test);
    if(test == '{'){
      flag0++;
      count = 0;
      temp[count++] = test;
    } 
    else if(test == '}'){
      temp[count++] = test;
      finish = false;
    }
    else{
      temp[count++] = test;
    } 

    if(count > MAX_DOC){
      Serial.printf("<------- MAX over ---------->\r\n");
      count = 0;
    } 
  }

  StaticJsonDocument<MAX_DOC> doc;
  DeserializationError error = deserializeJson(doc, temp);
  if (error) {
    // Serial.print(F("deserializeJson() failed: "));
    // Serial.println(error.f_str());
    Serial.printf("json error: %d ---------%s\r\n", count, temp);
    return;
  }
  ctr.ca = doc["ca"];
  ctr.se = doc["se"];
  ctr.va = doc["va"];
  Serial.printf("---> ca: %d, se: %d, va: %d\r\n", 
    ctr.ca, ctr.se, ctr.va);
}

