#include <stdio.h>

#include "dumpdata.h"
#include "common.h"
#include "pertec.h"

static int decode_pertec_command (capture *c, list_t *channels)
{
    int irev, iwrt, iwfm, iedit, ierase;

    /* negative logic */
    irev = capture_bit_name (c, "irev", channels) ? 0 : 1;
    iwrt = capture_bit_name (c, "iwrt", channels) ? 0 : 1;
    iwfm = capture_bit_name (c, "iwfm", channels) ? 0 : 1;
    iedit = capture_bit_name (c, "iedit", channels) ? 0 : 1;
    ierase = capture_bit_name (c, "ierase", channels) ? 0 : 1;

    return (irev << 4) | (iwrt << 3) |  (iwfm << 2) | (iedit << 1) | (ierase << 0);
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
struct pin_assignments
{
    int init;

    pin_info_t igo;
    pin_info_t irew;
};

static void parse_pertec_cap (capture *c, capture *prev, list_t *channels)
{
    static struct pin_assignments pa = {-1};
    static int prev_irew = 0;

    if (pa.init == -1 && c)
    {
	pa.init = 1;
	capture_channel_details (c, "igo", &pa.igo);
	capture_channel_details (c, "irew", &pa.irew);
    }

    if (!prev) // skip first sample
	return;

    if (!capture_bit (prev, pa.igo) &&
	capture_bit (c, pa.igo))
    {
	int cmd = decode_pertec_command (c, channels);
	printf ("igo: %x %s\n", cmd, pertec_command_name (cmd));
    }

    if (!capture_bit (prev, pa.irew) &&
	capture_bit (c, pa.irew))
    {
	printf ("rewind\n");
    }

    if (prev_irew != capture_bit (c, pa.irew))
    {
	printf ("irew: %d\n", capture_bit (c, pa.irew));
	prev_irew = capture_bit (c, pa.irew);
    }

}

static void parse_pertec_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c, *prev = NULL;

    c = (capture *)(b+1);//(char *)b+sizeof (bulk_capture);

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

    printf ("Pertec analysis of file: '%s'", filename);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_pertec_bulk_cap (n->data, channels);
    }

}
