include config.mk

MD_SRC = madasul.c
MD_OBJ = ${MD_SRC:.c=.o}
MC_SRC = saparoton.c ansi.c
MC_OBJ = ${MC_SRC:.c=.o}

all: options madasul #saparoton

options:
	@echo build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${MD_OBJ}: config.mk
${MC_OBJ}: config.mk

madasul: ${MD_OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${MD_OBJ} ${LDFLAGS}
	@echo

saparoton: ${MC_OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${MC_OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f madasul saparoton ${MD_OBJ} ${MC_OBJ}

install:
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	#@cp -f madasul angl mcp mlg ${DESTDIR}${PREFIX}/bin
	@cp -f madasul mcp mlg ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasul
	#@chmod 755 ${DESTDIR}${PREFIX}/bin/saparoton
	@chmod 755 ${DESTDIR}${PREFIX}/bin/mcp
	@chmod 755 ${DESTDIR}${PREFIX}/bin/mlg
	@sed "s#MADASULSRC#${PREFIX}/share/madasul/src/#g" < madasulm > ${DESTDIR}${PREFIX}/bin/madasulm
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasulm
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${PREFIX}/man/man1
	@sed "s/VERSION/${VERSION}/g" < madasul.1 > ${DESTDIR}${MANPREFIX}/man1/madasul.1
	#@sed "s/VERSION/${VERSION}/g" < saparoton.1 > ${DESTDIR}${MANPREFIX}/man1/saparoton.1
	@echo installing src files to ${DESTDIR}${PREFIX}/share/madasul
	@mkdir -p ${DESTDIR}${PREFIX}/share/madasul/src
	#@cp -Rf madasul.c config.def.h config.mk Makefile saparoton.c ansi.c ansi.h ${DESTDIR}${PREFIX}/share/madasul/src
	@cp -Rf madasul.c config.def.h config.mk Makefile ${DESTDIR}${PREFIX}/share/madasul/src

uninstall:
	rm ${DESTDIR}${PREFIX}/bin/madasul
	rm ${DESTDIR}${MANPREFIX}/man1/madasul.1
	rm ${DESTDIR}${PREFIX}/bin/saparoton
	rm ${DESTDIR}${MANPREFIX}/man1/saparoton.1
	rm ${DESTDIR}${PREFIX}/bin/madasulm
	rm ${DESTDIR}${PREFIX}/bin/mcp
	rm ${DESTDIR}${PREFIX}/bin/mlg
