#include <stdio.h>

#include "dumpdata.h"
#include "common.h"

static unsigned int get_ftrd (capture *c, list_t *channels)
{
    char name[200];
    unsigned int retval = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "FTRD%d", i);
	retval |= capture_bit_name (c, name, channels) ? 0 : 1 << i;
    }
#warning "Ignoring parity"

    return retval;
}

static unsigned int get_cis (capture *c, list_t *channels)
{
    char name[200];
    unsigned int retval = 0;
    int i;

    for (i = 0; i < 13; i++)
    {
	sprintf (name, "CIS%d", i);
	retval |= capture_bit_name (c, name, channels) ? 0 : 1 << i;
    }

    return retval;
}

static int decode_command (int command, char *buffer)
{
    int written = 0;
    const char *density[] = {"???", "1600/200", "556", "800"};
    const char *commands56[] = {"", "Read", "Write", "Search Forward File"};
    const char *commands7[] = {"", "Write EOF"};
    const char *commands89[] = {"", "Backspace", "Rewind", "Backspace File"};
    const char *commands10[] = {"", "Erase"};
    const char *commands12[] = {"", "Search Forward"};

    written += sprintf (buffer + written, "Trans: %d ", command & 0x3);
    //written += sprintf (buffer + written, "Density: %s ", density[(command >> 2) & 0x3]);
    //written += sprintf (buffer + written, "Parity: %s ", command & 1 << 4 ? "Event" : "Odd");

    written += sprintf (buffer + written, " %s", commands56[(command >> 5) & 3]);
    written += sprintf (buffer + written, " %s", commands7[(command >> 7) & 1]);
    written += sprintf (buffer + written, " %s", commands89[(command >> 8) & 3]);
    written += sprintf (buffer + written, " %s", commands10[(command >> 10) & 1]);
    written += sprintf (buffer + written, " %s", commands12[(command >> 12) & 1]);

    return written;
}

struct pin_assignments
{
    int init;

    channel_info *fwclk;
    channel_info *cccom;
    channel_info *cdavl;
    channel_info *frclk;
    channel_info *ffbusy;
};

static void parse_kennedy_cap (capture *c, list_t *channels)
{
    static capture *prev = NULL;
    static struct pin_assignments pa = {-1};
    static unsigned char buffer[100000];
    static int buffer_pos = 0;
    static unsigned int is_writing = 0;

    if (pa.init == -1)
    {
	pa.init = 1;
	pa.fwclk = capture_channel_details (c, "fwclk", channels);
	pa.cccom = capture_channel_details (c, "cccom", channels);
	pa.cdavl = capture_channel_details (c, "cdavl", channels);
	pa.frclk = capture_channel_details (c, "frclk", channels);
	pa.ffbusy = capture_channel_details (c, "ffbusy", channels);
    }

    if (!prev) // need it to detect edges
	goto done;

    if (capture_bit_transition (c, prev, pa.fwclk, TRANSITION_low_to_high))
    {
	if (!is_writing)
	    time_log (c, "ARGH: FWCLK transition, but not writing\n");
	if (capture_bit (c, pa.cdavl))
	{
	    display_data_buffer (buffer, buffer_pos, 0);
	    buffer_pos = 0;
	}
	else
	{
	    int data = (~get_cis (c, channels)) & 0xff;
	    //time_log (c, "Got data: 0x%x\n", data);
	    buffer[buffer_pos++] = data;
	}
    }

    if (!is_writing && capture_bit_transition (c, prev, pa.frclk, TRANSITION_low_to_high))
    {
	int data;
	data = get_ftrd (c, channels) & 0xff;
	buffer[buffer_pos++] = data;
    }

    if (!is_writing && buffer_pos > 0 && capture_bit (c, pa.ffbusy))
    {
	display_data_buffer (buffer, buffer_pos, 0);
	buffer_pos = 0;
    }

    if (capture_bit_transition (c, prev, pa.cccom, TRANSITION_low_to_high))
    {
	unsigned int cmd = get_cis (c, channels);
	char command_string[200];
	int type = (cmd & (0x3 << 5)) >> 5;
	decode_command (cmd, command_string);
	time_log (c, "CMD: 0x%x: %s\n", cmd, command_string);

	if (buffer_pos)
	{
	    time_log (c, "ARGH: Got command, but outstanding data\n");
	    display_data_buffer (buffer, buffer_pos, 0);
	}


	//time_log (c, "TYPE: %d\n", type);
	if (type == 2)
	    is_writing = 1;
	else
	    is_writing = 0;
	
    }

    if (buffer_pos >= sizeof (buffer))
    {
	time_log (c, "ARGH: BUFFER TOO LARGE\n");
	display_data_buffer (buffer, buffer_pos, 0);
	buffer_pos = 0;
    }

done:
    prev = c;
}

static void parse_kennedy_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c;

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_kennedy_cap (c, channels);
	c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));
}

void parse_kennedy (list_t *cap, char *filename, list_t *channels)
{
    list_t *n;
    int i;

    printf ("Kennedy analysis of file: '%s'\n", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_kennedy_bulk_cap (n->data, channels);
    }
}
