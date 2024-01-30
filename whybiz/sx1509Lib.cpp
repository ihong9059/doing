#include <Wire.h>           // Include the I2C library (required)
#include <SparkFunSX1509.h> //Click here for the library: http://librarymanager/All#SparkFun_SX1509
// #include "SparkFunSX1509.h" //Click here for the library: http://librarymanager/All#SparkFun_SX1509

// #include "mySpark.h"
#include "sx1509Lib.h"
#include "myJson.h"
#include "uttec.h"

static uint8_t relayStatus = 0;

const byte SX1509_ADDRESS = 0x3E; // SX1509 I2C address
SX1509 io;                        // Create an SX1509 object to be used throughout


void initSx1509(void){
  Serial.println("SX1509 Example");
  Wire.begin();
  if (io.begin(SX1509_ADDRESS) == false)
  {
    Serial.println("Failed to communicate. Check wiring and address of SX1509.");
    while (1);
  }
  for(uint8_t i = 0; i < 8; i++){
    io.pinMode(i + 8, OUTPUT); // Set LED pin to OUTPUT
    io.pinMode(i, INPUT_PULLUP); // Set LED pin to OUTPUT
  }
}

void testRelay(void){
  static uint16_t count = 0;
  static bool toggle = false;
  if(toggle){
    setRelay(count % 8, 1);
    count++;
  }
  else{
    setRelay(count % 8, 0);
  }
  toggle = !toggle;
}

void setRelay(uint8_t pin, uint8_t set){
  whybiz_t* pFactor = getWhybizFactor();
  io.digitalWrite(8 + pin, set);
  uint8_t temp = 0x01;
  temp = temp << pin;
  if(set){
    relayStatus |= temp;
  }
  else{
    relayStatus &= ~temp;
  }
  pFactor->relay = relayStatus;
}

uint8_t readSxRelay(void){
  return relayStatus;
}

uint8_t readSxSw(void){
  whybiz_t* pFactor = getWhybizFactor();
  // printf("readSxSw\r\n");
  uint8_t result = 0;
  for(uint8_t i = 0; i < 8; i++){
    uint8_t temp = 0x01;
    temp <<= i;
    if(io.digitalRead(i)){
      result = result | (temp) ;
    } 
    else{
      result = result & ~temp;
    }
  }
  pFactor->sw = result;
  return result;
}
