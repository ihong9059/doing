
#include <ArduinoJson.h>
#include "uttec.h"
#include "myJson.h"

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
  ack_t ack;

  switch(ctr.ca){
    case CTR_RELAY:
      Serial.printf("setRelay\r\n");
      setRelay(ctr.se, ctr.va);
      ack.ca = 3; ack.se = pFactor->relay; 
      ack.va = pFactor->relay; ack.crc = ctr.crc;
      sendAck(ack);
      saveFactorToFlash();  

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

void parseUart(void){
  newFrame_t myFrame = {0, };
  if(Serial2.available()){
    // Serial.printf("++++++++++++++++++++++++++++\r\n");
    while(Serial2.available() > 0){
      char test = Serial2.read();
      // Serial.printf("%d,", test); 
      if(test == '{'){
        myFrame.startFlag = true;
        // Serial.printf("---------< start frame\r\n");
      }
      else if(myFrame.startFlag){
        myFrame.frame[myFrame.id++] = test;
        Serial.printf("%d,", test); 
        if(test == '}'){
          Serial.printf("---------< end frame\r\n");
          myFrame.startFlag = false;
          myFrame.endFlag = true;
        } 
      } 
    }
    // Serial.printf("++++++++++++++++++++++++++++\r\n");
  }

  if(myFrame.endFlag){// process cmd
    uttecJson_t ctr;
    ctr.no = myFrame.frame[0];
    ctr.ca = myFrame.frame[1];
    ctr.se = myFrame.frame[2];
    ctr.va = myFrame.frame[3];
    ctr.crc = myFrame.frame[4];

    // if(pFactor->channel == LORA_CHANNEL){
    //     if(ctr.ca == 12) ctr.ca = CTR_RELAY;
    // }
    if(ctr.ca == 12) ctr.ca = CTR_RELAY; //for lora pass, when 13, return, no message pass

    int16_t totalCrc = ctr.no + ctr.va + ctr.ca + ctr.se;
    Serial.printf("no: %d, ca: %d, se: %d, va: %d\r\n", 
      ctr.no, ctr.ca, ctr.se, ctr.va);

    ack_t ack;
    whybiz_t* pFactor = getWhybizFactor();
    switch(ctr.ca){
      case CTR_RELAY:
          Serial.printf("no:%d, ca:%d, se:%d, va:%d, crc:%d\r\n",
          ctr.no, ctr.ca, ctr.se, ctr.va, ctr.crc);
          setRelay(ctr.se, ctr.va);
          ack.ca = 3; ack.se = pFactor->relay; 
          ack.va = pFactor->relay; ack.crc = ctr.crc;
          sendAck(ack);
          saveFactorToFlash();  
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
