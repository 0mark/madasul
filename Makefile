include config.mk

MD_SRC = madasuld.c
MD_OBJ = ${MD_SRC:.c=.o}

all: options madasuld #angl

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

madasuld: ${MD_OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${MD_OBJ} ${LDFLAGS}
	@echo

clean:
	@echo cleaning
	@rm -f madasuld ${MD_OBJ} ${MC_OBJ}

install:
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f madasuld madasul madasulc madasul-bashstuff.sh ${DESTDIR}${PREFIX}/bin
	#@sed "s#MADASULSRC#${PREFIX}/share/madasul/src/#g" < madasulm > ${DESTDIR}${PREFIX}/bin/madasulm
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasuld
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasul
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasulc
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasul-bashstuff.sh
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${PREFIX}/man/man1
	@sed "s/VERSION/${VERSION}/g" < madasuld.1 > ${DESTDIR}${MANPREFIX}/man1/madasuld.1
	@sed "s/VERSION/${VERSION}/g" < madasul.1 > ${DESTDIR}${MANPREFIX}/man1/madasul.1
	@sed "s/VERSION/${VERSION}/g" < madasulc.1 > ${DESTDIR}${MANPREFIX}/man1/madasulc.1
	@echo installing examples to ${DESTDIR}${PREFIX}/share/madasul/examples
	@mkdir -p ${DESTDIR}${PREFIX}/share/madasul/examples
	@cp -Rf examples/* ${DESTDIR}${PREFIX}/share/madasul/examples

uninstall:
	rm ${DESTDIR}${PREFIX}/bin/madasuld
	rm ${DESTDIR}${PREFIX}/bin/madasul
	rm ${DESTDIR}${PREFIX}/bin/madasulc
	rm ${DESTDIR}${PREFIX}/bin/madasul-bashstuff.sh
	rm ${DESTDIR}${MANPREFIX}/man1/madasuld.1
	rm ${DESTDIR}${MANPREFIX}/man1/madasul.1
	rm ${DESTDIR}${MANPREFIX}/man1/madasulc.1
	rm ${DESTDIR}${PREFIX}/share/madasul/examples/*
	rmdir ${DESTDIR}${PREFIX}/share/madasul/examples
	rmdir ${DESTDIR}${PREFIX}/share/madasul
