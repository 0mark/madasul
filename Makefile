include config.mk

MD_SRC = madasul.c
MD_OBJ = ${MD_SRC:.c=.o}
MC_SRC = angl.c ansi.c
MC_OBJ = ${MC_SRC:.c=.o}

all: options madasul angl

options:
	@echo build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${MD_OBJ}: config.h config.mk
${MC_OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

madasul: ${MD_OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${MD_OBJ} ${LDFLAGS}
	@echo

angl: ${MC_OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${MC_OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f madasul angl ${MD_OBJ} ${MC_OBJ}

install:
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f madasul angl mcp mlg ${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasul
	@chmod 755 ${DESTDIR}${PREFIX}/bin/angl
	@chmod 755 ${DESTDIR}${PREFIX}/bin/mcp
	@chmod 755 ${DESTDIR}${PREFIX}/bin/mlg
	@sed "s#MADASULSRC#${PREFIX}/share/madasul/src/#g" < madasulm > ${DESTDIR}${PREFIX}/bin/madasulm
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasulm
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < madasul.1 > ${DESTDIR}${MANPREFIX}/man1/madasul.1
	@sed "s/VERSION/${VERSION}/g" < angl.1 > ${DESTDIR}${MANPREFIX}/man1/angl.1
	@echo installing src files to ${DESTDIR}${PREFIX}/share/madasul
	@mkdir -p ${DESTDIR}${PREFIX}/share/madasul/src
	@cp -Rf madasul.c config.def.h config.mk Makefile angl.c ansi.c ansi.h ${DESTDIR}${PREFIX}/share/madasul/src

uninstall:
	rm ${PREFIX}/bin/madasul
	rm ${DESTDIR}${MANPREFIX}/man1/madasul.1
	rm ${PREFIX}/bin/angl
	rm ${DESTDIR}${MANPREFIX}/man1/angl.1
	rm ${PREFIX}/bin/madasulm
	rm ${PREFIX}/bin/mcp
	rm ${PREFIX}/bin/mlg
