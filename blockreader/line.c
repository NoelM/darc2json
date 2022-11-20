#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "line.h"
#include "utils.h"

struct Line* allocLine(struct Line* line) {
    line->raw = (uint8_t*) malloc(LINE_LEN * sizeof(uint8_t));
    return line;
}

int decodeLine(struct Line* line, char* raw) {
    memcpy(&line->timestamp, raw, 8);
    memcpy(&line->bic, raw+8, 2);

    line->logicChannel    = raw[10] & 0b00001111;
    line->decodeIndicator = (raw[10] & 0b00010000) >> 4;
    line->subChannel      = (raw[10] & 0b11100000) >> 5;
    line->localCounter    = raw[11] & 0b01111111;
    line->activeBit       = (raw[11] & 0b10000000) != 0;

    memcpy(line->raw, raw, LINE_LEN);

    return 0;
}

int sprintHeader(struct Line* line, char* buffer) {
    return sprintf(buffer, "%12lld [%1d] [%1X %1X %1X] %3d %c ", 
        line->timestamp, 
        BicFor(line->bic)+1, 
        line->logicChannel,
        line->decodeIndicator, 
        line->subChannel, 
        line->localCounter, 
        line->activeBit ? 'A' : ' ');
}

int sprintHex(struct Line* line, char* buffer) {
  int written = sprintHeader(line, buffer); 

  for (int i = 2; i < 22; i++) {
    if (line->raw[10+i] == 0) {
        written += sprintf(buffer+written, "   ");
    } else {
        written += sprintf(buffer+written, "%02X ", line->raw[10+i]);
    }
  }
  buffer[written] = '\n';
  written++;

  buffer[written] = '\0';
  return written;
}