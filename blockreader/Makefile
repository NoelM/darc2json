CC=gcc
CFLAGS=-I.
DEPS = utils.o line.h
OBJ = blockreader.o utils.o line.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

blockreader: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean
clean:
	rm *.o
