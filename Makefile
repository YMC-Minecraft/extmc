CFLAGS= \
	-I.\
       -std=c99 \
       -Wall \
       -D_POSIX_C_SOURCE=200809L \
       

LDFLAGS= \
	 -ldl \
	 -lpthread \


OBJ=main.o thpool.o mcin.o plugins.o rcon_host.o rcon.o net.o plugin_registry.o threads_util.o md5.o

BIN=extmc

debug: CFLAGS += -DCONTROL_SOCKET_PATH="\"./extmc.ctl\"" -g3 -O0 -rdynamic
debug: $(BIN)

release: CFLAGS += -DDISABLE_DEBUG
release: $(BIN)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) *~ *.o $(BIN)

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/
	ln -s $(DESTDIR)$(PREFIX)/bin/$(BIN) $(DESTDIR)$(PREFIX)/bin/extmcctl
