#include <stdio.h>
#include <iostream>
#include <fstream>

#include "utils.h"

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
  bool     sync = false;

  uint64_t timeUs;
  int      ebic;
  uint8_t  lineId = 0;
  uint8_t  info[22];

  uint64_t prevTimeUs;
  int      prevEbic;
  uint8_t  prevLineId = 0;
  uint8_t  prevInfo[22];

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

    prevLineId = lineId;
    lineId     = info[1];

    // Print depending on sync
    /*
    if (lineId == prevLineId + 1) {
      if (sync) {
        sprintLineWord(string, timeUs, ebic, info, wordCounter);
        printf("%s", string);
      } else {
        sprintLineWord(string, prevTimeUs, prevEbic, prevInfo, wordCounter);
        printf("%s", string);

        sprintLineWord(string, timeUs, ebic, info, wordCounter);
        printf("%s", string);

        sync = true;
      }
    } else {
      if (sync) {
        sync = false;
      }
      sprintLinePlain(string, timeUs, BicFor(bic) + 1, info, sync);
      printf("%s", string);
    }*/

    sprintLinePlain(string, timeUs, BicFor(bic) + 1, info, sync);
    printf("%s", string);
  }

  /*
  uint64_t counterOrder[0x10000];
  sortCounter(wordCounter, counterOrder, 0x10000);
 
  char wordString[17];
  char payload[8];
  for (int i = 0; i < 0x10000; i++) {
    uint64_t pos = counterOrder[i];
    if (wordCounter[pos] > 0) {
      getWordRepr(wordString, uint16_t(pos));
      wordString[16]= '\0';

      wordPayload(payload, uint16_t(pos));

      printf("%5lld   %10lld   %s%s\n", pos, wordCounter[pos], wordString, payload);
    }
  }
  */

  return 0;
}
