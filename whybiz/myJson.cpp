
#include <ArduinoJson.h>
#include "uttec.h"
#include "myJson.h"

// void testSendJson(void){
//   StaticJsonDocument<MAX_DOC> doc;
//   uint8_t rand = random(1, 100);
//   doc["no"] = rand;
//   doc["ca"] = rand % 4;
//   doc["de"] = rand;
//   doc["va"] = rand;
//   doc["crc"] = rand * 3 + (rand % 4);

//   serializeJson(doc, Serial2);
// }

#include "sx1509Lib.h"
#include <stdio.h>

void parseWifiJson(String pData){
  uttecJson_t ctr;
  whybiz_t* pFactor = getWhybizFactor();

  StaticJsonDocument<MAX_DOC> doc;
  DeserializationError error = deserializeJson(doc, pData);
  if (error) {
    // Serial.print(F("deserializeJson() failed: "));
    // Serial.println(error.f_str());
    Serial.printf("json error ---------\r\n");
    return;
  }    
  ctr.ca = doc["ca"];
  ctr.se = doc["se"];
  ctr.va = doc["va"];
  bool ctrFlag = false;

  switch(ctr.ca){
    case CTR_RELAY:
      Serial.printf("setRelay\r\n");
      setRelay(ctr.se, ctr.va);
    break;
    case CTR_LORA:
    case SET_LORA:
      pFactor->power = ctr.se; pFactor->rssi;
      ctrFlag = true;
      Serial.printf("set Lora factor: %d, %d\r\n",
        pFactor->power, pFactor->rssi);
      saveFactorToFlash();  
    break;
    case CTR_CHANNEL:
    case SET_CHANNEL:
      pFactor->channel = ctr.se; pFactor->lora_ch;
      ctrFlag = true;
      Serial.printf("set channel: %d, %d\r\n",
        pFactor->power, pFactor->rssi);
      saveFactorToFlash();  
    break;
    case SET_VERSION:
      pFactor->version = ctr.se; pFactor->ble;
      ctrFlag = true;
      Serial.printf("set version: %d, %d\r\n",
        pFactor->power, pFactor->rssi);
      saveFactorToFlash();  
    break;
    default:
      // Serial.printf("no category: %d\r\n", ctr.ca);
    break;
  }

  if(ctrFlag){
    Serial.printf("ca: %d, se: %d, va: %d\r\n",
      ctr.ca, ctr.se, ctr.va);
  }
}

	// for(int i = 0; i < len; i++){
	// 	myJson.frame[i] = *pBuf++;
	// 	if((myJson.frame[i] == '{')&&(!startFlag)){
	// 		myJson.start = i;	
	// 		startFlag = true;
	// 	} 
	// 	else{//alread startFlag
	// 		if(((i - myJson.start) == 6)&&(myJson.frame[i] == '}')){
	// 			// printk("end frame\r\n");
	// 			myJson.end = i;
	// 			myJson.flag = 1;
	// 		}
	// 		// if(myJson.frame[i] == '}') printk("myposition: %d\r\n", i);
	// 	}
	// }

void parseUart(void){
  newFrame_t myFrame = {0, };
  if(Serial2.available()){
    while(Serial2.available() > 0){
      char test = Serial2.read();
      if(test == '{'){
        myFrame.startFlag = true;
        Serial.printf("---------< start frame\r\n");
      }
      else if(myFrame.startFlag){
        myFrame.frame[myFrame.id++] = test;
        Serial.printf("%d,", test); 
        if(test == '}'){
          Serial.printf("---------< end frame\r\n");
          myFrame.startFlag = false;
        } 
      } 
    }
  }
} 

void parseReceiveJson(void){
  uttecJson_t ctr;
  char temp[MAX_DOC] = {0, };
  uint8_t count = 0;
  uint8_t flag0 = 0;
  uint8_t flag1 = 0;
  // send data only when you receive data:
  // Serial.printf("!!!");
  while(Serial2.available() > 0){
    // read the incoming byte:
    char test = Serial2.read();
    if(test == '{') flag0++;
    else if(test == '}'){
      Serial.printf("---------< end frame\r\n");
      flag1++;
    } 
    temp[count++] = test;
    if(count > MAX_DOC){
      // Serial.printf("<------- MAX over ---------->\r\n");
      return;
    } 
  }

  if(!(flag0 && flag1)) return;

  if(count > 0){
    StaticJsonDocument<MAX_DOC> doc;
    DeserializationError error = deserializeJson(doc, temp);
    if (error) {
      // Serial.print(F("deserializeJson() failed: "));
      // Serial.println(error.f_str());
      Serial.printf("json error ---------\r\n");
      return;
    }    

    ctr.no = doc["no"];
    ctr.ca = doc["ca"];
    ctr.se = doc["se"];
    ctr.va = doc["va"];
    ctr.crc = doc["crc"];
    int16_t totalCrc = ctr.no + ctr.va + ctr.ca + ctr.se;
    Serial.printf("no: %d, ca: %d, se: %d, va: %d\r\n", 
      ctr.no, ctr.ca, ctr.se, ctr.va);

    // if(totalCrc == myJason.crc){
      whybiz_t* pFactor = getWhybizFactor();
      switch(ctr.ca){
        case CTR_RELAY:
            Serial.printf("no:%d, ca:%d, se:%d, va:%d, crc:%d\r\n",
            ctr.no, ctr.ca, ctr.se, ctr.va, ctr.crc);
            setRelay(ctr.se, ctr.va);
        break;
        case CTR_LORA:
        case SET_LORA:
            pFactor->power = ctr.se;
            Serial.printf("set lora power-> se: %d, va: %d\r\n", 
                ctr.se, ctr.va);
            Serial.printf("save factor ###########\r\n");
            saveFactorToFlash();
        break;
        case CTR_CHANNEL:
        case SET_CHANNEL:
            pFactor->channel = ctr.se; pFactor->lora_ch = ctr.va;
            Serial.printf("set channel-> uart: %d, lora: %d\r\n", 
                ctr.se, ctr.va);
            Serial.printf("save factor ###########\r\n");
            setUartChannel(pFactor->channel);
            saveFactorToFlash();
        break;
        case SET_VERSION:
            pFactor->version = ctr.se; pFactor->ble = ctr.va;
            pFactor->node = ctr.va;
            Serial.printf("set number-> node: %d, ble: %d\r\n", 
                ctr.se, ctr.va);
            Serial.printf("save factor ###########\r\n");
            saveFactorToFlash();
        break;
        default: Serial.printf("error category: %d\r\n", ctr.ca); break;
      }
  }
}
