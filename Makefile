CFLAGS=-g -Wall -pipe 
LDFLAGS=
BISON=bison
FLEX=flex

OBJECTS=tlaparser.o parser.o lexer.o dumpdata.o lists.o
HEADERS=*.h
GENERATED=parser.c parser.h lexer.c parser.output

CFLAGS+=-DPARSE_SCSI
OBJECTS+=scsi.o

CFLAGS+=-DPARSE_XD
OBJECTS+=xd.o

default: tlaparser

tlaparser.c: parser.c

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)
%.c: %.y $(HEADERS)
	$(BISON) -v -d -o $@ $<

lexer.c: lexer.l parser.c
	$(FLEX) -o$@ $<

tlaparser: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) tlaparser $(GENERATED)
