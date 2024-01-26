#ifndef SX1509__H__
#define SX1509__H__


#define BIT_0 0B11111110
#define BIT_1 0B11111101
#define BIT_2 0B11111011
#define BIT_3 0B11110111
#define BIT_4 0B11101111
#define BIT_5 0B11011111
#define BIT_6 0B10111111
#define BIT_7 0B01111111

void initSxRelay(void);
void initSx1509(void);
uint8_t readSxRelay(void);
uint8_t readSxSw(void);

bool checkSx1509In(bool);

void writeOutSx(uint8_t bitNum, bool output);

void testRelay(void);
void testSw(void);

#endif 

