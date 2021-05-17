# Copyright 2019 ~ 2021 YuutaW Minecraft, All Rights Reserved.
# Proprietary and confidential.
# Unauthorized copying of any parts of this file, via any medium is strictly prohibited.
# Written by Yuuta Liang <yuuta@yuuta.moe>, April 2021.

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
