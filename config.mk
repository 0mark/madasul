# madasuk version
VERSION = "666.0.2"

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man

INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc -lpthread

CPPFLAGS = -DVERSION=\"${VERSION}\"
#CFLAGS = -g -std=c99 -pedantic -Wall -Os -D_REENTRANT ${INCS} ${CPPFLAGS}
CFLAGS = -std=c99 -pedantic -Wall -Os -D_REENTRANT ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
