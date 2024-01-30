#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "uttec.h"
#include "myBle.h"
#include "sx1509Lib.h"

/* --------------------------------------------------------------------- */
#ifdef BLE_PROGRAM

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void callTest(void){
  Serial.print("This is callTest\r\n");
}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      Serial.printf("length: %d\r\n", rxValue.length());
      Serial.printf("ca: %d, se: %d, va: %d\r\n", rxValue[0], rxValue[1], rxValue[2]);
      uint8_t ca = rxValue[0];
      uint8_t se = rxValue[1];
      uint8_t va = rxValue[2];
      whybiz_t* pFactor = getWhybizFactor();

      callTest();
      
      switch(ca){
        case CTR_RELAY:
          setRelay(se, va);
        break;
        case CTR_CHANNEL:
          setUartChannel(se - 1);
          pFactor->channel = se - 1;
          if(va == 0) pFactor->lora_ch = 20; //for mobile control
          else pFactor->lora_ch = va; //for server control
          printf("<------- setUartChannel ---------->: %d\r\n", se);
        break;
        case CTR_LORA:
          pFactor->power = se;
        break;
      }
    }
};

void initBle(void){
  // Create the BLE Device
  char temp[30] = {0, };
  whybiz_t* pFactor = getWhybizFactor();
  sprintf(temp, "whybiz_esp_%d", pFactor->ble);
  // BLEDevice::init("whybiz for esp32");
  BLEDevice::init(temp);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");  
}

void sendToBle(void){
  if (deviceConnected) {
    whybizFrame_t* pFrame = getWhybizFrame();
    uint8_t ble[3] = {pFrame->category, pFrame->sensor, pFrame->value};
    Serial.printf("ca: %d, se: %d, va: %d\r\n", ble[0],ble[1],ble[2]);  
  
    pTxCharacteristic->setValue(ble, sizeof(ble));
    pTxCharacteristic->notify();
  }
}

void loop_ble(void){
  if (deviceConnected) {
      Serial.printf("Ble connected..\r\n"); 
      sendToBle();
      // parseReceiveJson();
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // give the bluetooth stack the chance to get things ready
      pServer->startAdvertising(); // restart advertising
      Serial.println("start advertising");
      oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
  // do stuff here on connecting
      oldDeviceConnected = deviceConnected;
      Serial.println("wait\r\n");
  }

}

#endif 
