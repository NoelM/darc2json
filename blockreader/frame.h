#ifndef BR_FRAME_H_
#define BR_FRAME_H_

class Frame {
public:
    Frame(uint64_t rowLen);
    ~Frame();
    AddRow(char* row);

private:
    uint64_t rowLen;
    bool     sync;
    char*    prevRow;

    uint16_t missing;
    uint64_t size;

    char**  header;
    char**  block;
    char**  padding;
};

#endif