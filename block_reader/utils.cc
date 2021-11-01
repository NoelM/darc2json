#include <stdint.h>
#include <stdio.h>
#include <cstring>

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

char byteChar(uint8_t* bytes) {
  uint16_t word;
  std::memcpy(&word, bytes, 2);

  unsigned char value = 0;
  for (int inc = 0, wPos = 5; wPos < 12; wPos++, inc++) {
    value += ((word & (1 << wPos)) == 0 ? 0 : 1) << inc;
  }

  return (value >= 0x20 && value <= 0x7E) ? value : ' ';
}

void getByteRepr(char* repr, uint8_t byte) {
  for (int i = 0; i < 8; i++)
    repr[i] = ((byte & (1 << i)) == 0) ? '_' : '#';
}

void getWordRepr(char* repr, uint16_t word) {
  for (int i = 0; i < 16; i++)
    repr[i] = ((word & (1 << i)) == 0) ? '_' : '#';
}

int sprintLineHeader(char* line, uint64_t timeUs, int bic, uint8_t* bytes, bool sync) {
  return sprintf(line, "%12lld %1d %2X %3d %c ", timeUs, bic, bytes[0], bytes[1], sync ? 'S' : ' ');
}

int sprintLineWord(char* line, uint64_t timeUs, int bic, uint8_t* bytes) {
  int padding = sprintLineHeader(line, timeUs, bic, bytes, true);
  int written = padding;

  uint16_t word;
  char wordRepr[16];

  for (int i = 2; i < 22; i += 2) {
    std::memcpy(&word, &bytes[i], 2);
    getWordRepr(wordRepr, word);

    if (i > 2) {
      for (int j = 0; j < padding; j++) {
        line[written] = ' ';
        written++;
      }
    }

    std::memcpy(&line[written], wordRepr, 16);
    written += 16;

    line[written] = '\n';
    written++;
  }

  line[written] = '\0';
  return written;
}

int sprintLinePlain(char* line, uint64_t timeUs, int bic, uint8_t *bytes, bool sync) {
  int written = sprintLineHeader(line, timeUs, bic, bytes, sync); 

  char byteRepr[8];
  for (int i = 2; i < 22; i++) {
    getByteRepr(byteRepr, bytes[i]);
    
    std::memcpy(&line[written], byteRepr, 8);
    written += 8;
  }
  line[written] = '\n';
  written++;

  line[written] = '\0';
  return written;
}

