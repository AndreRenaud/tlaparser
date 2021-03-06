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

CFLAGS+=-DPARSE_SPI
OBJECTS+=spi.o

CFLAGS+=-DPARSE_FETEX
OBJECTS+=fetex.o

CFLAGS+=-DPARSE_OREO_FPGA_SPI
OBJECTS+=oreo_fpga_spi.o

CFLAGS+=-DPARSE_NOR
OBJECTS+=nor.o

CFLAGS+=-DPARSE_DM9000
OBJECTS+=dm9000.o

CFLAGS+=-DPARSE_CAMERA
OBJECTS+=camera_if.o

CFLAGS+=-DPARSE_SSC_AUDIO
OBJECTS+=ssc_audio.o

CFLAGS+=-DPARSE_OV3640
OBJECTS+=ov3640.o

CFLAGS+=-DPARSE_UNFORMATTED
OBJECTS+=cook_unformatted.o

CFLAGS+=-DPARSE_HALFFORMATTED
OBJECTS+=cook_half_formatted.o

default: tlaparser

%.o: %.c $(HEADERS)
	@echo "  CC $<"
	@$(CC) -c -o $@ $< $(CFLAGS)

tlaparser: $(OBJECTS)
	@echo "  LD $@"
	@$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) tlaparser
