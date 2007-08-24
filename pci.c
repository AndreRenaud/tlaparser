#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

    PCI_IRQ_ACK		= 0x0,
    PCI_IO_READ		= 0x2,
    PCI_IO_WRITE	= 0x3,
    PCI_READ		= 0x6,
    PCI_WRITE		= 0x7,
    PCI_CONFIG_READ	= 0xa,
    PCI_CONFIG_WRITE	= 0xb,
};

/* Globals since its a state machine */
static int state = PCI_IDLE;
static int clock, prev_clock;
static int frame, prev_clock_frame, prev_sample_frame;
static int data, prev_sample_data;
static int irdy, prev_sample_irdy, trdy, prev_sample_trdy;
static int command, prev_sample_command;
static int address;
static uint64_t command_time, prev_clock_time, clock_hold_time;


static unsigned int buf_idx, buffer[1024];
static capture *capture_init = NULL;

static void pci_update_on_clock(capture *c)
{
    prev_clock_frame = frame;
}

static void pci_update_on_sample(capture *c)
{
    prev_sample_frame = capture_bit(c, pa.nframe);
    prev_sample_irdy = capture_bit(c, pa.nirdy);
    prev_sample_irdy = capture_bit(c, pa.ntrdy);
    prev_sample_data = capture_data(c, pa.pci_ad, 32);
    prev_sample_command = capture_data(c, pa.pci_cbe, 4);
}

static void pci_capture_on_clock(capture *c)
{
    frame = capture_bit(c, pa.nframe);
    command = capture_data(c, pa.pci_cbe, 4);
    data = capture_data(c, pa.pci_ad, 32);
    irdy = capture_bit(c, pa.nirdy);
    trdy = capture_bit(c, pa.ntrdy);
}

static void pci_time_log(char *fmt, ...)
{
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsnprintf(buffer, 1024, fmt, args);
    va_end(args);
    
    printf("[%10.10lld] %s", command_time, buffer);
}

static void pci_dump_buffer(capture *c)
{
    const char *command_name[] = {
	"Interrupt acknowledge", "Special cycle", "IO Read", "IO Write", "Reserved",
	"Reserved", "Read", "Write", "Reserved", "Reserved", "Config Read", "Config Write",
	"Read Multi", "Dual address cycle", "Read line", "Write and invalidate"
    };
    int i;

    pci_time_log("%s at addr 0x%.8x of length %d\n", command_name[state], address, buf_idx); 

    for(i = 0; i < buf_idx; i++) 
	printf("\t0x%.8x\n", buffer[i]);
}

void pci_update_clock_hold(capture *c)
{
    /* 
     * If the clock hasn't changed, keep track of how long it hasn't changed for.
     * This is done because we are using transitional storage on the logic 
     * analyser, which means that rising edges for the clock may be incorrectly 
     * captured if no samples have happened immediately prior.
     */
    if(clock == prev_clock)
	clock_hold_time = capture_time(c) - prev_clock_time;
    else {
	clock_hold_time = 0;
	prev_clock_time = capture_time(c);
    }
    prev_clock = clock;
}

/*
 * Bulk of the work is done here.
 *
 * Because of the way the PCI bus works, we often save values from the previous
 * sample and use those instead of the value from the current sample. These values
 * are called prev_sample_x. Values save from the last rising clock edge are called
 * prev_clock_x.
 *
 */
#define CLOCK_CHANGE_THRESHOLD 30	/* nano-seconds */
static void parse_pci_capture(capture *c, list_t *channels, int last)
{
    clock = capture_bit(c, pa.clk);

    if(!capture_init || clock == prev_clock || prev_clock == 1) {
	/* Clock either hasn't changed, is on its first pulse, or is falling */
	capture_init = c;
	pci_update_clock_hold(c);
	pci_update_on_sample(c);
	return;
    }
	
    /* We are on a rising edge of the clock. Check that it is a real rising edge */
    clock_hold_time = capture_time(c) - prev_clock_time;
    if(clock_hold_time > CLOCK_CHANGE_THRESHOLD) {
	pci_update_on_sample(c);
	pci_update_clock_hold(c);
	return;
    }
    pci_update_clock_hold(c);
    capture_init = c;

    pci_capture_on_clock(c);    
    switch(state) {
    case PCI_IDLE:	
	if(!(prev_sample_frame == 1 && frame == 0) && 
	   ((prev_clock_frame == 1 && frame == 0) || prev_sample_frame == 0)) {
	    /* Frame has been pulled low. Start of transaction */	    
	    switch(prev_sample_command) {		
	    case PCI_IRQ_ACK:
	    case PCI_READ:
	    case PCI_WRITE:
	    case PCI_IO_READ:
	    case PCI_IO_WRITE:
		state = prev_sample_command;
		command_time = capture_time(c) - capture_time(first_capture);
		buf_idx = 0;
		address = prev_sample_data;
		break;
				
	    default:
		printf("Unhandled command 0x%x\n", command);
		break;
	    }
	    
	    //printf("PCI Transaction: Command = 0x%x, Address = 0x%.8x\n", command, address);	
	}
	break;

    case PCI_IRQ_ACK:
	if(prev_sample_irdy == 0 && prev_sample_trdy == 0 && frame == 1) {
	    /* Acknowledged */
	    pci_time_log("Interrupt acknowledged on vector 0x%x\n", data);
	    state = PCI_IDLE;
	}
	break;

    case PCI_READ:
    case PCI_WRITE:
    case PCI_IO_READ:
    case PCI_IO_WRITE:
	if(prev_sample_irdy == 0 && prev_sample_trdy == 0) {
	    /* Valid data on the line */
	    buffer[buf_idx++] = data;

	    if(frame == 1) {
		/* Transaction finished */
		pci_dump_buffer(c);
		state = PCI_IDLE;
	    }
	}
	break;

    default:
	printf("Warning: Unknown state\n");
	break;
    }
	
    pci_update_on_sample(c);
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
	
	first_capture = c;
	prev_clock_time = capture_time(c);
	clock_hold_time = 0;

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
