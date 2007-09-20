CFLAGS=-g -Wall -pipe 
LDFLAGS=

OBJECTS=tlaparser.o dumpdata.o lists.o parser_regexp.o

HEADERS=*.h

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

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

tlaparser: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) tlaparser
