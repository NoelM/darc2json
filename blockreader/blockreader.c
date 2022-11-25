#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "line.h"

#define FILTER_BLOCK 0x07

int main(int argc, char** argv) {
  if (argc == 1) {
    printf("usage ./blockreader [binary block]");
    return 0;
  }

  FILE *fptr;
  fptr = fopen(argv[1], "rb");

  char string[1024];

  uint8_t bufferPos = 0;
  char buffer[44];

  int blockPos = 0;
  char block[1024];

  bool followBlock;

  struct Line line;
  allocLine(&line);

  uint8_t lineId = 0;
  uint8_t prevLineId = 0;

  uint8_t blockId = 0;

  while (feof(fptr) == 0) {
    // Read binary line
    fread(buffer, 1, 44, fptr);
    bufferPos = 12;

    decodeLine(&line, buffer);
    if (line.decodeIndicator) {
      continue;
    }

    if (line.activeBit) {
      /*
      if (blockPos > 0 && blockPos%4 == 0) {
        printf("BLOCK %d %1d\n", blockId, (followBlock ? 1 : 0));

        int bp;
        for (bp = 0; bp < blockPos; bp++) {
          if (bp > 0 && bp%4 == 0) printf("\n");
          printf("%02X ", block[bp] & 0xff); 
        }
        printf("\n\n");

      } else {
        printf("corrupted blockId:%d blockPos: %d\n", blockId, blockPos);
      }
      */

      blockPos = 0;

      blockId = buffer[bufferPos];
      bufferPos++;

      followBlock = buffer[bufferPos] != 0;
      bufferPos++;

      if (blockId == FILTER_BLOCK && !followBlock) printf("\n");
    }

    /*
    for (;bufferPos < 32; bufferPos++) {
      if (buffer[bufferPos] == 0) continue;
      block[blockPos] = buffer[bufferPos];
      blockPos++;
    }*/

    if (blockId == FILTER_BLOCK && !followBlock) {
      sprintHex(&line, string);
      printf("%s", string);
    }
  }

  return 0;
}

/*
int main(int argc, char** argv) {
  if (argc == 1) {
    printf("usage ./blockreader [binary block]");
    return 0;
  }

  std::string filename = std::string(argv[1]);
  std::ifstream binaryFile(argv[1], std::ios::out | std::ios::binary);  
  
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

  struct Line line;
  allocLine(&line);

  while (binaryFile.good()) {
    // Read binary line
    binaryFile.read(buffer, 44);

    decodeLine(&line, buffer);
    if (line.decodeIndicator) {
      continue;
    }

    if (line.activeBit) {
      printf("\n");
    }

    sprintHex(&line, buffer);
    printf("%s", buffer);

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
    lineId     = info[1] & (0b01111111);

    bool groupByWord = false;

    sprintLineHex(string, timeUs, BicFor(bic) + 1, info, sync);
    printf("%s", string);

    // Print depending on sync
    if (lineId == prevLineId + 1) {
      if (sync) {
        if(groupByWord) sprintLineWord(string, timeUs, ebic, info, reprCounter);
        else sprintLineByte(string, timeUs, ebic, info, reprCounter);
        printf("%s", string);

      } else {
        if(groupByWord) sprintLineWord(string, prevTimeUs, prevEbic, prevInfo, reprCounter);
        else sprintLineByte(string, prevTimeUs, prevEbic, prevInfo, reprCounter);
        printf("%s", string);

        if(groupByWord) sprintLineWord(string, timeUs, ebic, info, reprCounter);
        else sprintLineByte(string, timeUs, ebic, info, reprCounter);
        printf("%s", string);

        sync = true;
      }
    } else {
      if (sync) {
        sync = false;
      }
      sprintLinePlain(string, timeUs, BicFor(bic) + 1, info, sync);
      printf("%s", string);
    }
  }

  //uint64_t counterOrder[0x10000];
  //sortCounter(reprCounter, counterOrder, 0x10000);

  // FOR WORD = 16b
  char wordString[17];
  char payload[8];
  for (int i = 0; i < 0x10000; i++) {
    uint64_t pos = counterOrder[i];
    if (reprCounter[pos] > 0) {
      getWordRepr(wordString, uint16_t(pos));
      wordString[16]= '\0';

      wordPayload(payload, uint16_t(pos));

      printf("%5lld   %10lld   %s%s\n", pos, reprCounter[pos], wordString, payload);
    }
  }

  // FOR BYTES = 8b
  char byteString[17];
  char payload[30];
  for (int i = 0; i < 0x10000; i++) {
    uint64_t pos = counterOrder[i];
    if (reprCounter[pos] > 0) {
      getByteRepr(byteString, uint16_t(pos));
      byteString[8]= '\0';

      bytePayload(payload, uint8_t(pos));

      printf("%5lld   %10lld   %s%s\n", pos, reprCounter[pos], byteString, payload);
    }
  }

  return 0;
}
*/
