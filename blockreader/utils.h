#include <stdint.h>

enum eBic { BIC1, BIC2, BIC3, BIC4 };
const uint16_t kBic1 = 0x135E;
const uint16_t kBic2 = 0x74A6;
const uint16_t kBic3 = 0xA791;
const uint16_t kBic4 = 0xC875;

eBic BicFor(uint16_t word) {
  if (word == kBic1)
    return BIC1;
  else if (word == kBic2)
    return BIC2;
  else if (word == kBic3)
    return BIC3;
  else
    return BIC4;
}

bool byteParity(uint8_t byte);

uint8_t removeParityBit(uint8_t byte);
uint8_t reverseEndianness(uint8_t byte);

void getByteRepr(char* line, uint8_t byte);
void getWordRepr(char* repr, uint16_t word);

int wordPayload(char* line, uint16_t word);

int sprintLineHeader(char* line, uint64_t timeUs, int bic, uint8_t* bytes, bool sync);
int sprintLineWord(char* line, uint64_t timeUs, int bic, uint8_t* bytes, uint64_t* wordCounter);
int sprintLinePlain(char* line, uint64_t timeUs, int bic, uint8_t *bytes, bool sync);

void sortCounter(uint64_t* counter, uint64_t* order, uint64_t len);

