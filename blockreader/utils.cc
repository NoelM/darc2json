#include <stdint.h>
#include <stdio.h>
#include <cstring>

const uint8_t VALUES[12] = {0x00, 0x4B, 0xFF, 0x1, 0xDD, 0x21, 0x41, 0xC1, 0x81, 0x61, 0xA1, 0xE1};

bool isRemarkable(uint8_t byte) {
  for (int i = 0; i < 12; i++)
    if (VALUES[i] == byte) return true;
  return false;
}

bool byteParity(uint8_t byte) {
  int nbOne = 0;

  for (int i = 0; i < 8; i++) {
    nbOne += (byte & (1 << i)) == 0 ? 0 : 1;
  }
  return nbOne%2 == 0;
}

uint8_t removeParityBit(uint8_t byte) {
  return byte & 0b01111111;
}

uint8_t reverseEndianness(uint8_t byte) {
  uint8_t reversed = 0;
  for (int i = 0; i < 8; i++) {
    if ((byte & (1 << i)) != 0)
      reversed += 1 << (7-i);
  }
  return reversed;
}

int wordPayload(char* line, uint16_t word) {
  unsigned char value = 0;
  for (int inc = 7, wPos = 4; wPos < 12; wPos++, inc--) {
    value += ((word & (1 << wPos)) == 0 ? 0 : 1) << inc;
  }

  return sprintf(line, "  %2X  %c", value, (value >= 0x20 && value <= 0x7E) ? value : ' ');
}

int bytePayload(char* line, uint8_t byte) {
  return sprintf(line, "  %c  %2X  %c", isRemarkable(byte) ? 'R' : ' ', byte, (byte >= 0x20 && byte <= 0x7E) ? byte : ' ');
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

  // First info byte, L-3 header
  uint8_t logChan = bytes[0] & 0b00001111;
  uint8_t decoder = (bytes[0] & 0b00010000) >> 4;
  uint8_t subChan = (bytes[0] & 0b11100000) >> 5;

  uint8_t counter = bytes[1] & 0b01111111;
  bool    active  = (bytes[1] & 0b10000000) != 0;

  return sprintf(line, "%12lld [%1d] [%1X %1X %1X] %3d %c %c ", timeUs, bic, logChan, decoder, subChan, counter, active ? 'A' : ' ', sync ? 'S' : ' ');
}

int sprintLineWord(char* line, uint64_t timeUs, int bic, uint8_t* bytes, uint64_t* wordCounter) {
  int padding = sprintLineHeader(line, timeUs, bic, bytes, true);
  int written = padding;

  uint16_t word;
  char wordRepr[16];

  for (int i = 2; i < 22; i += 2) {
    std::memcpy(&word, &bytes[i], 2);
    wordCounter[word]++;
    getWordRepr(wordRepr, word);

    if (i > 2) {
      for (int j = 0; j < padding; j++) {
        line[written] = ' ';
        written++;
      }
    }

    std::memcpy(&line[written], wordRepr, 16);
    written += 16;

    written += wordPayload(&line[written], word);

    line[written] = '\n';
    written++;
  }

  line[written] = '\0';
  return written;
}

int sprintLineByte(char* line, uint64_t timeUs, int bic, uint8_t* bytes, uint64_t* wordCounter) {
  int padding = sprintLineHeader(line, timeUs, bic, bytes, true);
  int written = padding;

  char byteRepr[8];

  for (int i = 2; i < 22; i++) {
    wordCounter[bytes[i]]++;
    getByteRepr(byteRepr, bytes[i]);

    if (i > 0) {
      for (int j = 0; j < padding; j++) {
        line[written] = ' ';
        written++;
      }
    }

    std::memcpy(&line[written], byteRepr, 8);
    written += 8;

    written += bytePayload(&line[written], bytes[i]);

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

// Terrible sort algorithm, fixed N^2 complexity :ok_hand:
void sortCounter(uint64_t* counter, uint64_t* order, uint64_t len) {
  for (uint64_t i = 0; i < len; i++) order[i] = i;

  uint64_t posi;
  uint64_t posj;
  uint64_t swap;

  for (uint64_t i = 0; i < len; i++) {
    for (uint64_t j = i+1; j < len; j++) {
      posi = order[i];
      posj = order[j];

      if (counter[posi] < counter[posj]) {
        swap = order[i];
        order[i] = order[j];
        order[j] = swap;
      }
    }
  }
}

