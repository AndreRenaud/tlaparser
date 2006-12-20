#include <stdio.h>

#include "dumpdata.h"
#include "common.h"
#include "pertec.h"


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

static const char *pertec_command_name (int cmd)
{
    switch (cmd)
    {
	case 0x00: return "read_fwd";
	case 0x01: return "space_fwd";
	case 0x08: return "write";
	case 0x0c: return "write_fm";
	default:   return "unknown";
    }
}

static void dump_buffer (unsigned char *buffer, int len)
{
    int i;
    printf ("buffer: %d\n", len);
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
    channel_info *ilwd;
};

static void parse_pertec_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static unsigned char buffer[10240];
    static int buffer_pos = 0;
    static int last_word = 0;

    if (!prev) // skip first sample
	return;
    if (pa.init == -1 && c) // work these out once only, to speed things up
    {
	pa.init = 1;
	pa.igo = capture_channel_details (c, "igo", channels);
	pa.irew = capture_channel_details (c, "irew", channels);
	pa.iwstr = capture_channel_details (c, "iwstr", channels);
	pa.ilwd = capture_channel_details (c, "ilwd", channels);
    }

    /* falling edge */
    if (!capture_bit (prev, pa.igo) &&
	capture_bit (c, pa.igo))
    {
	int cmd = decode_pertec_command (c, channels);
	printf ("igo: %x %s\n", cmd, pertec_command_name (cmd));
	buffer_pos = 0;
	last_word = 0;
    }

    /* falling edge */
    if (capture_bit (prev, pa.irew) &&
	!capture_bit (c, pa.irew))
    {
	printf ("rewind\n");
    }

    if (capture_bit (prev, pa.iwstr) &&
	!capture_bit (c, pa.iwstr))
    {
	buffer[buffer_pos++] = decode_write_data (c, channels);	

	if (last_word)
	{
	    dump_buffer (buffer, buffer_pos);
	    buffer_pos = 0;
	    last_word = 0;
	}
    }

    if (!capture_bit (prev, pa.ilwd) &&
	capture_bit (c, pa.ilwd))
	last_word = 1;

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
