#ifndef SX1509_LIB_H
#define SX1509_LIB_H

void initSx1509(void);
void testRelay(void);
void testSwitch(void);

void setRelay(uint8_t pin, uint8_t set);

uint8_t readSxRelay(void);

uint8_t readSxSw(void);

#endif

