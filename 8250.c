#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "dumpdata.h"
#include "common.h"

static int address_offset = 2; // how does our x_ma map to a[0-2] on the 8250

static int get_data (capture *c, list_t *channels)
{
    char name[10];
    int i;
    int retval = 0;
    int bit;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "x_md%d", i);
	bit = capture_bit_name (c, name, channels) ? 1 : 0;
	retval |= bit << i;
    }

    return retval;
}

static int get_addr (capture *c, list_t *channels)
{
    char name[10];
    int i;
    int retval = 0;
    int bit;

    for (i = 0; i < 3; i++)
    {
	sprintf (name, "x_ma%d", i + address_offset);
	bit = capture_bit_name (c, name, channels) ? 1 : 0;
	retval |= bit << i;
    }

    return retval;

}

static const char *get_addr_name (int addr, int read)
{
    if (read)
	switch (addr)
	{
	    case 0: return "RHR";
	    case 1: return "IER";
	    case 2: return "IIR";
	    case 3: return "LCR";
	    case 4: return "MCR";
	    case 5: return "LSR";
	    case 6: return "MSR";
	    case 7: return "SPR";
	}
    else
	switch (addr)
	{
	    case 0: return "THR";
	    case 1: return "IER";
	    case 2: return "FCR";
	    case 3: return "LCR";
	    case 4: return "MCR";
	    case 7: return "SPR";
	}

    return "unknown";
}



struct pin_assignments
{
    int init;

    channel_info *cs;
    channel_info *irq;
    channel_info *read;
    channel_info *write;
};

static void parse_8250_cap (capture *c, list_t *channels)
{
    static capture *prev = NULL;
    static struct pin_assignments pa = {-1};

    if (pa.init == -1 && c) // work these out once only, to speed things up
    {
	pa.init = 1;
	pa.irq = capture_channel_details ("quart0_int", channels);
	pa.cs = capture_channel_details ("quart0_cs_n", channels);
	pa.write = capture_channel_details ("x_wrn_1", channels);
	pa.read = capture_channel_details ("x_rdn_1", channels);
    }

    if (!prev) // skip first sample
    {
	prev = c;
	return;
    }

#if 0
    if (capture_bit (c, pa.cs) != capture_bit (prev, pa.cs))
	time_log (c, "cs changed: %d\n", capture_bit (c, pa.cs));
#endif


    if (capture_bit (c, pa.irq) != capture_bit (prev, pa.irq))
	time_log (c, "Interrupt %sasserted\n", capture_bit (c, pa.irq) ? "" : "de");

#if 1
    int read = 0;
    int write = 0;
    if (!capture_bit (c, pa.write) && capture_bit_transition (c, prev, pa.cs, TRANSITION_rising_edge))
	write = 1;
    if (!capture_bit (c, pa.read) && capture_bit_transition (c, prev, pa.cs, TRANSITION_rising_edge))
	read = 1;

    if (write || read)
    {
	int data = get_data (c, channels);
	int address = get_addr (c, channels);
	const char *name = get_addr_name (address, read);

	time_log (c, "%s 0x%x (%s) of 0x%x\n", write ? "write to" : "read from", address, name, data);
    }
#else
    if (!capture_bit (c, pa.cs)) // we're selected
    {
	int write = 0;
	int read = 0;
	if (capture_bit_transition (c, prev, pa.write, TRANSITION_rising_edge))
	    write = 1;
	if (capture_bit_transition (c, prev, pa.read, TRANSITION_rising_edge))
	    read = 1;

	if (write || read)
	{
	    int data = get_data (c, channels);
	    int address = get_addr (c, channels);

	    time_log (c, "%s of 0x%x to 0x%x\n", write ? "write" : "read", data, address);
	}
    }
#endif


    prev = c;
}

void parse_8250 (bulk_capture *b, char *filename, list_t *channels)
{
    int i;
    capture *c;

    printf ("8250 analysis of file: '%s'\n", filename);

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_8250_cap (c, channels);
	c++;
    }
}
