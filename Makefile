CFLAGS=-g -Wall -pipe 
LDFLAGS=
BISON=bison
FLEX=flex

OBJECTS=tlaparser.o dumpdata.o lists.o

# Bison/Flex one - don't use, crappy
#OBJECTS+=parser.o lexer.o 
# Regexp one - nice & fast & simple
OBJECTS+=parser_regexp.o

HEADERS=*.h
GENERATED=parser.c parser.h lexer.c parser.output

CFLAGS+=-DPARSE_PCI
OBJECTS+=pci.o

CFLAGS+=-DPARSE_SCSI
OBJECTS+=scsi.o

CFLAGS+=-DPARSE_XD
OBJECTS+=xd.o

CFLAGS+=-DPARSE_PERTEC
OBJECTS+=pertec.o

CFLAGS+=-DPARSE_8250
OBJECTS+=8250.o

CFLAGS+=-DPARSE_61K
OBJECTS+=61k.o

CFLAGS+=-DPARSE_KENNEDY
OBJECTS+=kennedy.o

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
