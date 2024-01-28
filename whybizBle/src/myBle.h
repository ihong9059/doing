#ifndef MYBLE__H__
#define MYBLE__H__
// 	BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
// 	Service UUID: 			6e400001-b5a3-f393-e0a9-e50e24dcca9e
// 	Tx Characteristic UUID: 6e400003-b5a3-f393-e0a9-e50e24dcca9e
// 	Rx Characteristic UUID: 6e400002-b5a3-f393-e0a9-e50e24dcca9e

void initBle(void);
void procRxBle(uint8_t* data, uint16_t len);
void procTxBle(void);

void procSwitchTxBle(bool);
void procRelayTxBle(bool);
void procAdcTxBle(bool);
#endif 
