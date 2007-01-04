#include <stdio.h>

#include "dumpdata.h"
#include "common.h"
#include "pertec.h"

enum
{
    CMD_READ_FWD = 0x00,
    CMD_SPACE_FWD = 0x01,
    CMD_WRITE = 0x08,
    CMD_WRITE_FM = 0x0c,
};

static int decode_pertec_command (capture *c, list_t *channels)
{
    int irev, iwrt, iwfm, iedit, ierase;

    /* negative logic */
    irev = capture_bit_name (c, "irev", channels);
    iwrt = capture_bit_name (c, "iwrt", channels);
    iwfm = capture_bit_name (c, "iwfm", channels);
    iedit = capture_bit_name (c, "iedit", channels);
    ierase = capture_bit_name (c, "ierase", channels);

    return (irev << 4) | (iwrt << 3) |  (iwfm << 2) | (iedit << 1) | (ierase << 0);
}

static int decode_write_data (capture *c, list_t *channels)
{
    char name[4];
    int i;
    int retval = 0;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "iw%d", i);
	retval |= capture_bit_name (c, name, channels) << i;
    }

    return retval;
}

static int decode_read_data (capture *c, list_t *channels)
{
    char name[4];
    int i;
    int retval = 0;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "ir%d", i);
	retval |= capture_bit_name (c, name, channels) << i;
    }

    return retval;
}

static const char *pertec_command_name (int cmd)
{
    switch (cmd)
    {
	case CMD_READ_FWD: return "read_fwd";
	case CMD_SPACE_FWD: return "space_fwd";
	case CMD_WRITE: return "write";
	case CMD_WRITE_FM: return "write_fm";
	default:   return "unknown";
    }
}

static void dump_buffer (char *title, unsigned char *buffer, int len)
{
    int i;
    printf ("%s: %d\n", title, len);
    for (i = 0; i < len; i++)
    {
	if (i % 24 == 0)
	    printf ("\t%4.4x: ", i);
	printf ("%2.2x%s", buffer[i], i % 24 == 23 ? "\n" : " ");
    }
    printf ("\n");
}

struct pin_assignments
{
    int init;

    channel_info *igo;
    channel_info *irew;
    channel_info *iwstr;
    channel_info *irstr;
    channel_info *ilwd;
    channel_info *idby;
};

static void parse_pertec_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static unsigned char buffer[10240];
    static int buffer_pos = 0;
    static int last_word = 0;
    static int writing = 0;
    static int reading = 0;

    if (!prev) // skip first sample
	return;
    if (pa.init == -1 && c) // work these out once only, to speed things up
    {
	pa.init = 1;
	pa.igo = capture_channel_details (c, "igo", channels);
	pa.irew = capture_channel_details (c, "irew", channels);
	pa.iwstr = capture_channel_details (c, "iwstr", channels);
	pa.irstr = capture_channel_details (c, "irstr", channels);
	pa.ilwd = capture_channel_details (c, "ilwd", channels);
	pa.idby = capture_channel_details (c, "idby", channels);
    }

#warning "Should be looking at ITAD, IFAD to work out if this is for us or not"

    if (capture_bit_transition (c, prev, pa.igo, TRANSITION_falling_edge))
    {
	int cmd = decode_pertec_command (c, channels);
	printf ("igo: %x %s\n", cmd, pertec_command_name (cmd));
	buffer_pos = 0;
	last_word = 0;

	if (cmd == CMD_READ_FWD)
	    reading = 1;
	else if (cmd == CMD_WRITE)
	    writing = 1;
    }

    if (reading && writing)
    {
	printf ("ARGH! SOMEHOW I'M BOTH READING & WRITING\n");
    }

    if (capture_bit_transition (c, prev, pa.irew, TRANSITION_falling_edge))
    {
	printf ("rewind\n");
    }

    if (writing && capture_bit_transition (c, prev, pa.iwstr, TRANSITION_falling_edge))
    {
	buffer[buffer_pos++] = decode_write_data (c, channels);	

	if (last_word)
	{
	    dump_buffer ("Write", buffer, buffer_pos);
	    buffer_pos = 0;
	    last_word = 0;
	    writing = 0;
	}
    }

    if (reading && capture_bit_transition (c, prev, pa.irstr, TRANSITION_falling_edge))
    {
	buffer[buffer_pos++] = decode_read_data (c, channels);	

    }

    if (buffer_pos && capture_bit_transition (c, prev, pa.idby, TRANSITION_falling_edge))
    {
	dump_buffer ("Read", buffer, buffer_pos);
	buffer_pos = 0;
	last_word = 0;
	reading = 0;
    }

    if (capture_bit_transition (c, prev, pa.ilwd, TRANSITION_rising_edge))
	last_word = 1;

    /* Should have a check here that makes sure IDBY is never high when IFBY is low */

    /* Should have a check here that looks as how ILDP, IDENT & IREW all tie together 
     * (make sure that we do BOT handling properly */
}

static void parse_pertec_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c, *prev = NULL;

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_pertec_cap (c, prev, channels);
	prev = c;
	c++;
    }
}

void parse_pertec (list_t *cap, char *filename, list_t *channels)
{
    list_t *n;
    int i;

    printf ("Pertec analysis of file: '%s'\n", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_pertec_bulk_cap (n->data, channels);
    }
}
