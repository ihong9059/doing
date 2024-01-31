#include <WiFi.h>       // standard library
#include <WebServer.h>  // standard library
// #include "SuperMon.h"   // .h file that stores your html page code

#include "uttec.h"
#include "myWifi.h"
#include "sx1509Lib.h"
#include "myJson.h"
#include "myBle.h"

#define RXD2 4  //ok
#define TXD2 5  //ok

uint32_t count2 = 0;

void taskParse(void* parameters){
  uint32_t parseCount = 0;
  for(;;){
#ifndef BLE_PROGRAM  
    if(!getWifiConnection())
#endif
      parseUart();
    if(!(parseCount%100))
      Serial.printf("parse: %d\r\n", parseCount);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    parseCount++;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.printf("Now start Whybiz project. 2023.12.23 23:30\r\n");

  initUttec();

  xTaskCreate(taskParse, "Task 1", 2048, //stack size(above 2048)
    NULL, 6, NULL// task handle
  );

#ifdef BLE_PROGRAM  
  initBle();
#else
  initWifi();
#endif
  // while(1);
}

void loop() {
  static uint32_t count = 0;
  uint16_t value = random(300);
  whybiz_t* pFactor = getWhybizFactor();
#ifdef BLE_PROGRAM  
  loop_ble();
#else
  loop_wifi();
#endif  
  loop_uttec();
  // parseReceiveJson();
  delay(500);
  signal();
  // Serial2.printf("AT=U");
  // digitalWrite(RS485_EN, 1);
  // delay(1);
  // Serial2.printf("rs485 test: %d\r\n", count);
  // delay(5);
  // digitalWrite(RS485_EN, 0);

  uint8_t temp = readSxSw();
  // Serial.printf("switch: %x\r\n", temp);
  readAdc();
  dispUartChannel();
  count++;
}
