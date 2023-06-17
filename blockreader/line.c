#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "line.h"
#include "utils.h"

void allocLine(struct Line* line) {
    line->raw = (uint8_t*) malloc(LINE_LEN * sizeof(uint8_t));
}

int decodeLine(struct Line* line, char* raw) {
    // 0:7 (8) bytes from json2darc code a UNIX timestamp
    memcpy(&line->timestamp, raw, 8);

    // 8:9 (2) bytes code a BIC number
    // BIC stands for Block Identification Code
    memcpy(&line->bic, raw+8, 2);

    // 0:3 (4) bits code a logic channel
    // Service Channel,       SeCh, 0x8
    // Short Message Channel, SMCh, 0x9
    // Long Message Channel,  LMCh, 0xA
    // Block Message Channel, BMCh, 0xB
    line->logicChannel    =  raw[10] & 0b00001111;

    // BMCh, 4 bits codes the Decode Indicator
    // "Set to 1 for real time information"
    line->decodeIndicator = (raw[10] & 0b00010000) >> 4;

    // BMCh, 5:7 (3) bits code the SubChannel (SCh)
    // 0  : Block Application Channel.
    // 1-3: Reserved.
    // 4  : Synchronous Frame Message
    // 5-6: Reserved
    // 7  : Reserved for Future Extension (RFE)
    line->subChannel      = (raw[10] & 0b11100000) >> 5;

    // Application Specifications
    // _not part of DARC specifications_
    // 
    // Data block are sent, each block is made of multiple lines.
    // Each line is prefixed with a counter, 
    // and a sync bit which indicates the begining of the frame

    // 0:6 (7) bits code a Local Counter, to identify lines of a frame   
    line->lineId    =  raw[11] & 0b01111111;

    // 7th bit codes a sync bit
    line->blockSync       = (raw[11] & 0b10000000) != 0;

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
        line->lineId, 
        line->blockSync ? 'A' : ' ');
}

int sprintHex(struct Line* line, char* buffer) {
    int written = sprintHeader(line, buffer); 

    // The DARC protocol specifies that a line contains 44 bytes
    // 
    // Here, we added a prefix of 10 bytes long for:
    // * (8 bytes) the UNIX timestamp
    // * (2 bytes) the Block Identification Code (BIC)
    //
    // Thus, the DARC-raw-line data start at byte 10,
    // and structured as follows:
    // * 10th byte (in line header), codes the Logic Channel
    // * 11th byte (in line header), codes the Counter and Active Byte 
    // * 12th to 32nd bytes, code raw data
    // * 33rd to 44th bytes, code CRC
    for (int i = 2 + 10; i < 22 + 10; i++) {
        if (line->raw[i] == 0) {
            // Do not prinx 00, instead a SPACE
            written += sprintf(buffer+written, "   ");
        } else {
            // If non-zero, print the Hex code
            written += sprintf(buffer+written, "%02X ", line->raw[i]);
        }
    }

    buffer[written] = '\n';
    written++;

    buffer[written] = '\0';
    return written;
}