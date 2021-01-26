CC=gcc
CFLAGS=-Wall
DEPS=iamroot.h pair_protocol.h
OBJ=iamroot.o pair_protocol.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

iamroot: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm *.o iamroot
