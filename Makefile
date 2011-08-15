include config.mk

MD_SRC = madasul.c
MD_OBJ = ${MD_SRC:.c=.o}
MC_SRC = mcp.c ansi.c
MC_OBJ = ${MC_SRC:.c=.o}

all: options madasul mcp

options:
	@echo build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo

.c.o:
	@echo CC $<
	@#@echo "AAA" @${CC} -c ${CFLAGS} $<
	@${CC} -c ${CFLAGS} $<

#${OBJ}: config.mk

#config.h:
#	@echo creating $@ from config.def.h
#	@cp config.def.h $@

madasul: ${MD_OBJ}
	@echo CC -o $@
	@#echo "BBB" @${CC} -o $@ ${MD_OBJ} ${LDFLAGS}
	@${CC} -o $@ ${MD_OBJ} ${LDFLAGS}
	@echo

mcp: ${MC_OBJ}
	@echo CC -o $@
	@#echo "BBB" @${CC} -o $@ ${MC_OBJ} ${LDFLAGS}
	@${CC} -o $@ ${MC_OBJ} ${LDFLAGS}

#mcp: ${OBJ}
#	@echo CC -o $@
#	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f madasul mcp ${MD_OBJ} ${MC_OBJ}

install:
	cp madasul mcp ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/madasul
	rm ${PREFIX}/bin/mcp
