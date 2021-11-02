#include <stdio.h>
#include <iostream>
#include <fstream>
#include <chrono>

#include "utils.h"

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

int main(int argc, char** argv) {
  if (argc == 1) {
    return 0;
  }

  std::string filename = std::string(argv[1]);
  std::ifstream binaryFile(argv[1], std::ios::out | std::ios::binary);  
  
  uint64_t wordCounter[0x10000];
  for (int i = 0; i < 0x10000; i++)
    wordCounter[i] = 0;

  uint16_t bic;
  uint8_t  channel;
  bool     sync = false;

  uint64_t timeUs;
  int      ebic;
  uint8_t  lineId = 0;
  uint8_t  info[22];

  uint64_t prevTimeUs;
  int      prevEbic;
  uint8_t  prevLineId = 0;
  uint8_t  prevInfo[22];

  int  written;
  char string[1024];
  char buffer[44];

  while (binaryFile.good()) {
    // Read binary line
    binaryFile.read(buffer, 44);

    // Swap and copy time
    prevTimeUs = timeUs;
    std::memcpy(&timeUs, &buffer, 8);

    // Swap and copy BIC
    std::memcpy(&bic, &buffer[8], 2);

    prevEbic = ebic;
    ebic = BicFor(bic) + 1;

    // Swap and copy Info
    std::memcpy(&prevInfo, &info, 22);
    std::memcpy(&info, &buffer[10], 22);

    channel    = info[0]; // not used yet

    prevLineId = lineId;
    lineId     = info[1];

    // Print depending on sync
    if (lineId == prevLineId + 1) {
      if (sync) {
        written = sprintLineWord(string, timeUs, ebic, info, wordCounter); 
        printf("%s", string);
      } else {
        written = sprintLineWord(string, timeUs, prevEbic, prevInfo, wordCounter);
        printf("%s", string);

        written = sprintLineWord(string, timeUs, ebic, info, wordCounter);
        printf("%s", string);

        sync = true;
      }
    } else {
      if (sync) {
        sync = false;
      }
      written = sprintLinePlain(string, timeUs, BicFor(bic) + 1, info, sync);
      printf("%s", string);
    }
  }

  uint64_t counterOrder[0x10000];
  sortCounter(wordCounter, counterOrder, 0x10000);
 
  char wordString[17];
  for (int i = 0; i < 0x10000; i++) {
    uint64_t pos = counterOrder[i];
    if (wordCounter[pos] > 0) {
      getWordRepr(wordString, uint16_t(pos));
      wordString[16]= '\0';
      printf("%5lld   %10lld   %s\n", pos, wordCounter[pos], wordString);
    }
  }

  return 0;
}
