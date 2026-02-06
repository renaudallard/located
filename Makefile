CC       = gcc
CFLAGS   = -Wall -Wextra -pedantic -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Ivendor
LDFLAGS  = -lmicrohttpd -lsqlite3 -lpthread

PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/man

SRCS     = src/main.c src/config.c src/device_db.c src/http_server.c src/routes.c vendor/cJSON.c
OBJS     = $(SRCS:.c=.o)
BIN      = located

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 located.1 $(DESTDIR)$(MANDIR)/man1/located.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -f $(DESTDIR)$(MANDIR)/man1/located.1

.PHONY: all clean install uninstall
