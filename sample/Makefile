CFLAGS= \
	-I.\
       -g \
       -std=c99 \
       -Wall \
       -D_POSIX_C_SOURCE=200809L \
       

LDFLAGS= \

OBJ=main.o

BIN=libsample.so

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) -fpic

$(BIN): $(OBJ)
	$(CC) -shared -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) *~ *.o $(BIN)
