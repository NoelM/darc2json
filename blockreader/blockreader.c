#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "line.h"

// Table layout, measured on the recordings (no spec):
// 32 blocks of 16 lines; sub-block 0 = 13 lines, sub-block 1 = 3 lines.
// Each sub-block starts with a sync line whose first 2 bytes are (blockId, subId).
// Everything else is little-endian 16-bit words: 158 per block = 79 slots of 2 words.
//
// Blocks 0..31 are only a rotating transport window. Before each block, a
// directory line (header 0x5B, sync bit, payload 01 00 00 03 30) announces
// which page the block carries: 16-bit page id, block number, length in bytes.
// A slot is identified by (page id, slot index).
#define BLOCKS   32
#define ROWS     16
#define SUB1_ROW 13
#define WORDS    158
#define MAX_GAP_US 1000000
#define MAX_PAGES 512
#define DIR_TTL_US 5000000  // a directory entry announces a block of the same ~5 s frame

// Index of the first word of a row: 10 words per row, minus the 2 sync headers
static int firstWord(int row) { return row * 10 - (row > 0) - (row > SUB1_ROW); }

// Clock line: header 0x5B, no sync bit, last 4 payload bytes read big-endian =
// 9 unknown bits | hour:5 | minute:6 | second:6 | 000010. Returns seconds of day or -1.
static int clockSeconds(const uint8_t* payload) {
  uint32_t v = (uint32_t)payload[16] << 24 | payload[17] << 16 | payload[18] << 8 | payload[19];
  int h = v >> 18 & 31, m = v >> 12 & 63, sec = v >> 6 & 63;
  if ((v & 63) != 2 || h > 23 || m > 59 || sec > 59) return -1;
  return h * 3600 + m * 60 + sec;
}

// In all five recordings, tick 0 falls one hour before a multiple of 4 h on the stream clock
#define TICK_OFFSET_S 3600
#define CYCLE_S 14400

// word = time in 10 s ticks (mod 1440) << 5 | 5-bit flag, or 0xFFxx status code.
// Times are printed as arrival time on the stream clock, then the wait from `now`.
static void sprintWord(char* s, uint16_t w, int now) {
  if (w == 0)              strcpy(s, "         -          ");
  else if (w >> 8 == 0xFF) sprintf(s, "       FF%02X         ", w & 0xFF);
  else {
    int wait = (((w >> 5) * 10 - now - TICK_OFFSET_S) % CYCLE_S + CYCLE_S) % CYCLE_S;
    if (wait > CYCLE_S - 3600) wait -= CYCLE_S;  // up to 1 h in the past: already gone
    int eta = ((now + wait) % 86400 + 86400) % 86400;
    sprintf(s, "%02d:%02d:%02d %c%2d:%02d /%-2d", eta / 3600, eta / 60 % 60, eta % 60,
            wait < 0 ? '-' : '+', abs(wait) / 60, abs(wait) % 60, w & 31);
  }
}

int main(int argc, char** argv) {
  assert(clockSeconds((const uint8_t*)"0123456789abcdef\xcb\x2d\x16\x02") == 11 * 3600 + 17 * 60 + 24);
  assert(firstWord(1) == 9 && firstWord(SUB1_ROW) == 129 && firstWord(ROWS) == WORDS);

  if (argc == 1) {
    printf("usage ./blockreader [binary block] [page (hex) slot]\n"
           "  without page/slot: every slot, printed when it changes\n"
           "  with page/slot:    that slot, printed every time it is received\n");
    return 0;
  }
  int wantPage = argc > 3 ? (int)strtol(argv[2], NULL, 16) : -1;
  int wantSlot = argc > 3 ? atoi(argv[3]) : -1;

  setvbuf(stdout, NULL, _IOLBF, 0);  // line-buffered so a pipe reader sees lines at once
  FILE* fptr = fopen(argv[1], "rb");
  if (!fptr) {
    perror(argv[1]);
    return 1;
  }

  static uint16_t word[MAX_PAGES][WORDS];
  static uint16_t printed[MAX_PAGES][WORDS];
  static uint64_t seenAt[MAX_PAGES][WORDS];  // 0 = never received

  static int16_t pageOf[0x10000];  // page id -> index + 1, 0 = unknown
  static uint16_t pageId[MAX_PAGES];
  int pages = 0;

  // Directory: what each transport block currently carries
  int      dirPage[BLOCKS];
  int      dirWords[BLOCKS];
  uint64_t dirTime[BLOCKS] = {0};

  char buffer[LINE_LEN];
  struct Line line;
  allocLine(&line);

  int block = -1;  // -1 = waiting for a sync line
  int page = 0;
  int syncRow = 0;
  uint8_t syncLineId = 0;
  uint64_t syncTime = 0;
  int      clock = -1;  // seconds of day at clockTime, -1 = no clock line seen yet
  uint64_t clockTime = 0;

  while (fread(buffer, 1, LINE_LEN, fptr) == LINE_LEN) {
    decodeLine(&line, buffer);

    if (line.logicChannel != 0xB || line.subChannel != 2) continue;

    uint8_t* payload = line.raw + 12;
    if (line.decodeIndicator) {
      if (line.blockSync && !memcmp(payload, "\x01\x00\x00\x03\x30", 5) && payload[8] < BLOCKS) {
        uint16_t id = payload[5] << 8 | payload[6];
        if (!pageOf[id] && pages < MAX_PAGES) {
          pageId[pages] = id;
          pageOf[id] = ++pages;
        }
        if (pageOf[id]) {
          dirPage[payload[8]]  = pageOf[id] - 1;
          dirWords[payload[8]] = (payload[10] << 8 | payload[11]) / 2;
          dirTime[payload[8]]  = line.timestamp;
        }
      }
      if (!line.blockSync && clockSeconds(payload) >= 0) {
        clock     = clockSeconds(payload);
        clockTime = line.timestamp;
      }
      continue;
    }
    if (clock < 0) continue;  // nothing can be dated before the first clock line

    int row;
    if (line.blockSync) {
      if (line.blockId >= BLOCKS || line.blockSubId > 1) {
        block = -1;
        continue;
      }
      block = line.blockId;
      if (!dirTime[block] || line.timestamp - dirTime[block] > DIR_TTL_US) {
        block = -1;  // directory line lost: we do not know what this block is
        continue;
      }
      page       = dirPage[block];
      syncRow    = line.blockSubId ? SUB1_ROW : 0;
      syncLineId = line.lineId;
      syncTime   = line.timestamp;
      row        = syncRow;
    } else {
      if (block < 0) continue;
      // lineId places the line even when neighbours were lost to CRC errors
      row = syncRow + ((line.lineId - syncLineId) & 0x7F);
      if (row >= ROWS || row == SUB1_ROW || line.timestamp - syncTime > MAX_GAP_US) {
        block = -1;
        continue;
      }
    }

    int first = firstWord(row);
    int count = firstWord(row + 1) - first;
    if (first + count > dirWords[block]) count = dirWords[block] - first;  // padding after the page
    if (count <= 0) continue;
    uint8_t* data = payload + (line.blockSync ? 2 : 0);

    for (int i = 0; i < count; i++) {
      word[page][first + i]   = data[2 * i] | data[2 * i + 1] << 8;
      seenAt[page][first + i] = line.timestamp;
    }

    // Print every slot touched by this line, if complete and changed
    for (int slot = first / 2; slot <= (first + count - 1) / 2; slot++) {
      int a = 2 * slot, b = a + 1;
      uint64_t ta = seenAt[page][a], tb = seenAt[page][b];
      // a slot can straddle two lines: both halves must come from the same pass
      if (!ta || !tb || (ta > tb ? ta - tb : tb - ta) > MAX_GAP_US) continue;
      if (wantPage >= 0) {
        // print once per reception: when the line holding the slot's second word arrives
        if (pageId[page] != wantPage || slot != wantSlot || b < first) continue;
      } else if (word[page][a] == printed[page][a] && word[page][b] == printed[page][b]) continue;

      printed[page][a] = word[page][a];
      printed[page][b] = word[page][b];

      char sa[32], sb[32];
      int now = (clock + (int)((line.timestamp - clockTime) / 1000000)) % 86400;
      sprintWord(sa, word[page][a], now);
      sprintWord(sb, word[page][b], now);
      printf("%02d:%02d:%02d  p%04X s%02d  %s %s\n",
             now / 3600, now / 60 % 60, now % 60, pageId[page], slot, sa, sb);
    }
  }

  fclose(fptr);
  return 0;
}
