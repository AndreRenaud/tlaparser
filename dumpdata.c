#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>

#include "dumpdata.h"

#define bit(d,b) ((d & (1 << b)) ? 1 : 0)
#define falling_edge(b) (bit(diff, b) && !bit(val, b))
#define rising_edge(b) (bit(diff, b) && bit(val, b))

static int dump_single_capture (capture *c, capture *prev_cap)
{
    int i;

    if (prev_cap && capture_time (c) < capture_time (prev_cap))
	printf ("ARGH WE WENT BACKWARDS\n");

    printf ("%llx: ", capture_time (c));
    for (i = 0; i < CAPTURE_DATA_BYTES; i++)
    {
         printf ("%2.2x ", c->data[i]);
    }
    printf ("\n");

    return 0;
}

uint64_t capture_time (capture *c)
{
    uint64_t t, b;
    t = ntohl (c->time_top) & 0xffff; // not sure what the story is here, packed data isn't all time stamp
    //t = 0;
    b = ntohl (c->time_bottom);

    // timings are in 1/8th of a nano-second, so change it into nano seconds
    return (t << 32 | b) / 8;
}

int dump_capture (bulk_capture *b)
{
   int i;
   capture *c, *prev = NULL;

   c = b->data;

   //printf ("Capture Group: %p\n", b);
   for (i = 0; i < b->length / sizeof (capture); i++)
   {
      dump_single_capture (c, prev);
      prev = c;
      c++;
   }

   return 0;
}

void dump_channel_list (list_t *channels)
{
    list_t *n;
    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	printf ("%s->%s Probe %d, index %d %s\n", c->probe_name, c->name, c->probe, c->index, c->inverted ? "Inverted" : "");
    }
}

void dump_capture_list (list_t *cap, char *name, list_t *channels)
{
    list_t *n;

    printf ("Capture '%s'\n", name);
    dump_channel_list (channels);

    for (n = cap; n != NULL; n = n->next)
    {
	int i;
	printf ("Capture item: %p\n", n);
	printf ("time       ");
	for (i = 0; i < CAPTURE_DATA_BYTES; i++)
	    printf ("%2.2d ", i);
	printf ("\n");
	dump_capture (n->data);
    }
}

void dump_changing_channels (list_t *cap, char *name, list_t *channels)
{
    list_t *n;
    uint8_t changes[CAPTURE_DATA_BYTES];
    int b;
    for (b = 0; b < CAPTURE_DATA_BYTES; b++)
	changes[b] = 0;

    for (n = cap; n != NULL; n = n->next)
    {
	int i;
	bulk_capture *bc = n->data;
	capture *c, *prev = NULL;

	c = bc->data;

	//printf ("Capture Group: %p\n", b);
	for (i = 0; i < bc->length / sizeof (capture); i++)
	{
	    if (prev)
	    {
		for (b = 0; b < CAPTURE_DATA_BYTES; b++)
		{
		    changes[b] |= c->data[b] ^ prev->data[b];
		}
	    }

	    prev = c;
	    c++;
	}

    }
    printf ("Changed bits on '%s'\n", name);
    for (b = 0; b < CAPTURE_DATA_BYTES; b++)
	printf ("0x%2.2x ", changes[b]);
    printf ("\n");

}

int capture_bit (capture *cap, channel_info *c)
{
    int retval;
    if (!c || c->probe < 0 || c->probe >= CAPTURE_DATA_BYTES || c->index < 0 || c->index >= 8)
    {
	printf ("Invalid probe: %d index: %d\n",
		c ? c->probe : -1 , c ? c->index : -1);
	abort ();
    }
    retval = (cap->data[c->probe] & (1 << c->index));
    if (c->inverted)
	retval = !retval;
    return retval ? 1 : 0;
}

channel_info *capture_channel_details (capture *cap, char *channel_name, list_t *channels)
{
    list_t *n;

    for (n = channels; n!= NULL; n = n->next)
    {
	channel_info *c = n->data;
	if (strcasecmp (c->name, channel_name) == 0)
	{
	    return c;
	}
    }

    printf ("Unknown channel: %s\n", channel_name);
    return NULL;
}

int capture_bit_name (capture *cap, char *channel_name, list_t *channels)
{
    channel_info *chan;

    chan = capture_channel_details (cap, channel_name, channels);
    if (chan)
	return capture_bit (cap, chan);
    return -1;
}

int capture_bit_transition (capture *cur, capture *prev, channel_info *chan, int dir)
{
    int n, p;
    if (!chan)
	return -1;
    p = capture_bit (prev, chan);
    if (p < 0)
	return -1;
    n = capture_bit (cur, chan);
    if (n < 0)
	return -1;

    if (n && !p && dir == TRANSITION_low_to_high)
	return 1;
    if (!n && p && dir == TRANSITION_high_to_low)
	return 1;

    return 0;
}

int capture_bit_transition_name (capture *cur, capture *prev, char *name, list_t *channels, int dir)
{
    channel_info *chan;
    chan = capture_channel_details (cur, name, channels);

    return capture_bit_transition (cur, prev, chan, dir);
}

/* Works out where in the binary blob the channel info is found
 * Channels are ordered as specified in the file (So the order we call build_channel matters)
 * Probes are also paired, so e3 & e2 are always together, so are a1 & a0. If a1 is entirely unused,
 * it will not appear in the tla file, but we must still account for it (that is the bit below looking
 * for 3 & 1, checking that we haven't skipped a probe
 * This is all vaguely confusing
 */
channel_info *build_channel (char *probe_name, char *name, int inverted)
{
    channel_info *retval;
    // they seem to have leading & trailing $ signs on them, so chop them off if they're there
    retval = malloc (sizeof (channel_info));

    strncpy (retval->probe_name, probe_name, 20);
    retval->probe_name[19] = '\0';

    retval->inverted = inverted;

    if (name[0] == '$' && name[strlen(name) - 1] == '$')
    {
	strncpy (retval->name, &name[1], strlen(name) - 2);
	retval->name[strlen(name) - 2] = '\0';
    }
    else
	strncpy (retval->name, name, 20);
    retval->name[19] = '\0';

    retval->index = atoi (&probe_name[3]);

#if 0
#warning "Using vaguelly magic packed probe assignment - almost definitely wrong"
    static int probe = -1; // so that we ++ to 0
    static char last_probe[10] = "\0";

    if (strncasecmp (probe_name, last_probe, 2) != 0)
    {
	if (probe != -1 && last_probe[0] != probe_name[0] && probe_name[1] != '3' && probe_name[1] != '1') // we've dropped half of a probe pair
	    probe++;
	strcpy (last_probe, probe_name);

	probe++;

    }
    retval->probe = probe;
#endif

#if 0
#warning "Using guessed packing method" // this is wrong
/* Probes are packed a3 a2 c3 c2 d3 d2 e3 e2 a1 a0 c1 c0 d1 d0 e1 e0 ?? */ 
    retval->probe = (retval->probe_name[0] - 'A') * 2;
    int probe_index = atoi (&retval->probe_name[1]);
    if (probe_index <= 1)
	retval->probe += 8;
    retval->probe += 1 - (probe_index % 2);
#endif

#if 1
#warning "Overriding channel probes" // for SCSI
    if (strncmp (retval->probe_name, "A2", 2) == 0)
	retval->probe = 2;
    else if (strncmp (retval->probe_name, "A1", 2) == 0)
	retval->probe = 8;
    else if (strncmp (retval->probe_name, "A0", 2) == 0)
	retval->probe = 9;
#endif

#if 0
#warning "Overriding channel probes for 8250"
    if (strncmp (retval->probe_name, "A3", 2) == 0)
	retval->probe = 1;
    else if (strncmp (retval->probe_name, "A2", 2) == 0)
	retval->probe = 2;
    else if (strncmp (retval->probe_name, "D2", 2) == 0)
	retval->probe = 7;
    else if (strncmp (retval->probe_name, "D3", 2) == 0)
	retval->probe = 3;
#endif

#if 0
    printf ("Added channel %s %s - (%d, %d) %s\n",
	    retval->probe_name, retval->name, retval->probe, retval->index, retval->inverted ? "Inverted" : "");
#endif

    return retval;
}

bulk_capture *build_dump (void *data, int length)
{
   bulk_capture *retval;
   capture *c;
   int i;
   int nrecords = length / sizeof (capture);

   retval = malloc (sizeof (bulk_capture));
   retval->data = malloc (length);

   //data += sizeof (capture * 16); // skip the header crap

    /* Attempt to convert the endian-ness of them? */
    for (i = 0; i < nrecords; i++)
    {
	int v;
	c = (capture *)(((char *)data) + i * sizeof (capture));
	for (v = 0; v < CAPTURE_DATA_BYTES / 4; v++)
	{
	    uint32_t *val;
	    val = (uint32_t *)&c->data[v];
	    *val = htonl (*val);
	}

	memcpy (&retval->data[i], c, sizeof (capture));
    }

   //memcpy (retval->data, data, length);

   retval->length = length;

   //printf ("build_dump: %p, %d -> %p, %d\n", data, length, retval->data, retval->length);

   return retval;
}


int time_log (capture *c, char *msg, ...)
{
    char buffer[1024];
    va_list ap;
    //uint64_t time_now = capture_time (c); // we want it in nano-seconds
    uint64_t time_now = capture_time (c) / 1000; // we want it in useconds
    static uint64_t last_time = -1;

    va_start (ap, msg);
    vsnprintf (buffer, 1024, msg, ap);
    va_end (ap);
    buffer[1023] = '\0';

    printf ("[%10.10lld] ", time_now);
    if (last_time != -1)
	printf ("[%8.8lld] ", time_now - last_time);
    else
	printf ("[None    ] ");
    last_time = time_now;

    printf ("%s", buffer);

    return 0;
}

