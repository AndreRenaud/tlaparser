#include <stdio.h>
#include <stdlib.h>

#include "dumpdata.h"
#include "common.h"

struct pin_assignments {
    int init;

    channel_info *clk;
    channel_info *par;
    channel_info *nframe;
    channel_info *ntrdy;
    channel_info *nirdy;
    channel_info *ndevsel;
    channel_info *pci_ad[32];
    channel_info *pci_cbe[4];
};

static struct pin_assignments pa = {0};

/* States */
enum {
    PCI_IDLE		= -1,
    PCI_IO_READ		= 0x2,
    PCI_IO_WRITE	= 0x3,
    PCI_READ		= 0x6,
    PCI_WRITE		= 0x7,
    PCI_CONFIG_READ	= 0xa,
    PCI_CONFIG_WRITE	= 0xb,
};

/* Globals since its a state machine */
static int state = PCI_IDLE, clock, prev_clock = 1, frame, prev_frame = 1, 
    command, data, address, irdy, trdy;
static unsigned int buf_idx, buffer[1024];
static capture *capture_init = NULL;

static void pci_update_on_clock(capture *c)
{
    prev_frame = frame;
}

static void pci_capture_on_clock(capture *c)
{
    frame = capture_bit(c, pa.nframe);
    command = capture_data(c, pa.pci_cbe, 4);
    data = capture_data(c, pa.pci_ad, 32);
    irdy = capture_bit(c, pa.nirdy);
    trdy = capture_bit(c, pa.ntrdy);
}

static void pci_dump_buffer(void)
{
    const char *command_name[] = {
	"Interrupt acknowledge", "Special cycle", "IO Read", "IO Write", "Reserved",
	"Reserved", "Read", "Write", "Reserved", "Reserved", "Config Read", "Config Write",
	"Read Multi", "Dual address cycle", "Read line", "Write and invalidate"
    };
    int i;

    printf("%s at addr 0x%.8x of length %d\n", command_name[state], address, buf_idx);
    
    for(i = 0; i < buf_idx; i++) 
	printf("\t0x%.8x\n", buffer[i]);
}

/*
 * Bulk of the work is done here
 */
static void parse_pci_capture(capture *c, list_t *channels, int last)
{
    static int count = 0;

    clock = capture_bit(c, pa.clk);
    if(!capture_init || clock == prev_clock || prev_clock == 1) {
	/* Clock either hasn't changed, is on its first pulse, or is falling */
	capture_init = c;
	prev_clock = clock;
	return;
    }
    capture_init = c;
    prev_clock = clock;
	
    /* We are on the rising edge of the clock */
    pci_capture_on_clock(c);
    
    switch(state) {
    case PCI_IDLE:
	if(prev_frame == 1 && frame == 0) {
	    /* Frame has been pulled low. Start of transaction */

#if 0
	    /* DEBUG: Limit the number of transactions shown */
	    count++;
	    if(count > 20) 
		return;
#endif
	    
	    switch(command) {
	    case 0x0:
		/* Interrupt acknowledge */
		break;
		
	    case PCI_READ:
	    case PCI_WRITE:
#if 0
	    case PCI_IO_READ:
	    case PCI_IO_WRITE:
	    case PCI_CONFIG_READ:
	    case PCI_CONFIG_WRITE:
#endif
		state = command;
		buf_idx = 0;
		address = data;
		break;
				
	    default:
		printf("Unhandled command 0x%x\n", command);
		break;
	    }
	    
	    //printf("PCI Transaction: Command = 0x%x, Address = 0x%.8x\n", command, address);	
	}
	break;

    case PCI_READ:
    case PCI_WRITE:
	if(irdy == 0 && trdy == 0) {
	    /* Valid data on the line */
	    buffer[buf_idx++] = data;

	    if(frame == 1) {
		/* Transaction finished */
		pci_dump_buffer();
		state = PCI_IDLE;
	    }
	}
	break;

    default:
	printf("Warning: Unknown state\n");
	break;
    }
	
    pci_update_on_clock(c);
}

static void parse_pci_bulk_capture(bulk_capture * b, list_t * channels)
{
    int i;
    capture *c;

    c = b->data;

    /* Initialise the channels */
    if(c && pa.init == 0) {
	char buffer[32];
	
	pa.init = 1;
	pa.clk = capture_channel_details(c, "CLK", channels);
	pa.par = capture_channel_details(c, "PAR", channels);
	pa.nframe = capture_channel_details(c, "nFRAME", channels);
	pa.ntrdy = capture_channel_details(c, "nTRDY", channels);
	pa.nirdy = capture_channel_details(c, "nIRDY", channels);
	pa.ndevsel = capture_channel_details(c, "nDEVSEL", channels);

	for(i = 0; i < 32; i++) {
	    sprintf(buffer, "AD%d", i);
	    pa.pci_ad[i] = capture_channel_details(c, buffer, channels);
	}
	
	for(i = 0; i < 4; i++) {
	    sprintf(buffer, "nCBE%d", i);
	    pa.pci_cbe[i] = capture_channel_details(c, buffer, channels);
	}
    }
    
    for (i = 0; i < b->length / sizeof(capture); i++) {
	parse_pci_capture(c, channels,
			  i == (b->length / sizeof(capture)) - 1);
	c++;
    }
}

void parse_pci(list_t * cap, char *filename, list_t * channels)
{
    list_t *n;
    int i;

    printf("PCI analysis of file: '%s'\n", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++) {
	printf("Parsing capture block %d\n", i);
	parse_pci_bulk_capture(n->data, channels);
    }
}
