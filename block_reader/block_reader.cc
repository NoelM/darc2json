#include <stdio.h>
#include <iostream>
#include <fstream>
#include <chrono>

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

bool parityCheck(uint8_t byte) {
  int nbOne = 0;

  for (int i = 0; i < 8; i++) {
    nbOne += (byte & (1 << i)) == 0 ? 0 : 1;
  }
  return nbOne%2 == 0;
}

uint8_t removeParityBit(uint8_t byte) {
  if (parityCheck(byte))
    return byte & 0b01111111;

  return byte;
}

uint8_t reverseEndianess(uint8_t byte) {
  uint8_t reversed = 0;
  for (int i = 0; i < 8; i++) {
    if ((byte & (1 << i)) != 0)
      reversed += 1 << (7-i);
  }
  return reversed;
}

char printChar(uint8_t byte) {
  if (parityCheck(byte))
    byte = byte & 0b01111111;

  if (byte >= 0x20 && byte < 0x7F)
    return char(byte);

  return ' ';
}

void printBits(char* line, uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    if ((byte & (1 << i)) == 0) {
      line[i] = ' ';
    } else {
      line[i] = '#';
    }
  }
}

char byteChar(uint8_t* bytes) {
  uint16_t word;
  std::memcpy(&word, bytes, 2);

  char value = 0;
  for (int inc = 0, wPos = 5; wPos < 13; wPos++, inc++) {
    value += ((word & (1 << wPos)) == 0 ? 0 : 1) << inc;
  }

  return value;
}

void printLine(uint8_t* bytes, int len) {
  char line[8];

  for (int i = 0; i < len/2; i++) {
    printBits(line, bytes[2*i]);
    for (int j = 0; j < 8; j++) printf("%c", line[j]);

    printBits(line, bytes[2*i+1]);
    for (int j = 0; j < 8; j++) printf("%c", line[j]);


    printf(" %c\n", byteChar(&bytes[2*i]));
  }
}

int main(int argc, char** argv) {
  if (argc == 1) {
    return 0;
  }

  std::string filename = std::string(argv[1]);
  std::ifstream binaryFile(argv[1], std::ios::out | std::ios::binary);  

  int bicCounters[4] = {0, 0, 0, 0};

  uint8_t channel;

  uint8_t lineId = 0;
  uint8_t info[22];

  uint8_t prevLineId = 0;
  uint8_t prevInfo[22];

  bool sync = false;

  char buffer[44];
  while (binaryFile.good()) {
    binaryFile.read(buffer, 44);

    uint64_t timeUs;
    std::memcpy(&timeUs, &buffer, 8);

    uint16_t bic;
    std::memcpy(&bic, &buffer[8], 2);
    bicCounters[BicFor(bic)] += 1;

    std::memcpy(&prevInfo, &info, 22);
    prevLineId = lineId;

    std::memcpy(&info, &buffer[10], 22);

    channel = info[0];
    lineId  = info[1];

    if (lineId == prevLineId + 1) {
      if (sync) {
        printLine(&info[2], 20);
      } else {
        printLine(&prevInfo[2], 20);
        printLine(&info[2], 20);
        sync = true;
      }
    } else {
      sync = false;
      printf("\n\n");
    }

    /*printf("%12lld %1d %2X %3d", timeUs, BicFor(bic) + 1, info[0], info[1]);

    char line[8];
    for (int i = 2; i < 22; i++) {
      //uint8_t byte = reverseEndianess(info[i]);
      //printf(" %c", printChar(info[i]));
      printBits(line, info[i]);
      for (int i = 0; i < 8; i++) printf("%c", line[i]);
      /*
      printf(" %2X", byte);
      if (parityCheck(byte)) {
        printf("+ ");
      } else {
        printf("  ");
      }
      // here
    }
    printf("\n");*/
  }

  //std::cout << "STATS" << std::endl;
  //std::cout << "BIC1 " << bicCounters[0] << " -- BIC2 " << bicCounters[1] << " -- BIC3 " << bicCounters[2] << " -- BIC4 " << bicCounters[3] << std::endl;

  return 0;
}
