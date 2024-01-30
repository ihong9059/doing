#ifndef UTTEC_JSON_H
#define UTTEC_JSON_H

typedef struct{
  uint16_t no;
  uint8_t ca;
  uint8_t se;
  int16_t va;
  uint16_t crc;
} uttecJson_t;

typedef struct{
  bool startFlag;
  uint8_t frame[5];
  uint8_t id;
  bool endFlag;
} newFrame_t;

#define MAX_DOC 100

void testReceive(void);

void parseWifiJson(String pData);

void parseReceiveJson(void);
void testSendJson(void);

void procCmdLine(uttecJson_t);
void procAdc(uttecJson_t);

void parseUart(void);


#endif 
