# madasuk version
VERSION = "666.0.2"

# paths
PREFIX = /home/mark/bin
MANPREFIX = ${PREFIX}/man

# dbus/notify adds ~250k mem usage
#NOTIFY_INCS = `pkg-config --cflags dbus-1`
#NOTIFY_LIBS = `pkg-config --libs dbus-1`
#NOTIFY_FLAGS = -DUSE_NOTIFY
#NOTIFY_CFILES = notify.c

INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc -lpthread

CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = -g -std=c99 -pedantic -Wall -Os -D_REENTRANT ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
