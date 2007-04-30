#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "dumpdata.h"
#include "common.h"

static int pertec_id = 0; /* Which pertec id (combination of IFAD << 2 | ITAD0 << 1 | ITAD1 << 0) are we? */
static int ignore_parity = 0; /* Do we ignore parity errors? */
static int ignore_id = 0; /* Do we ignore ID selection? */

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

static int parity (int val)
{
    int par = 0;
    int i;

    for (i = 0; i < 8; i++)
	par = par ^ (val & (1 << i) ? 0 : 1);

    return par;
}

static int decode_write_data (capture *c, list_t *channels)
{
    char name[4];
    int i;
    int retval = 0;
    int bit;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "iw%d", i);
	bit = capture_bit_name (c, name, channels);
	retval |= bit << i;
    }

    if (!ignore_parity)
    {
	bit = capture_bit_name (c, "iwp", channels) ? 1 : 0;
	if (parity (retval) != bit)
	    time_log (c, "Parity error on write data: 0x%x (par = %d, not %d)\n", retval, bit, parity (retval));
    }

    return retval;
}

static int decode_read_data (capture *c, list_t *channels)
{
    char name[4];
    int i;
    int retval = 0;
    int bit;

    for (i = 0; i < 8; i++)
    {
	sprintf (name, "ir%d", i);
	bit = capture_bit_name (c, name, channels) ? 1 : 0;
	retval |= bit << i;
    }

    if (!ignore_parity)
    {
	bit = capture_bit_name (c, "irp", channels) ? 0 : 1;
	if (parity (retval) != bit)
	    time_log (c, "Parity error on read data: 0x%x (par = %d, not %d)\n", retval, bit, parity (retval));
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

#if 0
static void dump_buffer (char *title, unsigned char *buffer, int len)
{
    int i;
    printf ("%s: %d\n", title, len);
    for (i = 0; i < len; i++)
    {
	if (i % 24 == 0)
	    printf ("  %4.4x: ", i);
	printf ("%2.2x%s", buffer[i], i % 24 == 23 ? "\n" : " ");
    }
    printf ("\n");
}
#endif

struct pin_assignments
{
    int init;

    channel_info *igo;
    channel_info *irew;
    channel_info *iwstr;
    channel_info *irstr;
    channel_info *ilwd;
    channel_info *idby;
    channel_info *ifby;
    channel_info *ifad;
    channel_info *itad0;
    channel_info *itad1;
    channel_info *ident;
    channel_info *ifmk;
    channel_info *ildp;
};

static void parse_pertec_cap (capture *c, list_t *channels)
{
    static uint64_t first_good = 0;
    static int prev_bad = 0;
    static capture *prev = NULL;
    static struct pin_assignments pa = {-1};
    static unsigned char buffer[10240];
    static int buffer_pos = 0;
    static int last_word = 0;
    static int writing = 0;
    static int reading = 0;

    if (pa.init == -1 && c) // work these out once only, to speed things up
    {
	pa.init = 1;
	pa.igo = capture_channel_details (c, "igo", channels);
	pa.irew = capture_channel_details (c, "irew", channels);
	pa.iwstr = capture_channel_details (c, "iwstr", channels);
	pa.irstr = capture_channel_details (c, "irstr", channels);
	pa.ilwd = capture_channel_details (c, "ilwd", channels);
	pa.idby = capture_channel_details (c, "idby", channels);
	pa.ifby = capture_channel_details (c, "ifby", channels);
	pa.ifad = capture_channel_details (c, "ifad", channels);
	pa.itad0 = capture_channel_details (c, "itad<0>", channels);
	pa.itad1 = capture_channel_details (c, "itad<1>", channels);
	pa.ident = capture_channel_details (c, "ident", channels);
	pa.ifmk = capture_channel_details (c, "ifmk", channels);
	pa.ildp = capture_channel_details (c, "ildp", channels);
    }

    if (!prev) // skip first sample
    {
	prev = c;
	return;
    }

    if (!ignore_id)
    {
	/* Ignore any transitions that aren't for us */
	int id = capture_bit (c, pa.ifad) << 2 | capture_bit (c, pa.itad0) << 1 | capture_bit (c, pa.itad1);

	if (id != pertec_id) // we don't want ones that aren't for us
	{
	    prev_bad = 1;
	    return;
	}
    }

    if (prev_bad || first_good == 0)
    {
	prev_bad = 0;
	first_good = capture_time (c);
	return;
    }
    else if (capture_time(c) - first_good < 60 * 1000) // we also want to ignore any samples for 150ns after we're selected, to allow them to wobble
	return;

    if (capture_bit (c, pa.idby) != capture_bit (prev, pa.idby))
	time_log (c, "idby %sactive\n", capture_bit (c,pa.idby) ? "": "in");

    if (capture_bit (c, pa.ifby) != capture_bit (prev, pa.ifby))
	time_log (c, "ifby %sactive\n", capture_bit (c,pa.ifby) ? "": "in");

    if (capture_bit (c, pa.ident) != capture_bit (prev, pa.ident))
	time_log (c, "ident %sactive\n", capture_bit (c,pa.ident) ? "": "in");

    if (capture_bit (c, pa.ildp) != capture_bit (prev, pa.ildp))
	time_log (c, "ildp %sactive\n", capture_bit (c,pa.ildp) ? "": "in");

    if (capture_bit (c, pa.idby)) /* ifmk only valid when idby is active */
    {
	if (capture_bit (c, pa.ifmk) != capture_bit (prev, pa.ifmk))
	    time_log (c, "ifmk %sactive\n", capture_bit (c,pa.ifmk) ? "": "in");
    }

    if (capture_bit_transition (c, prev, pa.igo, TRANSITION_rising_edge))
    {
	int cmd = decode_pertec_command (c, channels);
	time_log (c, "igo: %x %s\n", cmd, pertec_command_name (cmd));
	buffer_pos = 0;
	last_word = 0;

	if (reading || writing)
	    time_log (c, "igo while outstanding read/write");


	if (cmd == CMD_READ_FWD)
	    reading = 1;
	else if (cmd == CMD_WRITE)
	    writing = 1;
    }

    if (reading && writing)
    {
	time_log (c, "ARGH! SOMEHOW I'M BOTH READING & WRITING\n");
    }

    if (capture_bit_transition (c, prev, pa.irew, TRANSITION_falling_edge))
    {
	time_log (c, "rewind\n");
    }

    if (writing && capture_bit_transition (c, prev, pa.iwstr, TRANSITION_falling_edge))
    {
	if (buffer_pos == 0)
	    time_log (c, "First write byte\n");
	buffer[buffer_pos++] = decode_write_data (c, channels);

	if (last_word)
	{
	    display_data_buffer (buffer, buffer_pos, 0);
	    //dump_buffer ("Write", buffer, buffer_pos);
	    buffer_pos = 0;
	    last_word = 0;
	    writing = 0;
	}
    }

    if (reading && capture_bit_transition (c, prev, pa.irstr, TRANSITION_falling_edge))
    {
	if (buffer_pos == 0)
	    time_log (c, "First read byte\n");
	buffer[buffer_pos++] = decode_read_data (c, channels);
    }

    if (buffer_pos && capture_bit_transition (c, prev, pa.idby, TRANSITION_falling_edge))
    {
	display_data_buffer (buffer, buffer_pos, 0);
	//dump_buffer ("Read", buffer, buffer_pos);
	buffer_pos = 0;
	last_word = 0;
	reading = 0;
    }

    if (capture_bit_transition (c, prev, pa.ilwd, TRANSITION_rising_edge) && writing)
    {
	time_log (c, "Last word\n");
	last_word = 1;
    }

    if (capture_bit (c, pa.idby) && !capture_bit (c, pa.ifby))
	time_log (c, "IDBY high, but IFBY low\n");

    /* Should have a check here that looks as how ILDP, IDENT & IREW all tie together
     * (make sure that we do BOT handling properly */
    prev = c;
}

static void parse_pertec_bulk_cap (bulk_capture *b, list_t *channels)
{
    int i;
    capture *c;

    c = b->data;

    for (i = 0; i < b->length / sizeof (capture); i++)
    {
	parse_pertec_cap (c, channels);
	c++;
    }
}

void parse_pertec (list_t *cap, char *filename, list_t *channels)
{
    list_t *n;
    int i;
    char idbuf[100];

    if (option_val ("pertecid", idbuf, 100))
	pertec_id = atoi(idbuf);
    ignore_id =  option_set ("ignoreid");
    ignore_parity = option_set ("ignore_parity");

    printf ("Pertec analysis of file: '%s', using ID %d\n", filename, pertec_id);

    for (n = cap, i = 0; n != NULL; n = n->next, i++)
    {
	printf ("Parsing capture block %d\n", i);
	parse_pertec_bulk_cap (n->data, channels);
    }
}
