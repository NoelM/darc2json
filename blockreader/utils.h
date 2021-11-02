#include <stdint.h>

bool parityCheck(uint8_t byte);

uint8_t removeParityBit(uint8_t byte);

uint8_t reverseEndianess(uint8_t byte);

char printChar(uint8_t byte);

void getByteRepr(char* line, uint8_t byte);
void getWordRepr(char* repr, uint16_t word);

char byteChar(uint16_t word);

int sprintLineHeader(char* line, uint64_t timeUs, int bic, uint8_t* bytes, bool sync);
int sprintLineWord(char* line, uint64_t timeUs, int bic, uint8_t* bytes, uint64_t* wordCounter);
int sprintLinePlain(char* line, uint64_t timeUs, int bic, uint8_t *bytes, bool sync);

void sortCounter(uint64_t* counter, uint64_t* order, uint64_t len);

