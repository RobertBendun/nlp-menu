# nlp-menu - dynamic menu with NLP techniques
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dmenu.c stest.c util.c
OBJ = $(SRC:.c=.o) engine.o

all: nlp-menu stest

.c.o:
	$(CC) -c $(CFLAGS) $<

%.o: %.cc lisp.cc unicode.cc
	$(CXX) -c $(CXXFLAGS) $<

config.h:
	cp config.def.h $@

$(OBJ): arg.h config.h config.mk drw.h

nlp-menu: dmenu.o drw.o util.o engine.o
	$(CXX) -o $@ dmenu.o drw.o util.o engine.o $(LDFLAGS)

stest: stest.o
	$(CC) -o $@ stest.o $(LDFLAGS)

clean:
	rm -f dmenu stest $(OBJ) dmenu-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f nlp-menu $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/nlp-menu
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < nlp-menu.1 > $(DESTDIR)$(MANPREFIX)/man1/nlp-menu.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nlp-menu.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/nlp-menu\
		$(DESTDIR)$(MANPREFIX)/man1/nlp-menu.1\

.PHONY: all clean dist install uninstall
