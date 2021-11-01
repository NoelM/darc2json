#include <stdint.h>

bool parityCheck(uint8_t byte);

uint8_t removeParityBit(uint8_t byte);

uint8_t reverseEndianess(uint8_t byte);

char printChar(uint8_t byte);

void getByteRepr(char* line, uint8_t byte);

char byteChar(uint8_t* bytes);

int sprintLineHeader(char* line, uint64_t timeUs, int bic, uint8_t* bytes, bool sync);
int sprintLineWord(char* line, uint64_t timeUs, int bic, uint8_t* bytes);
int sprintLinePlain(char* line, uint64_t timeUs, int bic, uint8_t *bytes, bool sync);

