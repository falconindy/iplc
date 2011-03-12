# iplc - parseable network interface output

CC ?= cc
CFLAGS += -std=c99 -Wall -Wextra -pedantic

SRC = iplc.c
OBJ = ${SRC:.c=.o}

all: iplc

.c.o:
	${CC} -c ${CFLAGS} $<

ipl: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install: ipl ipl.1
	install -D -m755 iplc ${DESTDIR}${PREFIX}/bin/iplc

strip: iplc
	strip --strip-all iplc

clean:
	rm -f *.o iplc

.PHONY: all clean install strip

