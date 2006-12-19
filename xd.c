#include <stdio.h>

#include "dumpdata.h"
#include "common.h"

static uint8_t printable_char (int data)
{
    if (data >= ' ' && data <= 126)
	return data;
    return '.';
}

static uint8_t xd_data (capture *c, list_t *channels)
{
    uint8_t retval = 0;
    retval |= (capture_bit_name (c, "d0", channels) ? 1 : 0) << 0;
    retval |= (capture_bit_name (c, "d1", channels) ? 1 : 0) << 1;
    retval |= (capture_bit_name (c, "d2", channels) ? 1 : 0) << 2;
    retval |= (capture_bit_name (c, "d3", channels) ? 1 : 0) << 3;
    retval |= (capture_bit_name (c, "d4", channels) ? 1 : 0) << 4;
    retval |= (capture_bit_name (c, "d5", channels) ? 1 : 0) << 5;
    retval |= (capture_bit_name (c, "d6", channels) ? 1 : 0) << 6;
    retval |= (capture_bit_name (c, "d7", channels) ? 1 : 0) << 7;

    //printf ("data: 0x%x\n", retval);
    return retval;
}

static char *nand_command (unsigned int command)
{
    switch (command)
    {
	case 0x80:
	    return "Serial Data Input";
	case 0x00:
	    return "Read (1)";
	case 0x01:
	    return "Read (2)";
	case 0x50:
	    return "Read (3)";
	case 0xFF:
	    return "Reset";
	case 0x10:
	    return "True Page Program";
	case 0x11:
	    return "Dummy Page Program for Multi Block Programming";
	case 0x15:
	    return "Multi Block Program";
	case 0x60:
	    return "Block Erase (step 1)";
	case 0xD0:
	    return "Block Erase (step 2)";
	case 0x70:
	    return "Status Read (1)";
	case 0x71:
	    return "Status Read (2)";
	case 0x90:
	    return "ID Read (1)";
	case 0x91:
	    return "ID Read (2)";
	case 0x9A:
	    return "ID Read (3)";
	default:
	    return "Unknown";
    }
}

static void dump_data_buffer (unsigned char *buffer, unsigned int len)
{
    int i;

    printf ("\tData: ");
    for (i = 0; i < len; i++)
    {
	if (i % 32 == 0)
	    printf ("\n\t  ");
	printf ("%2.2x ", buffer[i]);
    }
    printf ("\n\tLen=%d\n", len);
}

static void parse_xd_cap (capture *c, capture *prev, list_t *channels)
{
    static unsigned int cle_mode;
    static unsigned int ale_mode;
    static unsigned int cur_data = 0;
    static unsigned int cur_data_len = 0;
    static unsigned char data_buffer[1024];
    static unsigned int data_len = 0;

    if (!prev) // need it to detect edges
	return;

    if (!capture_bit_name (c, "nce", channels)) // we're accessing xD
    {
#if 0
	printf ("nce: %d %d %d\n", 
		capture_bit_name (c, "ale", channels),
		capture_bit_name (c, "cle", channels),
		capture_bit_name (c, "nwe", channels));
#endif
	if (capture_bit_name (c, "ale", channels))
	{
	    //printf ("into ale mode\n");
	    ale_mode = 1;
	}
	else if (ale_mode)
	{
	    printf ("\tAddress: 0x%08x\n", cur_data);
	    cur_data = cur_data_len = ale_mode = 0;
	}

	if (capture_bit_name (c, "cle", channels))
	{
	    //printf ("into cle mode\n");
	    cle_mode = 1;
	    if (data_len)
	    {
		dump_data_buffer (data_buffer, data_len);
		data_len = 0;
	    }
	}
	else if (cle_mode)
	{
	    printf ("Command: %s (0x%x)\n", nand_command (cur_data), cur_data);
	    cur_data = cur_data_len = cle_mode = 0;
	}

	if (capture_bit_transition (c, prev, "nwe", channels, TRANSITION_low_to_high))
	{
	    uint8_t data = xd_data (c, channels);
	    //printf ("Write\n");
	    if (ale_mode || cle_mode)
	    {
		cur_data |= (data << cur_data_len);
		cur_data_len += 8;
	    }
	    else
	    {
		data_buffer[data_len] = data;
		data_len++;
	    }
	}

	if (capture_bit_transition (c, prev, "nre", channels, TRANSITION_low_to_high))
	{
	    uint8_t data = xd_data (c, channels);
	    //printf ("read\n");
	    if (ale_mode || cle_mode)
		printf ("WARNING: READ WHILE IN ALE/CLE\n");

	    data_buffer[data_len] = data;
	    data_len++;
	}
    }
    else if (data_len) // finished accessing, so dump our outstanding data
    {
	dump_data_buffer (data_buffer, data_len);
	data_len = 0;
    }
}

static void parse_xd_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c, *prev = NULL;

    c = (capture *)(b+1);//(char *)b+sizeof (bulk_capture);

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_xd_cap (c, prev, channels);
	prev = c;
	c++;
    }

    printf ("Parsed %d captures\n", b->length / sizeof (capture));
}

void parse_xd (list_t *cap, char *filename, list_t *channels)
{
    list_t *n;
    int i;

    printf ("xD analysis of file: '%s'\n", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_xd_bulk_cap (n->data, channels);
    }
}
