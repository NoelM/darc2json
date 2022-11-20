#ifndef BR_UTILS_H_
#define BR_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

int BicFor(uint16_t word);

bool byteParity(uint8_t byte);

uint8_t removeParityBit(uint8_t byte);
uint8_t reverseEndianness(uint8_t byte);

void getByteRepr(char* line, uint8_t byte);
void getWordRepr(char* repr, uint16_t word);

int wordPayload(char* line, uint16_t word);
int bytePayload(char* line, uint8_t byte);

int sprintLineHeader(char* line, uint64_t timeUs, int bic, uint8_t* bytes, bool sync);
int sprintLineWord(char* line, uint64_t timeUs, int bic, uint8_t* bytes, uint64_t* wordCounter);
int sprintLineByte(char* line, uint64_t timeUs, int bic, uint8_t* bytes, uint64_t* wordCounter);
int sprintLinePlain(char* line, uint64_t timeUs, int bic, uint8_t *bytes, bool sync);
int sprintLineHex(char* line, uint64_t timeUs, int bic, uint8_t *bytes, bool sync);

void sortCounter(uint64_t* counter, uint64_t* order, uint64_t len);

#endif

