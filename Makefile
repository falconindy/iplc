# iplc - parseable network interface output

CC ?= cc
CFLAGS += -std=c99 -g -Wall -Wextra -pedantic

SRC = iplc.c
OBJ = ${SRC:.c=.o}

all: iplc

.c.o:
	${CC} -c ${CFLAGS} $<

iplc: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install: iplc
	install -D -m755 iplc ${DESTDIR}${PREFIX}/bin/iplc

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/iplc

strip: iplc
	strip --strip-all iplc

clean:
	rm -f *.o iplc

.PHONY: all clean install strip

