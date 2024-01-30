#include <WiFi.h>       // standard library
#include <WebServer.h>  // standard library
// #include "SuperMon.h"   // .h file that stores your html page code

#include "uttec.h"
#include "myWifi.h"
#include "sx1509Lib.h"
#include "myJson.h"

#ifndef BLE_PROGRAM

// #define USE_INTRANET
// #define MYHOME 1


#ifdef MYHOME
#define LOCAL_SSID "ihong"
#define LOCAL_PASS "hongks@6063"
IPAddress local_IP(192, 168, 0, 111);
IPAddress local_gw(192, 168, 0, 1);
#else
#define LOCAL_SSID "uttec8"
#define LOCAL_PASS "a123456789"
IPAddress local_IP(192, 168, 8, 111);
IPAddress local_gw(192, 168, 8, 1);
#endif

#define AP_SSID "uttec_whybiz"
#define AP_PASS "uttec_whybiz"

char XML[2048];
static uint8_t mySelect = 0;

IPAddress Actual_IP;

IPAddress PageIP(192, 168, 1, 3);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;

IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

WiFiServer uttecServer(20000);

void initWifi(void){
  disableCore0WDT();
#ifdef USE_INTRANET
  if (!WiFi.config(local_IP, local_gw, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  Serial.print("Connecting to ");
  Serial.println(LOCAL_SSID);
  WiFi.begin(LOCAL_SSID, LOCAL_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Actual_IP = WiFi.localIP();
#endif

#ifndef USE_INTRANET
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  WiFi.softAPConfig(PageIP, gateway, subnet);
  delay(100);
  Actual_IP = WiFi.softAPIP();
  Serial.print("IP address: "); Serial.println(Actual_IP);
#endif

  printWifiStatus();
  uttecServer.begin();
  whybiz_t* pData = getWhybizFactor();
  // testEeprom();
}

// #include <WiFi.h>

const uint16_t myPort = 8091;
const char* myHost = "192.168.1.4";

void connectToServer(void){
  static uint32_t count = 0;
  WiFiClient client;
  if(!client.connect(myHost, myPort)){
    Serial.printf("Connection fail\r\n");
    return;
  }
  client.printf("Hello from esp32: %d", count++);
  client.stop();
}

void serverForGeneral(void){
  static bool connectFlag = false;
  WiFiClient uttec_client = uttecServer.available();

  if(!uttec_client){
    return;
  }

  Serial.printf("new client");

  while(uttec_client.connected()){
    parseReceiveJson();
    if(!connectFlag){
      Serial.print("remoteIP: ");
      Serial.println(uttec_client.remoteIP());
      printf("\r\nremotePort: %d\r\n", uttec_client.remotePort());
      connectFlag = true;
    }
    if(uttec_client.available()){
      static uint32_t count = 0;
      whybiz_t* pFactor = getWhybizFactor();

      String receiveData = uttec_client.readStringUntil('\n');
      // Serial.println(receiveData);
      parseWifiJson(receiveData);
      char temp [100] = {0, };
      uint8_t ca = count % 7;
      uint8_t se;
      uint8_t va;
      bool sendFlag = false;
      switch(ca){
        case ADC_DEVICE: 
          readAdc();
          se = pFactor->adc1; va = pFactor->adc2; sendFlag = true; break;
        case SWITCH_DEVICE:
          pFactor->sw = readSxSw(); 
          se = pFactor->sw; va = pFactor->sw; sendFlag = true; break;
        case RELAY_DEVICE: se = pFactor->relay; va = pFactor->relay; sendFlag = true; break;
        case LORA_DEVICE: se = pFactor->power; va = pFactor->rssi; sendFlag = true; break;
        case VERSION_DEVICE: se = pFactor->version; va = pFactor->ble; sendFlag = true; break;
        case CHANNEL_DEVICE: se = pFactor->channel; va = pFactor->lora_ch; sendFlag = true; break;
        default: Serial.printf("return: %d\r\n", ca); break;
      }
      
      uint8_t len = sprintf(temp, "{\"ca\":%d,\"se\":%d,\"va\":%d}\n", ca, se, va);
      if(sendFlag){
        uttec_client.write_P(temp, len);
      }  
      count++;
      sendJsonForStatus();
    }
  }
  connectFlag = false;
  Serial.printf("connection end\r\n");
}

void loop_wifi(void){
  char temp[100];
  serverForGeneral(); //for uttec_client, port: 2000
  // connectToServer(); //for myPort: 8091, 192.168.1.4
}


void SendWebsite() {
  Serial.println("sending web page");
}

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.print("Open http://");
  Serial.println(ip);
}


#endif