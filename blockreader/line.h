#ifndef BR_LINE_H
#define BR_LINE_H

#define LINE_LEN 44

#include <inttypes.h>

struct Line
{
    uint64_t timestamp;
    uint8_t  bic;
    uint8_t  logicChannel;
    uint8_t  decodeIndicator;
    uint8_t  subChannel;

    uint8_t  localCounter;
    uint8_t  activeBit;

    uint8_t* raw;
};

struct Line* allocLine(struct Line* line);
int decodeLine(struct Line* line, char* raw);
int sprintHeader(struct Line* line, char* buffer);
int sprintHex(struct Line* line, char* buffer);

#endif