include config.mk

SRC = madasuld.c
OBJ = ${SRC:.c=.o}

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

${OBJ}: config.mk
${OBJ}: config.mk

madasuld: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}
	@echo

clean:
	@echo cleaning
	@rm -f madasuld ${OBJ} ${OBJ}

install:
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f madasuld ${DESTDIR}${PREFIX}/bin
	@sed "s#PREFIX#${PREFIX}#g" < madasul > ${DESTDIR}${PREFIX}/bin/madasul
	@sed "s#PREFIX#${PREFIX}#g" < madasulc > ${DESTDIR}${PREFIX}/bin/madasulc
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasuld
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasul
	@chmod 755 ${DESTDIR}${PREFIX}/bin/madasulc
	@mkdir -p ${DESTDIR}${PREFIX}/lib
	@cp -f madasul-bashstuff ${DESTDIR}${PREFIX}/lib
	#@chmod 755 ${DESTDIR}${PREFIX}/bin/madasul-bashstuff
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${PREFIX}/man/man1
	@sed "s/VERSION/${VERSION}/g" < madasuld.1 > ${DESTDIR}${MANPREFIX}/man1/madasuld.1
	@sed "s/VERSION/${VERSION}/g" < madasul.1 > ${DESTDIR}${MANPREFIX}/man1/madasul.1
	@sed "s/VERSION/${VERSION}/g" < madasulc.1 > ${DESTDIR}${MANPREFIX}/man1/madasulc.1
	@echo installing examples to ${DESTDIR}${PREFIX}/share/madasul/examples
	@mkdir -p ${DESTDIR}${PREFIX}/share/madasul/examples
	@for i in $$(ls examples); do sed "s#PREFIX#${PREFIX}#g" < examples/$$i > ${DESTDIR}${PREFIX}/share/madasul/examples/$$i; done

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
